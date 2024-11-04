/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "CardProcess.h"
#include <phApp_Init.h>
#include "Porting_SAMD21.h"
#include <phNfcLib.h>
#include "Wiegand_out.h"
#include "tk_output.h"
#include <string.h>

#include <asf.h>    // for NVIC_SystemReset()

#define _min_(a, b)((a<b)?(a):(b))

#define CHECK_CARD_EXIST_COUNT				5
#define READ_CARD_AUTH_KEY_RETRY			3
#define READ_CARD_NO_TAG_RETRY				2

// Mifare Card Information
#define MF_SMALL_SECTOR_BLOCKS				4
#define MF_SMALL_SECTOR_DATA_BLOCKS			(MF_SMALL_SECTOR_BLOCKS-1)
#define MF_BIG_SECTOR_BLOCKS				16
#define MF_BIG_SECTOR_DATA_BLOCKS			(MF_BIG_SECTOR_BLOCKS-1)
#define MF_BIG_SECTOR_START					32
#define MF_1K_NUMBER_OF_SECTOR				16
#define MF_1K_MAX_SECTOR					(MF_1K_NUMBER_OF_SECTOR-1)
#define MF_4K_NUMBER_OF_SECTOR				40
#define MF_4K_MAX_SECTOR					(MF_4K_NUMBER_OF_SECTOR-1)

#define MF_4K_ALL_SMALL_SECTOR_BLOCKS		(MF_SMALL_SECTOR_BLOCKS*MF_BIG_SECTOR_START)
#define MF_4K_ALL_SMALL_SECTOR_DATA_BLOCKS	(MF_SMALL_SECTOR_DATA_BLOCKS*MF_BIG_SECTOR_START)
#define MF_4K_ALL_SECTOR_BLOCKS				(MF_4K_ALL_SMALL_SECTOR_BLOCKS+(MF_BIG_SECTOR_BLOCKS*(MF_4K_NUMBER_OF_SECTOR-MF_BIG_SECTOR_START)))

#define WG_INP_FMT_MASK 				((1<<3) | (1<<4))	 
#define WG_INP_FMT_DEC_STR 				(1<<3) 	//0x08

#define No_Disable_Enable_UID 	0

#define OES_AID1	0x489B
#define OES_AID2	0x9B48
bool bIsOES;

BlockCacheBuffer_t m_tBlockCacheBuffer;

u8 MAD_KEY[6]={0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
bool bMAD,bMAD2;
UINT_T m_AID[8];
uint8_t m_bSdlen;
uint8_t m_bAuthSector;
uint8_t m_bATQA[2];
BYTE m_bTmpbuf[MAX_USER_BUF];

bool bAutoMode = true;

extern phacDiscLoop_Sw_DataParams_t       * pDiscLoop;
extern void *psalMFC;

void BIN2STR(u8* bBIN, u8 bSize, u8* bString, u16 bSTR_LEN);
uint DataLineBlockToLineBlock(uint uBlock);
uint LineBlockNextDataBlock(uint uBlock);
BYTE DecodeData(BYTE dat, BYTE i);
uint Mifare_SearchType(BlockCacheBuffer_t* ptBlockCacheBuffer, BYTE nSector, u8 nType);
BYTE ReadLineBlock(BYTE bBlock, BYTE* pbBuffer);


// ************************************************************************
// Current Card
// ************************************************************************
void BIN2STR(u8* bBIN, u8 bSize, u8* bString, u16 bSTR_LEN)
{
	uint8_t i;	
    memset(bString,0,bSTR_LEN);    
    
    for(i=0; i<bSize; i++)
    {
        if((bBIN[i]>>4)>=0x0A)
        {
            bString[i*2] = ((bBIN[i]>>4)-0x0A)+'A';
        }
        else
        {
            bString[i*2] = (bBIN[i]>>4)|'0';
        }
            
        if((bBIN[i] & 0x0F)>=0x0A)
        {
            bString[(i*2)+1] = ((bBIN[i] & 0x0F)-0x0A)+'A';
        }
        else
        {
            bString[(i*2)+1] = (bBIN[i] & 0x0F)|'0';
        }
    }
}

void SwapValue(void *val, u8 size)
{
	u8 tmp,i;
	u8 *p = (u8 *) val;

	for(i=0; i<(size/2); i++)
	{
		tmp = p[i];
		p[i] = p[size-i-1];
		p[size-i-1] = tmp;
	}
}

u16 MIFARE_READ_WRITE(bool Select, u8 bSector, u8 bBlocks, u8 bOffset, u8 bLength, u8* bWriteBuf, u8* bOutBuf, u8 bOutBufSize)
{
        phStatus_t status;
        u8 bBuffer[16];
    
        status = ERR_MIFARE_FORMAT;
    
        memset(bBuffer, 0x00, sizeof(bBuffer));
        memset(bOutBuf, 0x00, bOutBufSize);
    
        if(bSector<MIFARE_SECTOR_SIZE)
        {
            bBlocks = (bSector*4)+bBlocks;
            
            if(Select)
            {
                if((bOffset+bLength)<(MIFARE_BLOCK_SIZE+1))
                {
                    if(bLength == 0)
                        bLength = MIFARE_BLOCK_SIZE - bOffset;
                    
                    if(bLength<=bOutBufSize)
                    {
                            status = phalMfc_Read(psalMFC, bBlocks, bBuffer);

                            if(PH_ERR_SUCCESS == status)
                            {
                                    memcpy(bOutBuf, bBuffer+bOffset, bLength);
                            }
                    }
                }
            }
            else
            {
                status = phalMfc_Write(psalMFC, bBlocks, bWriteBuf);
            }
        }
                
        return status;
}

// ************************************************************************
// ReadLineBlock
// ************************************************************************
#ifndef _KEY_B_
#ifdef	_CUSTOMER_SAT_ 	// 2008/11/03
BYTE code m_bNewCardKey[]={0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif //#ifdef	_CUSTOMER_SAT_ 	// 2008/11/03
#endif //#ifndef _KEY_B_
BYTE ReadLineBlock(BYTE bBlock, BYTE* pbBuffer)
{
	BYTE bLastStatus;
	BYTE bSector;
	BYTE bNoTagReTry;
	BYTE bTry;
    BYTE bKeyType;
	
    bKeyType = (g_tRegisters.s1.bEnableUID & 0x80) ? PHHAL_HW_MFC_KEYB : PHHAL_HW_MFC_KEYA;

	if(bBlock<MF_4K_ALL_SMALL_SECTOR_BLOCKS)
	{
		bSector=(bBlock>>2);		// (bBlock>>2)=>(iBlock/MF_SMALL_SECTOR_BLOCKS);
		bBlock=(bBlock & 0x03);
	}
	else
	{
		bBlock-=MF_4K_ALL_SMALL_SECTOR_BLOCKS;
		bSector=(bBlock>>4);		// (bBlock>>4)=>(iBlock/MF_BIG_SECTOR_BLOCKS);
		bSector+=MF_BIG_SECTOR_START;
		bBlock=(bBlock & 0x0F);
	}
	bLastStatus=MI_OK;
	if(bBlock==0)
		bLastStatus=MI_KEYERR;
	bNoTagReTry=0;
	for(bTry=0; bTry<3; bTry++)
	{
		if(bLastStatus!=MI_OK)
		{
            bLastStatus = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, NULL, NULL, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);
            if(bLastStatus==MI_OK)
            {
                bLastStatus = phalMfc_Authenticate(psalMFC, (bSector*4), bKeyType, STORE_KEYNUM_MFC, 0, bUid, bUidSize);
            }
        }
		if(bLastStatus==MI_OK)
		{
            bLastStatus=MF_Read(bSector, bBlock, 0, 0, (u8*)pbBuffer, MIFARE_BLOCK_SIZE);
			if(bLastStatus==MI_OK)
				break;
			if(bLastStatus!=MI_NOTAGERR)
				bLastStatus=MI_KEYERR;
		}
		if(bLastStatus==MI_NOTAGERR)
		{
			bNoTagReTry++;
			if(bNoTagReTry>=READ_CARD_NO_TAG_RETRY)
				break;
		}
	}
	return bLastStatus;
}

// ************************************************************************
// SectorToByteOffset
// ************************************************************************
uint SectorToByteOffset(BYTE bSector, bool bIsDataBlockOnly)
{
	uint uBlocks;
	if(bSector<MF_BIG_SECTOR_START)
	{
		if(bIsDataBlockOnly)
			uBlocks=(MF_SMALL_SECTOR_DATA_BLOCKS*bSector);
		else
			uBlocks=(MF_SMALL_SECTOR_BLOCKS*bSector);
	}
	else
	{
		bSector-=MF_BIG_SECTOR_START;
		if(bIsDataBlockOnly)
		{
			uBlocks=(MF_BIG_SECTOR_DATA_BLOCKS*bSector);
			uBlocks+=MF_4K_ALL_SMALL_SECTOR_DATA_BLOCKS;
		}
		else
		{
			uBlocks=(MF_BIG_SECTOR_BLOCKS*bSector);
			uBlocks+=MF_4K_ALL_SMALL_SECTOR_BLOCKS;
		}
	}
	return (MIFARE_BLOCK_SIZE*uBlocks);
}

// ************************************************************************
// BlockCacheBufferInfoInit
// ************************************************************************
void BlockCacheBufferInfoInit(BlockCacheBuffer_t* ptBlockCacheBuffer)
{
	if(ptBlockCacheBuffer!=NULL)
	{
		memset(ptBlockCacheBuffer, 0x00, sizeof(BlockCacheBuffer_t));
		ptBlockCacheBuffer->uLineBlockNumber=LINE_BLOCK_NUMBER_INVALID;
	}
}

// ************************************************************************
// DataLineBlockToLineBlock
// ************************************************************************
uint DataLineBlockToLineBlock(uint uBlock)
{
	if(uBlock>=MF_4K_ALL_SMALL_SECTOR_DATA_BLOCKS)
	{
		uBlock-=MF_4K_ALL_SMALL_SECTOR_DATA_BLOCKS;
		uBlock+=(uBlock/MF_BIG_SECTOR_DATA_BLOCKS);
		uBlock+=MF_4K_ALL_SMALL_SECTOR_BLOCKS;
	}
	else
		uBlock+=(uBlock/MF_SMALL_SECTOR_DATA_BLOCKS);
	return uBlock;
}

// ************************************************************************
// LineBlockNextDataBlock
// ************************************************************************
uint LineBlockNextDataBlock(uint uBlock)
{
	uBlock++;
	if(uBlock>=MF_4K_ALL_SMALL_SECTOR_BLOCKS)
	{
		if((uBlock & 0x000F)==0x000F)
			uBlock++;
	}
	else
	{
		if((uBlock & 0x0003)==0x0003)
			uBlock++;
	}
	return uBlock;
}

// ************************************************************************
// ReadLineBlockBytes
// ************************************************************************
BYTE ReadLineBlockBytes(BlockCacheBuffer_t* ptBlockCacheBuffer, uint uOffset, BYTE* pbBuffer, BYTE bLength)
{
	BYTE bLastStatus=MI_EMPTY;
	if(ptBlockCacheBuffer!=NULL)
	{
		uint uLineBlock;
		BYTE bLen;
		bool bIsDataBlockOnly;
		bIsDataBlockOnly=0;
		if(uOffset & LINE_BLOCK_DATA_BLOCK_ONLY_FLAG)
		{
			bIsDataBlockOnly=1;
			uOffset=(uOffset & (~LINE_BLOCK_DATA_BLOCK_ONLY_FLAG));
		}
		uLineBlock=(uOffset>>4);
		if(bIsDataBlockOnly)
			uLineBlock=DataLineBlockToLineBlock(uLineBlock);
		uOffset=(uOffset & 0x000F);
		while(bLength>0)
		{
			if(uLineBlock>=MF_4K_ALL_SECTOR_BLOCKS)
			{
				bLastStatus=MI_OVFLERR;
				break;
			}
			bLastStatus=MI_OK;
			if(ptBlockCacheBuffer->uLineBlockNumber!=uLineBlock)
			{
				BlockCacheBufferInfoInit(ptBlockCacheBuffer);
				bLastStatus=ReadLineBlock(((BYTE)uLineBlock), ptBlockCacheBuffer->bBuffer);
				if(bLastStatus!=MI_OK)
					break;
				ptBlockCacheBuffer->uLineBlockNumber=uLineBlock;
			}
			bLen=(MIFARE_BLOCK_SIZE-uOffset);
			if(bLen>bLength)
				bLen=bLength;
			memcpy(pbBuffer, &ptBlockCacheBuffer->bBuffer[uOffset], bLen);
			pbBuffer+=bLen;
			bLength-=bLen;
			uOffset+=bLen;
			if(uOffset>=MIFARE_BLOCK_SIZE)
			{
				uOffset=0;
				if(bIsDataBlockOnly)
					uLineBlock=LineBlockNextDataBlock(uLineBlock);
				else
					uLineBlock++;
			}
		}
	}
	return bLastStatus;
}

BYTE DecodeData(BYTE dat, BYTE i)
{
	BYTE EnCode[]={0x00, 0x2D, 0x5F, 0x7B, 0xA7, 0x98};

	if(g_tRegisters.s1.bEncryptValue<sizeof(EnCode))
		dat = dat ^ bUid[i%4] ^ EnCode[g_tRegisters.s1.bEncryptValue];

	return dat;
}

bool bAID_CFG;	// 2007/07/04
bool Mifare_GetLineByte(BlockCacheBuffer_t* ptBlockCacheBuffer, BYTE StartSector, uint index, BYTE* dat)  // 2008/7/15, new KeyType option
{
	BYTE bLastStatus;
	uint uOffset;
	
	uOffset=(LINE_BLOCK_DATA_BLOCK_ONLY_FLAG | (SectorToByteOffset(StartSector, 1)+index));
	bLastStatus=ReadLineBlockBytes(ptBlockCacheBuffer, uOffset, dat, 1);
	if(bLastStatus==MI_OK)
	{
		if(index>0 && g_tRegisters.s1.bEncryptValue>0 && !bAID_CFG)	// Encrypt & Decode , 2005/02/24, 2008/06/06 CFG don't decode
			 *dat = DecodeData((*dat), (index-1));
		return 1;
	}
	return 0;
}

bool Mifare_Memcpy(BlockCacheBuffer_t* ptBlockCacheBuffer, BYTE* dest, BYTE nSector, uint index, u8 len)
{
	u8 i;
	BYTE dat;
	for (i=0; i<len; i++)
	{
		if ( Mifare_GetLineByte(ptBlockCacheBuffer,nSector, index+i, &dat) )
		{
			dest[i] = dat;
		}
		else return 0;
	}
	return 1;
}

BYTE Mifare_GetAIDSector(void)
{
	u8 bBlockBuffer[16];
	BYTE blk, sec, Retry;
	BYTE bLastStatus;
	
	bMAD = 0;
	bMAD2= 0;
	bAID_CFG = 0;
	bIsOES = 0;

    Is_Ours_Mfc_Setkey(MAD_KEY, PH_KEYSTORE_KEY_TYPE_MIFARE, STORE_KEYNUM_MFC_MAD, PHHAL_HW_MFC_ALL);

	if (g_tRegisters.s1.wAID.wInt == 0 )
	{
		return g_tRegisters.s1.bSector;
	}
	// ******************************
	// Check MAD Card
	// ******************************
	Retry = 0;
	bLastStatus = MI_OK;
	do
	{
		if(bLastStatus!=MI_OK)
			bLastStatus = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, NULL, NULL, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);

        bLastStatus = phalMfc_Authenticate(psalMFC, 0x00, PHHAL_HW_MFC_KEYA, STORE_KEYNUM_MFC_MAD, 0, bUid, bUidSize);
		if(bLastStatus==MI_OK)
		{
            bLastStatus = MF_Read(0, 3, 0, 0, bBlockBuffer, sizeof(bBlockBuffer));
			if ( bLastStatus==MI_OK && (bBlockBuffer[9] & 0x80) )  // check DA (MAD Available) GPB
			{
				bMAD2= (bBlockBuffer[9] & 0x02) ? 1 : 0;	// check MAD2 version
				bMAD = 1;
			}
		}
		Retry++;
	}while(bLastStatus!=MI_OK && Retry<3);
	    
	// ******************************
	// Search AID from MAD 
	// ******************************
	if ( bMAD )
	{	// Read & Check MAD1 Block
		for (blk=1; blk<3; blk++)
		{
            bLastStatus = MF_Read(0, blk, 0, 0, (u8*)&m_AID, sizeof(m_AID));
			if(bLastStatus==MI_OK)
			{
				for (sec=0; sec<8; sec++)
				{
					if ( (blk>0 || sec>0) )	// 2007/07/04
					{
						if ( m_AID[sec].wInt==g_tRegisters.s1.wAID.wInt )
						{
							return ((blk-1)*8+sec);
						}
						if ( m_AID[sec].wInt==0x4839 )	// 2007/07/04,support Configure Card (AID=4839H)
						{
							bAID_CFG = 1;
							return ((blk-1)*8+sec);
						}
						if ( m_AID[sec].wInt==OES_AID1 || m_AID[sec].wInt==OES_AID2 )	// 2008/8/18, Support OES card
						{
							bIsOES = 1;
							return ((blk-1)*8+sec);
						}
					}
				}
			}
		}
        
		if ( bMAD2 )
		{	// Read & Check MAD2 Block
            bLastStatus = phalMfc_Authenticate(psalMFC, 0x10, PHHAL_HW_MFC_KEYA, STORE_KEYNUM_MFC_MAD, 0, bUid, bUidSize);
			if(bLastStatus==MI_OK)
			{
				for (blk=0; blk<3; blk++)
				{
                    bLastStatus = MF_Read(0x10, blk, 0, 0, (u8*)&m_AID, sizeof(m_AID));
					if(bLastStatus==MI_OK)
					{
						for (sec=0; sec<8; sec++)
						{
							if ( (blk>0 || sec>0) )	// 2007/07/04, 
							{
								if ( m_AID[sec].wInt==g_tRegisters.s1.wAID.wInt )
								{
									return (blk*8+sec+16);
								}
								if ( m_AID[sec].wInt==0x4839 )	// 2007/07/04,support Configure Card (AID=4839H)
								{
									bAID_CFG = 1;
									return (blk*8+sec+16);
								}
								if ( m_AID[sec].wInt==OES_AID1 || m_AID[sec].wInt==OES_AID2 )	// 2008/8/18, Support OES card
								{
									bIsOES = 1;
									return (blk*8+sec+16);
							   	}
							}
						}
					}
				}
			}
		}// End Of MAD 2
	}// End Of AID Search in MAD

	// If any Error , must Request,AntiCollision,Select again
	if(bLastStatus!=MI_OK)
        bLastStatus = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, NULL, NULL, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);

	return g_tRegisters.s1.bSector;
}

uint Mifare_SearchType(BlockCacheBuffer_t* ptBlockCacheBuffer, BYTE nSector, u8 nType)
{
	u8 type;
	uint offset;

	offset = 0;
    while( Mifare_GetLineByte(ptBlockCacheBuffer, nSector, offset, &type) && offset<4096)
	{
		if ( (type & (BYTE)0xC0)==nType )
		{
			m_bSdlen = type & (BYTE)0x3F;
			return (offset+1);
		}
			
		if ( (type & (BYTE)0x3F)<(BYTE)0x3F )
		{
			offset = offset + (type & (BYTE)0x3F) + 1;
		}
		else
		{
			offset = offset + (BYTE)0x3F;

			do
			{
				if ( Mifare_GetLineByte(ptBlockCacheBuffer, nSector, offset, &type)==0 )
					return 0;

				offset = offset+1;
			}while (type!=0 && offset<4096);
		}
	}
	return 0;
}

u8 Mifare_Update_Buffer(void)
{
	u16 offset;
	u8 i;
    u8 tmpLen;

	m_bSdlen = 0;
	m_bAuthSector = 0xFF;
	memset(m_bTmpbuf, 0, sizeof(m_bTmpbuf));

    Is_Ours_Mfc_Setkey(g_tRegisters.s1.bAPP_KEY, PH_KEYSTORE_KEY_TYPE_MIFARE, STORE_KEYNUM_MFC, PHHAL_HW_MFC_ALL);

	if((g_tRegisters.s1.bEnableUID & 0x3F) < 2)     // Not UID-only
	{
        m_bAuthSector = Mifare_GetAIDSector();

		if ( g_tRegisters.s1.bLength==0 && bIsOES==0 )
		{
			offset = Mifare_SearchType(&m_tBlockCacheBuffer, m_bAuthSector, MF700_DFT_DATA);
			if ( offset>0 )
			{
				if ( m_bSdlen<0x3F )
				{
					if ( Mifare_Memcpy(&m_tBlockCacheBuffer, m_bTmpbuf, m_bAuthSector, offset, m_bSdlen)==0 )
						 m_bSdlen=0; // error
					else m_bSdlen--;
				}
				else
				{
					i=0;
					while(i<MAX_USER_DATA)
					{
						if(!Mifare_GetLineByte(&m_tBlockCacheBuffer, m_bAuthSector, offset+i, &m_bTmpbuf[i]))
						{
							i=0; // error
							break;
						}
						if(m_bTmpbuf[i]==0x00)
							break;
						i++;
					}
					m_bSdlen = i;
				}
			}
		}
		else
		{
			if(bIsOES)	// 2008/8/18, support OES wiegand
			{
				if(m_bAuthSector==1)
				{
					uint OESid;
					if(Mifare_Memcpy(&m_tBlockCacheBuffer, ((BYTE*)&OESid), m_bAuthSector, 0, sizeof(OESid)))
					{
						if(OESid!=0x4DB2)
							m_bAuthSector=15;
					}
				}
				else
					m_bAuthSector=15;

				offset = 48 - i;

				if(Mifare_Memcpy(&m_tBlockCacheBuffer, m_bTmpbuf+1, m_bAuthSector, offset, i))
				{
					SwapValue(m_bTmpbuf+1, i);
					m_bSdlen = i;
				}
			}
			else
            {
                if(Mifare_Memcpy(&m_tBlockCacheBuffer, (BYTE*)m_bTmpbuf, m_bAuthSector, g_tRegisters.s1.bOffset, g_tRegisters.s1.bLength))
                {
                    m_bSdlen = g_tRegisters.s1.bLength;
                }
            }
		}
	}
	
	// Support Uknow, Send UID
	if (m_bSdlen == 0 && (g_tRegisters.s1.bEnableUID & 0x3F) > 0)   // Not Data-only
	{
	 	memcpy(m_bTmpbuf, bUid, bUidSize);
		m_bSdlen = bUidSize;
	}

    #ifdef STANDARD_DF750_760A
    if((g_wPartNum == PART_NUM_DF750AK_00) || (g_wPartNum == PART_NUM_DF760AK_00))
    {
        if((m_bSdlen != 0) && ((g_tRegisters.s1.bEnableUID & 0x3F) < 2))    // Not UID-only
        {
            tmpLen = m_bSdlen;

            offset = Mifare_SearchType(&m_tBlockCacheBuffer, m_bAuthSector, MF700_DFT_SEX);

            if((offset > 0) && (m_bSdlen > 1))
            {
                dwPassCode = 0;

                m_bSdlen = _min_(m_bSdlen - 1, sizeof(dwPassCode));
                if (Mifare_Memcpy(&m_tBlockCacheBuffer, (uint8_t *)&dwPassCode, m_bAuthSector, offset + 1, m_bSdlen) != 0)
                {
                    if(dwPassCode > 0)
                    {
                        ulCONFIG_Tick = phDriver_GetTick();
                        bConfig_TimeSec = 10 * 2;

                        bVerifyMode = true;
                    }
                }
            }

            m_bSdlen = tmpLen;

            if(bVerifyMode == false)
            {
                if(g_tRegisters.s1.bKeyPad_Flag & Bit7)     // Have to check passcode
                    m_bSdlen = 0;
            }
        }
    }
    #endif

	return m_bSdlen;
}

// support Ultralight
u8 MF_UL_Update_Buffer(void)
{
	m_bSdlen = 0; 
	
	if((g_tRegisters.s1.bEnableUID & 0x3F) < 2) // Not UID-Only
	{
		u8 bBlockBuffer[16];
		u8 bOffset;
		u8 bPage;
        
		bOffset = g_tRegisters.s1.bOffset;
		bPage = g_tRegisters.s1.bSector;
		while(m_bSdlen<g_tRegisters.s1.bLength)
		{
			if(bOffset<4)
			{
				u8 bLen;
                if(PH_ERR_SUCCESS != MF_Read(0, bPage, 0, 4, bBlockBuffer, sizeof(bBlockBuffer)))
					break;	// exit loop
				bLen=(4-bOffset);
                if((m_bSdlen+bLen)>(sizeof(m_bTmpbuf)+1))
                    break;
				memcpy(m_bTmpbuf+m_bSdlen, (bBlockBuffer+bOffset), bLen);
				m_bSdlen+=bLen;
				bOffset=0;
			}
			else
				bOffset-=4;
			bPage++;
			if(bPage==0)
				break;
		}

		if(m_bSdlen>g_tRegisters.s1.bLength)
			m_bSdlen=g_tRegisters.s1.bLength;
	}

	// Support Uknow, Send UID
	if (m_bSdlen == 0 && (g_tRegisters.s1.bEnableUID & 0x3F) > 0)
	{
	 	memcpy(m_bTmpbuf, bUid, bUidSize);
		m_bSdlen = bUidSize;
	}
    
	return m_bSdlen;
}

// Check Configured Card
bool MF_DF_Check_Config(void)
{
    #define MAD0_AID_BLOCK  (0x00)
    #define MAD1_AID_BLOCK  (0x10 * 4)

    phStatus_t status = PH_ERR_SUCCESS;

    uint8_t Config_AID[2] = {0x39, 0x48};
    uint8_t bBlockBuffer[16];
    uint8_t bMAD_Ver = 0;
    uint8_t i, j;

    // Configured card is not enabled
    #ifdef STANDARD_DF750_760A
    if((g_tRegisters.s1.Alignment & Bit3) == 0)
    #else
    if((g_tRegisters.s1.CFG_FLlg & Bit3) == 0)
    #endif
        return 0;

    // Not Mifare classic card
    if(((bSak & 0x3B) != 0x08) && ((bSak & 0x3B) != 0x18) && ((bSak & 0x3B) != 0x19) && ((bSak & 0x3B) != 0x39))
        return 0;

    do {
        // Check MAD version
        Is_Ours_Mfc_Setkey(MAD_KEY, PH_KEYSTORE_KEY_TYPE_MIFARE, STORE_KEYNUM_MFC_MAD, PHHAL_HW_MFC_ALL);

        status = phalMfc_Authenticate(psalMFC, MAD0_AID_BLOCK, PHHAL_HW_MFC_KEYA, STORE_KEYNUM_MFC_MAD, 0, bUid, bUidSize);
        if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
            break;

        status = phalMfc_Read(psalMFC, 0x03, bBlockBuffer);
        if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
            break;

        if(bBlockBuffer[9] & 0x80)      // bit7: DA
        {
            if((bBlockBuffer[9] & 0x03) == 0x01)  // bit1/0: ADV
                bMAD_Ver = 1;
            else if((bBlockBuffer[9] & 0x03) == 0x10)
                bMAD_Ver = 2;
        }

        if(bMAD_Ver > 0)
        {
            // Check MAD rev.1 & rev.2
            for(i = 0x01; i < 0x03; i++)
            {
                status = phalMfc_Read(psalMFC, i, bBlockBuffer);
                if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                    break;

                for(j = 0; j < 8; j++)
                {
                    if(memcmp(Config_AID, bBlockBuffer + (j * 2), 2) == 0)
                        goto Is_Config_Card;
                }
            }

            // Check MAD rev.2
            if(bMAD_Ver == 2)
            {
                status = phalMfc_Authenticate(psalMFC, MAD1_AID_BLOCK, PHHAL_HW_MFC_KEYA, STORE_KEYNUM_MFC_MAD, 0, bUid, bUidSize);
                if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                    break;

                for(i = 0x01; i < 0x04; i++)
                {
                    status = phalMfc_Read(psalMFC, MAD1_AID_BLOCK + i, bBlockBuffer);
                    if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                        break;

                    for(j = 0; j < 8; j++)
                    {
                        if(memcmp(Config_AID, bBlockBuffer + (j * 2), 2) == 0)
                            goto Is_Config_Card;
                    }
                }
            }
        }

    } while(0);

    return 0;

Is_Config_Card:

    return 1;
}

// support DesFire
u8 MF_DF_Update_Buffer(void)
{
    phStatus_t status = PH_ERR_SUCCESS;

    uint8_t   * pDataBuffer = NULL;
    uint16_t 	wRxDataLen = RD_BUFFER_LEN;
    uint8_t 	aOffset[3] = {0x00, 0x00, 0x00};        // 3 bytes size is for NxpLib usage
    uint8_t 	aTxDataLen[3] = {0x00, 0x00, 0x00};     // 3 bytes size is for NxpLib usage
    uint8_t 	aKeySettings[2];
    uint8_t 	aFileSettings[17];
    uint8_t     bSettingsLen;
    uint8_t     bCommMode;
    uint8_t     aValueAmount[4];
    uint8_t 	aTxDataCnt[3] = {0x00, 0x00, 0x00};     // 3 bytes size is for NxpLib usage
    uint8_t     aAccessKey[32] = {0x00};                // for 3K3DES/AES192/AES256 24/32 bytes key

    #ifdef STANDARD_DF750_760A
    uint8_t     aPasscode_AID[4] = {0x3F, 0x70, 0xF4, 0x00};
    uint8_t     aPasscode_KEY[16] = {0x50, 0x41, 0x53, 0x53, 0x43, 0x4F, 0x44, 0x45, 0x5B, 0x10, 0x27, 0x04, 0x05, 0x10, 0x23, 0x5D};
    uint8_t     bPasscode_KNO = 2;
    uint8_t     bPasscode_FID = 0;
    #endif

	m_bSdlen = 0;
    memset(m_bTmpbuf, 0, sizeof(m_bTmpbuf));

	if((g_tRegisters.s1.bEnableUID & 0x3F) < 2) // Not UID-only
	{
        phpalI14443p4a_Rats(pDiscLoop->pPal1443p4aDataParams, ((phpalI14443p4a_Sw_DataParams_t *)(pDiscLoop->pPal1443p4aDataParams))->bFsdi,
                ((phpalI14443p4a_Sw_DataParams_t *)(pDiscLoop->pPal1443p4aDataParams))->bCid, bDataBuffer);

        phpalI14443p4_SetProtocol(pDiscLoop->pPal14443p4DataParams, PH_OFF, ((phpalI14443p4a_Sw_DataParams_t *)(pDiscLoop->pPal1443p4aDataParams))->bCid,
                   PH_OFF, PH_OFF, ((phpalI14443p4a_Sw_DataParams_t *)(pDiscLoop->pPal1443p4aDataParams))->bFwi, ((phpalI14443p4a_Sw_DataParams_t *)(pDiscLoop->pPal1443p4aDataParams))->bFsdi,
                   ((phpalI14443p4a_Sw_DataParams_t *)(pDiscLoop->pPal1443p4aDataParams))->bFsci);

        do {
            // Check Application Presence
            status = phalMfdf_SelectApplication(psalMFDF, g_tRegisters.s1.dfAID);
            if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                break;

            // Check Key Type
            switch(g_tRegisters.s1.dfOptions & 0x0F){
              case 1:   // AES
                MfDf_KeyStore_Setkey(g_tRegisters.s1.dfKeyValue, PH_KEYSTORE_KEY_TYPE_AES128, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW);
                status = phalMfdf_AuthenticateAES(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                break;
              case 2:   // 3K3DES
                memcpy(aAccessKey, g_tRegisters.s1.dfKeyValue, 16);
                memset(aAccessKey + 16, 0x00, 8);           // use 8 bytes of 0x00 as default key value for 3rd key

                MfDf_KeyStore_Setkey(aAccessKey, PH_KEYSTORE_KEY_TYPE_3K3DES, STORE_KEYNUM_3K3DES, STORE_KEYVER_3K3DES_NEW);
                status = phalMfdf_AuthenticateISO(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_3K3DES, STORE_KEYVER_3K3DES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                break;
              case 3:   // DES
                MfDf_KeyStore_Setkey(g_tRegisters.s1.dfKeyValue, PH_KEYSTORE_KEY_TYPE_2K3DES, STORE_KEYNUM_DES, STORE_KEYVER_DES_NEW);      // for 16 bytes key
                status = phalMfdf_Authenticate(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_DES, STORE_KEYVER_DES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                break;
              case 0:   // Auto
                status = phalMfdf_GetKeySettings(psalMFDF, aKeySettings);
                if((aKeySettings[1] & 0xC0) == 0x80)        // AES
                {
                    MfDf_KeyStore_Setkey(g_tRegisters.s1.dfKeyValue, PH_KEYSTORE_KEY_TYPE_AES128, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW);
                    status = phalMfdf_AuthenticateAES(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                }
                else if((aKeySettings[1] & 0xC0) == 0x40)   // 3K3DES
                {
                    memcpy(aAccessKey, g_tRegisters.s1.dfKeyValue, 16);
                    memset(aAccessKey + 16, 0x00, 8);       // use 8 bytes of 0x00 as default key value for 3rd key

                    MfDf_KeyStore_Setkey(aAccessKey, PH_KEYSTORE_KEY_TYPE_3K3DES, STORE_KEYNUM_3K3DES, STORE_KEYVER_3K3DES_NEW);
                    status = phalMfdf_AuthenticateISO(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_3K3DES, STORE_KEYVER_3K3DES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                }
                else    // if((aKeySettings[1] & 0xC0) == 0x00) or Others    // DES
                {
                    MfDf_KeyStore_Setkey(g_tRegisters.s1.dfKeyValue, PH_KEYSTORE_KEY_TYPE_2K3DES, STORE_KEYNUM_DES, STORE_KEYVER_DES_NEW);  // for 16 bytes key
                    status = phalMfdf_Authenticate(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_DES, STORE_KEYVER_DES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                }
                break;
              default:
                MfDf_KeyStore_Setkey(g_tRegisters.s1.dfKeyValue, PH_KEYSTORE_KEY_TYPE_AES128, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW);
                status = phalMfdf_AuthenticateAES(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW, g_tRegisters.s1.dfKeyNo, NULL, 0);
                break;
            }
            if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                break;

            status = phalMfdf_GetFileSettings(psalMFDF, g_tRegisters.s1.dfFileID, aFileSettings, &bSettingsLen);
            if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                break;

            // Check File Communication Mode
            switch((g_tRegisters.s1.dfOptions >> 4) & 0x0F){
              case 1:   // Plain
                bCommMode = PHAL_MFDF_COMMUNICATION_PLAIN;
                break;
              case 2:   // MACing
                bCommMode = PHAL_MFDF_COMMUNICATION_MACD;
                break;
              case 3:   // Full
                bCommMode = PHAL_MFDF_COMMUNICATION_ENC;
                break;
              case 0:   // Auto
                if((aFileSettings[1] & 0x03) == 0x03)       // Full
                    bCommMode = PHAL_MFDF_COMMUNICATION_ENC;
                else if((aFileSettings[1] & 0x03) == 0x01)  // MACing
                    bCommMode = PHAL_MFDF_COMMUNICATION_MACD;
                else    // if((aFileSettings[1] & 0x03) == 0x00/0x02)   // Plain
                    bCommMode = PHAL_MFDF_COMMUNICATION_PLAIN;
                break;
              default:
                bCommMode = PHAL_MFDF_COMMUNICATION_PLAIN;
                break;
            }

            // Check File Type
            switch(g_tRegisters.s1.dfFileType){
              case 0:   // Standard Data File
              case 1:   // Backup Data File
                memcpy(aOffset, g_tRegisters.s1.dfOffset, 2);   // Utility only sends 2 bytes size
                status = phalMfdf_ReadData(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aOffset, aTxDataLen, &pDataBuffer, &wRxDataLen);
                break;
              case 2:   // Value File
                status = phalMfdf_GetValue(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aValueAmount);
                if ((status & PH_ERR_MASK) == PH_ERR_SUCCESS)
                {
                    pDataBuffer = aValueAmount;
                    wRxDataLen = sizeof(aValueAmount);
                }
                break;
              case 3:   // Line Record File
                aTxDataCnt[0] = 1;
                status = phalMfdf_ReadRecords(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aOffset, aTxDataCnt, aTxDataLen, &pDataBuffer, &wRxDataLen);
                break;
              case 0xFF:   // Auto
                if(aFileSettings[0] == 2)      // Value File
                {
                    status = phalMfdf_GetValue(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aValueAmount);
                    if ((status & PH_ERR_MASK) == PH_ERR_SUCCESS)
                    {
                        pDataBuffer = aValueAmount;
                        wRxDataLen = sizeof(aValueAmount);
                    }
                }
                else if((aFileSettings[0] == 3) || (aFileSettings[0] == 4))      // Line Record File or Cyclic Record File
                {
                    aTxDataCnt[0] = 1;
                    status = phalMfdf_ReadRecords(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aOffset, aTxDataCnt, aTxDataLen, &pDataBuffer, &wRxDataLen);
                }
                else    // if((aFileSettings[0] == 0) || (aFileSettings[0] == 1)) or Others // Standard Data File or Backup Data File
                {
                    memcpy(aOffset, g_tRegisters.s1.dfOffset, 2);   // Utility only sends 2 bytes size
                    status = phalMfdf_ReadData(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aOffset, aTxDataLen, &pDataBuffer, &wRxDataLen);
                }
                break;
              default:
                memcpy(aOffset, g_tRegisters.s1.dfOffset, 2);   // Utility only sends 2 bytes size
                status = phalMfdf_ReadData(psalMFDF, bCommMode, g_tRegisters.s1.dfFileID, aOffset, aTxDataLen, &pDataBuffer, &wRxDataLen);
                break;
            }
            if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                break;

            // Update Data Length
            if((g_tRegisters.s1.dfFileType == 0) || (g_tRegisters.s1.dfFileType == 1)
            || ((g_tRegisters.s1.dfFileType == 0xFF) && ((aFileSettings[0] == 0) || (aFileSettings[0] == 1))))  // Standard Data File or Backup Data File
            {
                if(g_tRegisters.s1.dfLength > 0)
                {
                    if(wRxDataLen >= g_tRegisters.s1.dfLength)
                        m_bSdlen = g_tRegisters.s1.dfLength;
                }
                else
                {
                    if(wRxDataLen >= MAX_USER_DATA)
                        m_bSdlen = MAX_USER_DATA;
                    else
                        m_bSdlen = wRxDataLen;
                }
            }
            else
            {
                if(wRxDataLen >= MAX_USER_DATA)
                    m_bSdlen = MAX_USER_DATA;
                else
                    m_bSdlen = wRxDataLen;
            }

            // Update Data Buffer
            memcpy(m_bTmpbuf, pDataBuffer, m_bSdlen);

        } while(0);
	}

	// Support Uknow, Send UID
	if ((m_bSdlen == 0) && ((g_tRegisters.s1.bEnableUID & 0x3F) > 0))   // Not Data-only
	{
	 	memcpy(m_bTmpbuf, bUid, bUidSize);
		m_bSdlen = bUidSize;
	}

    #ifdef STANDARD_DF750_760A
    if((g_wPartNum == PART_NUM_DF750AK_00) || (g_wPartNum == PART_NUM_DF760AK_00))
    {
        if((m_bSdlen != 0) && ((g_tRegisters.s1.bEnableUID & 0x3F) < 2))    // Not UID-only
        {
            // Get Passcode
            do {
                // Check Application Presence
                status = phalMfdf_SelectApplication(psalMFDF, aPasscode_AID);
                if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                    break;

                status = phalMfdf_GetKeySettings(psalMFDF, aKeySettings);
                if((aKeySettings[1] & 0xC0) == 0x80)        // AES
                {
                    MfDf_KeyStore_Setkey(aPasscode_KEY, PH_KEYSTORE_KEY_TYPE_AES128, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW);
                    status = phalMfdf_AuthenticateAES(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_AES, STORE_KEYVER_AES_NEW, bPasscode_KNO, NULL, 0);
                }
                else if((aKeySettings[1] & 0xC0) == 0x40)   // 3K3DES
                {
                    memcpy(aAccessKey, aPasscode_KEY, 16);
                    memset(aAccessKey + 16, 0x00, 8);       // use 8 bytes of 0x00 as default key value for 3rd key

                    MfDf_KeyStore_Setkey(aAccessKey, PH_KEYSTORE_KEY_TYPE_3K3DES, STORE_KEYNUM_3K3DES, STORE_KEYVER_3K3DES_NEW);
                    status = phalMfdf_AuthenticateISO(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_3K3DES, STORE_KEYVER_3K3DES_NEW, bPasscode_KNO, NULL, 0);
                }
                else    // if((aKeySettings[1] & 0xC0) == 0x00) or Others    // DES
                {
                    MfDf_KeyStore_Setkey(aPasscode_KEY, PH_KEYSTORE_KEY_TYPE_2K3DES, STORE_KEYNUM_DES, STORE_KEYVER_DES_NEW);       // for 16 bytes key
                    status = phalMfdf_Authenticate(psalMFDF, PHAL_MFDF_NO_DIVERSIFICATION, STORE_KEYNUM_DES, STORE_KEYVER_DES_NEW, bPasscode_KNO, NULL, 0);
                }
                if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                    break;

                status = phalMfdf_GetFileSettings(psalMFDF, bPasscode_FID, aFileSettings, &bSettingsLen);
                if ((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
                    break;
                if((aFileSettings[1] & 0x03) == 0x03)       // Full
                    bCommMode = PHAL_MFDF_COMMUNICATION_ENC;
                else if((aFileSettings[1] & 0x03) == 0x01)  // MACing
                    bCommMode = PHAL_MFDF_COMMUNICATION_MACD;
                else    // if((aFileSettings[1] & 0x03) == 0x00/0x02)   // Plain
                    bCommMode = PHAL_MFDF_COMMUNICATION_PLAIN;

                status = phalMfdf_GetValue(psalMFDF, bCommMode, bPasscode_FID, aValueAmount);
                if ((status & PH_ERR_MASK) == PH_ERR_SUCCESS)
                {
                    dwPassCode = 0;

                    // Update Passcode
                    memcpy((uint8_t *)&dwPassCode, aValueAmount, sizeof(aValueAmount));

                    if(dwPassCode > 0)
                    {
                        ulCONFIG_Tick = phDriver_GetTick();
                        bConfig_TimeSec = 10 * 2;

                        bVerifyMode = true;
                    }
                }

            } while(0);

            if(bVerifyMode == false)
            {
                if(g_tRegisters.s1.bKeyPad_Flag & Bit7)     // Have to check passcode
                    m_bSdlen = 0;
            }
        }
    }
    #endif

	return m_bSdlen;
}

u8 Data_Output(phStatus_t ReadStatus)
{
	phStatus_t status = PH_ERR_SUCCESS;
    u8 i = 0;
    u8 bDataLen = 0, bin_len;
    u8 tk2buf[36];
	bool bWGOutParity, bOutput_Error = 0;

    if(PH_ERR_SUCCESS == ReadStatus)
    {
        switch(g_tRegisters.s1.bInterface)
        {
            case DATA_OUTPUT_WIEGAND:
            {
                // Wiegand Output *****************************************************
                if (g_tRegisters.s1.bW_Flag & Bit1)	// 2008/7/17, without parity bit
                {
                     bDataLen = g_tRegisters.s1.bW_Bits;
                     bWGOutParity = FALSE;
                }
                else
                {
                    #ifdef DF700A_NCS
                    if((g_tRegisters.s1.bEnableUID & 0x3F) == 2)            // UID-only
                        bDataLen = m_bSdlen * 8;
                    else
                    #endif
                        bDataLen = g_tRegisters.s1.bW_Bits - 2;             // Data Bits
                    bWGOutParity = TRUE;
                }
                
                // Wiegand DEC2STR Flag 
                if((g_tRegisters.s1.bW_Flag & WG_INP_FMT_MASK) == WG_INP_FMT_DEC_STR)
                {
                    if(DecToHex((char*)m_bTmpbuf, m_bSdlen, (unsigned char*)m_bTmpbuf, sizeof(m_bTmpbuf)) == 0)
                    {
                        if ((g_tRegisters.s1.bEnableUID & 0x3F) != No_Disable_Enable_UID)
                            memcpy(m_bTmpbuf, bUid, bUidSize);
                        else
                            return FALSE;
                    }
                }

                if ( g_tRegisters.s1.bW_Flag & Bit2 ) 			// 2010/7/19, Non-ISO format (Low byte first)
                {
                    memset(tk2buf, 0, sizeof(tk2buf));
                    bit_copy_LE(tk2buf, (u8*)m_bTmpbuf, 0, bDataLen);
                    memcpy(m_bTmpbuf, tk2buf, (bDataLen + 7) / 8);
                }

                if (g_tRegisters.s1.bW_Flag & Bit7)     // Include RID
                {
                    if ((g_tRegisters.s1.bW_Flag & Bit0) == 0)
                    {
                        AddBits(m_bTmpbuf, bDataLen, g_tRegisters.s1.bDeviceId, 8, FALSE);
                        bDataLen += 8;

                        if(g_tRegisters.s1.bWp_Len > 0)
                        {
                            AddBits(m_bTmpbuf, bDataLen, g_tRegisters.s1.bWp_Bits, 8, FALSE);
                            bDataLen += g_tRegisters.s1.bWp_Len;
                        }

                        Wiegand_MSB(m_bTmpbuf, bDataLen, bWGOutParity);             // MSB first
                    }
                    else
                    {
                        memset(tk2buf, 0, sizeof(tk2buf));
                        memcpy(tk2buf, m_bTmpbuf, (bDataLen + 7) / 8);
                        AddBits(m_bTmpbuf, 0, g_tRegisters.s1.bDeviceId, 8, FALSE);
                        memcpy(m_bTmpbuf + 1, tk2buf, (bDataLen + 7) / 8);
                        bDataLen += 8;

                        if(g_tRegisters.s1.bWp_Len > 0)
                        {
                            memset(tk2buf, 0, sizeof(tk2buf));
                            memcpy(tk2buf, m_bTmpbuf, (bDataLen + 7) / 8);
                            ShiftBits(tk2buf, m_bTmpbuf, 0, g_tRegisters.s1.bWp_Len, bDataLen, sizeof(tk2buf) * 8, TRUE);

                            AddBits(tk2buf, 0, g_tRegisters.s1.bWp_Bits, g_tRegisters.s1.bWp_Len, TRUE);
                            bDataLen += g_tRegisters.s1.bWp_Len;

                            memcpy(m_bTmpbuf, tk2buf, (bDataLen + 7) / 8);
                        }

                        Wiegand_LSB(m_bTmpbuf, bDataLen, bWGOutParity);	 	        // LSB first
                    }
                }
                else                                    // Without RID
                {
                    if ((g_tRegisters.s1.bW_Flag & Bit0) == 0)
                    {
                        if(g_tRegisters.s1.bWp_Len > 0)
                        {
                            AddBits(m_bTmpbuf, bDataLen, g_tRegisters.s1.bWp_Bits, 8, FALSE);
                            bDataLen += g_tRegisters.s1.bWp_Len;
                        }

                        Wiegand_MSB(m_bTmpbuf, bDataLen, bWGOutParity);		        // MSB first
                    }
                    else
                    {
                        if(g_tRegisters.s1.bWp_Len > 0)
                        {
                            memset(tk2buf, 0, sizeof(tk2buf));
                            memcpy(tk2buf, m_bTmpbuf, (bDataLen + 7) / 8);
                            ShiftBits(tk2buf, m_bTmpbuf, 0, g_tRegisters.s1.bWp_Len, bDataLen, sizeof(tk2buf) * 8, TRUE);
                            bDataLen += g_tRegisters.s1.bWp_Len;
                            memcpy(m_bTmpbuf, tk2buf, (bDataLen + 7) / 8);

                            AddBits(m_bTmpbuf, 0, g_tRegisters.s1.bWp_Bits, g_tRegisters.s1.bWp_Len, TRUE);
                        }

                        Wiegand_LSB(m_bTmpbuf, bDataLen, bWGOutParity);             // LSB first
                    }
                }
            }
                break;

            case DATA_OUTPUT_TK2:
            {
                memset(tk2buf, 0, sizeof(tk2buf));
                tk2buf[0] = (g_tRegisters.s1.bDeviceId % 10) + ((u8)(g_tRegisters.s1.bDeviceId / 10) << 4); // RID->BCD

                if ((g_tRegisters.s1.bT_Flag & Bit1) != 0)
                {
                    SwapValue(m_bTmpbuf, m_bSdlen);     // LSB->MSB
                }

                // ABA TK2 Output *****************************************************
                bDataLen = g_tRegisters.s1.bT_Byte;     //Code Length

                switch(g_tRegisters.s1.bT_Format)
                {
                    case TK2_DEC:           // DEC string

                        if (bDataLen > 0)
                            STR2BCD(m_bTmpbuf, m_bSdlen, tk2buf + 1, sizeof(tk2buf) - 1);
                        else
                            bDataLen = STR2BCD(m_bTmpbuf, m_bSdlen, tk2buf + 1, sizeof(tk2buf) - 1);
                        break;

                    case TK2_BCD:           // BCD

                        memcpy(tk2buf + 1, m_bTmpbuf, sizeof(tk2buf) - 1);

                        if (bDataLen == 0)
                        {
                            bDataLen = m_bSdlen * 2;
                        }
                        break;

                    case TK2_DIRECT:        // Direct (Memory Map)

                        for (i = 0; i < (sizeof(tk2buf) - 1); i++)
                            tk2buf[i + 1] = ((u8)m_bTmpbuf[i] >> 4) | (((u8)m_bTmpbuf[i] << 4) & 0xF0); 

                        if (bDataLen == 0)
                        {    
                            bDataLen = m_bSdlen * 2;
                        }
                        break;

                    case TK2_BYTE_TO_DEC:   // Byte to Dec.

                        bin_len = m_bSdlen;
                        m_bSdlen = BYTE2BCD(m_bTmpbuf, m_bSdlen, tk2buf + 1, sizeof(tk2buf) - 1);

                        for (i = 0; i < m_bSdlen; i++) // High-Low Swap
                            m_bTmpbuf[i] = (tk2buf[i + 1] >> 4) | ((tk2buf[i+1] << 4) & 0xF0); 

                        memcpy(tk2buf + 1, m_bTmpbuf, m_bSdlen);

                        if (bDataLen == 0)
                            bDataLen = bin_len * 3;
                        break;

                    case TK2_BIN_TO_DEC:    // Binary (Default)
                    default:

                        if (bDataLen > 0)
                             HEX2BCD(m_bTmpbuf, m_bSdlen, tk2buf + 1, sizeof(tk2buf) - 1);
                        else
                            bDataLen = HEX2BCD(m_bTmpbuf, m_bSdlen, tk2buf + 1, sizeof(tk2buf) - 1);
                        break;
                }

                if(g_tRegisters.s1.bT_Format == TK2_BCD)
                {
                    for(i = 1; i <= m_bSdlen; i++)
                    {
                        if(((tk2buf[i] & 0xF0) >= 0xA0) || ((tk2buf[i] >> 4) >= 0x0A))
                        {
                            bOutput_Error = 1;
                            status = ERR_FORMAT_UNUSUAL;
                        }
                    }
                }

                if(bOutput_Error != 1)
                {
                    if (g_tRegisters.s1.bT_Flag & Bit7)           // Include RID
                    {
                        if (g_tRegisters.s1.bT_Flag & Bit0)
                        {
                            TK2_Output_LSB(tk2buf, bDataLen + 2);            // LSB First
                        }
                        else
                        {
                            tk2buf[(bDataLen / 2) + 1] = tk2buf[0];
                            TK2_Output_MSB(tk2buf + 1, bDataLen + 2);        // MSB First
                        }
                    }
                    else                                          // Without RID
                    {
                        if (g_tRegisters.s1.bT_Flag & Bit0)
                            TK2_Output_LSB(tk2buf + 1, bDataLen);            // LSB First
                        else
                            TK2_Output_MSB(tk2buf + 1, bDataLen);            // MSB First
                    }
                }
            }
                break;

            case DATA_OUTPUT_RS232:
                #ifdef STANDARD_DF750_760A
                if(0)
                #else
                if(g_tRegisters.s1.S_InputDataType)
                #endif
                {
                    for (i=1; i<=m_bSdlen; i++)	
                    {
                        if(m_bTmpbuf[i]==0)
                        {
                            m_bSdlen=(i-1);
                            break;
                        }
                    }
                }

                if ( g_tRegisters.s1.bS_Baud>0 )
                    serial_baudrate(g_tRegisters.s1.bS_Baud * 2400l);	// Set Output Baudrate

                if ( g_tRegisters.s1.bS_Flag & Bit6 )				// Header
                    bDataBuffer[bDataLen++] = g_tRegisters.s1.bS_Header;

                if ( g_tRegisters.s1.bS_Flag & Bit7 )				// RID
                    bDataBuffer[bDataLen++] = g_tRegisters.s1.bDeviceId;

                if ( g_tRegisters.s1.bS_Flag & Bit5 )				// Send Data Length
                    bDataBuffer[bDataLen++] = m_bSdlen;

                if(g_tRegisters.s1.bS_Flag & Bit0)
                {
                    SwapValue(m_bTmpbuf, m_bSdlen);     //MSB
                }
                
                if(g_tRegisters.s1.bS_Flag & Bit1)				// Format Hex/Bin
                {
                    BIN2STR((u8*)m_bTmpbuf,m_bSdlen,(u8*)bDataBuffer+bDataLen,sizeof(bDataBuffer)-bDataLen);
                    bDataLen+=m_bSdlen*2;
                }
                else
                {
                    memcpy(bDataBuffer+bDataLen, m_bTmpbuf, m_bSdlen);
                    bDataLen+=m_bSdlen;
                }

                
                if ( g_tRegisters.s1.bS_Flag & Bit4 )				// Send CR
                {
                    bDataBuffer[bDataLen++] = _CR_;
                }

                if ( g_tRegisters.s1.bS_Flag & Bit3 )				// Send LF
                {
                    bDataBuffer[bDataLen++] = _LF_;
                }

                if ( g_tRegisters.s1.bS_Flag & Bit2 )				// Tailer
                {
                    bDataBuffer[bDataLen++] = g_tRegisters.s1.bS_Tailer;
                }
                
                if(g_CmdSource == CMD_SOURCE_RS232)
                    RS232_Output(bDataBuffer, bDataLen);
                else if(g_CmdSource == CMD_SOURCE_RS485)
                    RS485_Output(bDataBuffer, bDataLen);

                if ( g_tRegisters.s1.bS_Baud>0 )
                {
                    serial_baudrate(19200l);		// return to default
                }

                break;
            default:
                    status = ERR_FORMAT_UNUSUAL;
                break;
        }
    }
    else
    {
        if ( g_tRegisters.s1.bS_Baud>0 )
            serial_baudrate(g_tRegisters.s1.bS_Baud * 2400l);	// Set Output Baudrate

        bDataBuffer[bDataLen++] = 0x15;
        
        if(g_CmdSource == CMD_SOURCE_RS232)
            RS232_Output(bDataBuffer, bDataLen);
        else if(g_CmdSource == CMD_SOURCE_RS485)
            RS485_Output(bDataBuffer, bDataLen);

        if ( g_tRegisters.s1.bS_Baud>0 )
        {
            serial_baudrate(19200l);		// return to default
        }
        status = ERR_FORMAT_UNUSUAL;
    }

    return status;
}

void Status_Output(phStatus_t OutputStatus)
{
    if(PH_ERR_SUCCESS == OutputStatus)
    {
        bData_Output = 1;

        if ((g_tRegisters.s1.bLED_Flag & Bit2)==0)
        {
            #ifdef DF700A_NCS
            LedStatusCheck(LEDBUZ_STATUS_1BEEP, true);      // Forced to beep under 2-wire control
            #else
            if(g_tRegisters.s1.bLED_Flag & Bit3)
                LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
            else
                LedStatusCheck(LEDBUZ_STATUS_NORMAL, false);
            #endif
        }
        else
            LedStatusCheck(LEDBUZ_STATUS_VALID, false);
    }
    else
    {
        if((g_tRegisters.s1.bLED_Flag & Bit0)!=0)
            LedStatusCheck(LEDBUZ_STATUS_EXTERNAL, false);
        else
            LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
    }
}

u32 GetCurrentCardClass(void) 
{
    u16 wATQA;
    wATQA = (((u16)m_bATQA[0]<<8) | m_bATQA[1]);
	u32 ulResult=(wATQA & 0xFFFF);
	ulResult=((((u32)bSak<<16)) | (((u32)wATQA) & 0xFFFFul));
	if((wATQA & 0x0Ful)!=TYPE_NOT_DEFINE)
	{
		switch(wATQA & 0xFF00ul)
		{
		case 0x0400ul:
			ulResult=MF_SMARTMX_CL1_1K;
			break;
		case 0x0200ul:
			ulResult=MF_SMARTMX_CL1_4K;
			break;
		case 0x4800ul:
			ulResult=MF_SMARTMX_CL2;
			break;
		}
	}
	return ulResult;
}

#define CARD_FPHEAD_SIZE   5   //sizeof(CARD_FPHEAD)
void MF700_CFG_Card(void)
{
	CARD_FPHEAD info;

    m_bAuthSector = Mifare_GetAIDSector();

    Is_Ours_Mfc_Setkey(g_tRegisters.s1.bCfgKey, PH_KEYSTORE_KEY_TYPE_MIFARE, STORE_KEYNUM_MFC, PHHAL_HW_MFC_ALL);

	// read cfg head info
    if(Mifare_Memcpy(&m_tBlockCacheBuffer, m_bTmpbuf, m_bAuthSector, 0, CARD_FPHEAD_SIZE))
	{
        info.type = m_bTmpbuf[0];
        info.action = m_bTmpbuf[1];
        info.fno = m_bTmpbuf[2];
        info.length = (u16)(((u8)m_bTmpbuf[4] << 8) | (u8)m_bTmpbuf[3]);
		if ( info.type==0xF0 && info.action==0xCF && info.length>0 )
		{
            memset(m_bTmpbuf, 0x00, sizeof(m_bTmpbuf));
            if(Mifare_Memcpy(&m_tBlockCacheBuffer, m_bTmpbuf, m_bAuthSector, CARD_FPHEAD_SIZE, info.length))
			{
                bAID_CFG = 0;
                phpalI14443p3a_HaltA(pDiscLoop->pPal1443p3aDataParams);
                GNET_SetRegister((u16)PBL_SETTING, (u8*)m_bTmpbuf, info.length);
				return;
			}
		}
	}

	ReadConfig();

	phpalI14443p3a_HaltA(pDiscLoop->pPal1443p3aDataParams);
}

u8 Mifare_AUTH_SECTOR(void)
{
	phStatus_t status = ERR_MIFARE_FORMAT;

    status = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, NULL, NULL, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);

    if(status == PH_ERR_SUCCESS)
    {
        // Reset alive period counting
        if(bLoopLIVE == LOOP_ENABLE)
            ulALIVE_Tick = phDriver_GetTick();

        status = MI_KEYERR;

        #ifdef STANDARD_DF750_760A
        if((bConfigMode == false) && ((g_wPartNum == PART_NUM_DF750AK_00) || (g_wPartNum == PART_NUM_DF760AK_00)))
            bAID_CFG = 0;
        else
        #endif
        bAID_CFG = MF_DF_Check_Config();

        // if(m_bAuthSector!=0xFF)
        {
            #ifdef STANDARD_DF750_760A
            if((bConfigMode == false) && (bVerifyMode == false) && (bAID_CFG == 0)) // Add (bAID_CFG == 0) for non-keypad DF750A/760A
            #else
            if(bAID_CFG == 0) // 2007/07/04, for configure card
            #endif
            {
                BlockCacheBufferInfoInit(&m_tBlockCacheBuffer);
                
                switch(GetCurrentCardClass())
                {
                    case MF_ULTRALIGHT_CL2:
                        if(MF_UL_Update_Buffer() > 0)
                            status = MI_OK;
                        break;
                    case MF_1K_CL1:
                    case MF_4K_CL1:
                    case MF_1K_CL1_UID7:
                    case MF_4K_CL1_UID7:
                    case MF_1K_CL1_Infineon:
                    case MF_4K_CL1_Infineon:
                    case MF_1K_CL1_UID7_Infineon:
                    case MF_4K_CL1_UID7_Infineon:
                        if((g_tRegisters.s1.dfDataType & Bit7) != 0)
                        {
                            if(Mifare_Update_Buffer() > 0)
                            {
                                #ifdef STANDARD_DF750_760A
                                if(bVerifyMode == true)
                                    status = GNET_ERROR_DONT_RESPONSE;
                                else
                                #endif
                                status = MI_OK;
                            }
                        }
                        break;
                    case MF_DESFIRE_CL2:
                    case MF_DESFIRE:
                        if((g_tRegisters.s1.dfDataType & Bit6) == 0)
                        {
                            if(MF_DF_Update_Buffer() > 0)
                            {
                                #ifdef STANDARD_DF750_760A
                                if(bVerifyMode == true)
                                    status = GNET_ERROR_DONT_RESPONSE;
                                else
                                #endif
                                status = MI_OK;
                            }
                        }
                        break;
                    default:
                        status = MI_NOTAGERR;
                        break;
                }
            }
            #ifdef STANDARD_DF750_760A
            else if(bAID_CFG == 1)      // under ((bConfigMode == true) || (bVerifyMode == true))
            #else
            else
            #endif
            {
                MF700_CFG_Card();

                // To prevent repeated reading config card after reboot by itself
                do
                {
                    status = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, bUid, bUidSize, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);

                    if(status != PH_ERR_SUCCESS)
                        break;

                    status = phpalI14443p3a_HaltA(pDiscLoop->pPal1443p3aDataParams);

                    phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 5, NULL);

                }while(1);

                NVIC_SystemReset();

                #ifdef STANDARD_DF750_760A
                bConfigMode = false;
                #endif

                status = GNET_ERROR_DONT_RESPONSE;
            } // end of bAID_CFG
            #ifdef STANDARD_DF750_760A
            else        // (bAID_CFG == 0) under ((bConfigMode == true) || (bVerifyMode == true))
            {
                status = GNET_ERROR_DONT_RESPONSE;
            }
            #endif
        }//if(m_bAuthSector!=0xFF))

        if(status != GNET_ERROR_DONT_RESPONSE)
        {
            status = Data_Output(status);

            phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 50, NULL);

            Status_Output(status);

            if(g_tRegisters.s1.bContinue==0)	// One Time
            {
                status = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, NULL, NULL, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);
                
                if(status != PH_ERR_SUCCESS)
                    status = phpalI14443p3a_ActivateCard(pDiscLoop->pPal1443p3aDataParams, NULL, NULL, bUid, &bUidSize, &bSak, &bMoreCardsAvailable);
                    
                status = phpalI14443p3a_HaltA(pDiscLoop->pPal1443p3aDataParams);
            }
            else
                phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 200, NULL);
        }
    }
    else
        status = ERR_MIFARE_FORMAT;

    return status;
}
