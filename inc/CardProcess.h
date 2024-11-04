/*************************************************************************************
 *
 *
 *************************************************************************************/
#ifndef _CARD_PROCESS_DEFINE_
#define _CARD_PROCESS_DEFINE_

#include "ams_types.h"
#include "phApp_Init.h"

#define MF_Read(bSector, bBlocks, bOffset, bLength, bOutBuf, bOutBufSize)			MIFARE_READ_WRITE(1, bSector, bBlocks, bOffset, bLength, NULL, bOutBuf, bOutBufSize)
#define MF_Weite(bSector, bBlocks, bWriteBuf)			                            MIFARE_READ_WRITE(0, bSector, bBlocks, NULL, NULL, bWriteBuf, NULL, NULL)

#define MIFARE_SECTOR_BLOCK_SIZE        (16*3)
#define MIFARE_SECTOR_SIZE              40
#define MAX_USER_DATA	                128
#define MAX_USER_BUF	                255
#define MF700_DFT_SEX                   0x80
#define MF700_DFT_DATA                  0xC0
#define TYPE_NOT_DEFINE                 0

#define DATA_CONVERT_ASC2STR            0
#define DATA_CONVERT_BCD2DEC            1
#define DATA_CONVERT_BIN2STR            2
#define DATA_CONVERT_HEX2DEC            3


#define TYPE_MIFARE_CLASSIC             0
#define TYPE_MIFARE_DESFIRE             1
#define TYPE_ALL                        2

#define ERR_MIFARE_FORMAT               0x10
#define ERR_FORMAT_UNUSUAL              0x11

#define MIFARE_BLOCK_SIZE					16

#define LINE_BLOCK_DATA_BLOCK_ONLY_FLAG		0x8000u
#define LINE_BLOCK_NUMBER_INVALID			0xFFFFu

#define MAX_CARD_UID_LEN	            10

// DATA output Set
typedef enum
{
		DATA_OUTPUT_WIEGAND = 0,
		DATA_OUTPUT_TK2 = 1,
		DATA_OUTPUT_RS232 = 2,
} DATA_OutputSet;

typedef struct 
{
	uint8_t	bLen;
	uint8_t bUIDs[MAX_CARD_UID_LEN];
} UID_INFO_T;

typedef struct
{
	uint uLineBlockNumber;
	BYTE bBuffer[MIFARE_BLOCK_SIZE];
} BlockCacheBuffer_t;

typedef struct _card_fphead
{
	u8	type;
    u8  action;
    u8	fno;
    u16 length;
}CARD_FPHEAD;

typedef union UINT_T
{
	WORD	wInt;
	BYTE	bChar[2];
}UINT_T;

extern u8 MAD_KEY[6];
extern bool bAutoMode;
extern uint8_t m_bATQA[2];
extern u8 bCmdTask;
extern uint8_t m_bAuthSector;

extern uint8_t m_bSdlen;
extern BYTE m_bTmpbuf[];


u8 Mifare_Update_Buffer(void);
u8 MF_UL_Update_Buffer(void);
u8 MF_DF_Update_Buffer(void);
bool MF_DF_Check_Config(void);
u32 GetCurrentCardClass(void);
BYTE Mifare_GetAIDSector(void);
u8 Data_Output(phStatus_t ReadStatus);
void Status_Output(phStatus_t OutputStatus);
uint SectorToByteOffset(BYTE bSector, bool bIsDataBlockOnly);
void BlockCacheBufferInfoInit(BlockCacheBuffer_t* ptBlockCacheBuffer);
BYTE ReadLineBlockBytes(BlockCacheBuffer_t* ptBlockCacheBuffer, uint uOffset, BYTE* pbBuffer, BYTE bLength);
bool Mifare_Memcpy(BlockCacheBuffer_t* ptBlockCacheBuffer, BYTE* dest, BYTE nSector, uint index, u8 len);
bool Mifare_GetLineByte(BlockCacheBuffer_t* ptBlockCacheBuffer, BYTE StartSector, uint index, BYTE* dat);
u8 Mifare_AUTH_SECTOR(void);
u16 MIFARE_READ_WRITE(bool Select, u8 bSector, u8 bBlocks, u8 bOffset, u8 bLength, u8* bWriteBuf, u8* bOutBuf, u8 bOutBufSize);

#define MF_ULTRALIGHT_CL2		        0x004400ul
#define MF_MINI_CL1				        0x090400ul
#define MF_1K_CL1				        0x080400ul
#define MF_4K_CL1				        0x180200ul
#define MF_1K_CL1_UID7			        0x084400ul
#define MF_4K_CL1_UID7			        0x184200ul
#define MF_1K_CL1_Infineon				0x880400ul
#define MF_4K_CL1_Infineon				0x980200ul
#define MF_1K_CL1_UID7_Infineon			0x884400ul
#define MF_4K_CL1_UID7_Infineon			0x984200ul
#define MF_PLUS_CL1_2K			        0x100400ul
#define MF_PLUS_CL1_4K			        0x110200ul
#define MF_PLUS_CL2_2K			        0x104400ul
#define MF_PLUS_CL2_4K			        0x114200ul
#define MF_PLUS_CL1_SL3_2K		        0x200400ul
#define MF_PLUS_CL1_SL3_4K		        0x200200ul
#define MF_PLUS_CL2_SL3_2K		        0x204400ul
#define MF_PLUS_CL2_SL3_4K		        0x204200ul
#define MF_SMARTMX_CL1_1K		        0xFF040Ful
#define MF_SMARTMX_CL1_4K		        0xFF020Ful
#define MF_SMARTMX_CL2			        0xFF480Ful
#define MF_DESFIRE_CL2			        0x204403ul
#define MF_DESFIRE			            0x200403ul
#define MF_PRO_1K_CL1			        0x280400ul
#define MF_PRO_4K_CL1			        0x380200ul
#define MF_PRO_1K_CL2_UID7		        0x284400ul
#define MF_PRO_4K_CL2_UID7		        0x384200ul

///////////////////////////////////////
// Status Define
///////////////////////////////////////
#define READER_ERR_BASE_START           (uint8_t)0
#define MI_OK                           (uint8_t)0
#define MI_CHK_OK                       (uint8_t)0
#define MI_CRC_ZERO                     (uint8_t)0

#define MI_CRC_NOTZERO                  (uint8_t)1

#define MI_CRCERR                       (uint8_t)2
#define MI_EMPTY                        (uint8_t)3
#define MI_AUTHERR                      (uint8_t)4
#define MI_PARITYERR                    (uint8_t)5
#define MI_CODEERR                      (uint8_t)6

#define MI_SERNRERR                     (uint8_t)8
#define MI_KEYERR                       (uint8_t)9
#define MI_NOTAUTHERR                   (uint8_t)10
#define MI_BITCOUNTERR                  (uint8_t)11
#define MI_BYTECOUNTERR                 (uint8_t)12
#define MI_IDLE                         (uint8_t)13
#define MI_TRANSERR                     (uint8_t)14
#define MI_WRITEERR                     (uint8_t)15
#define MI_INCRERR                      (uint8_t)16
#define MI_DECRERR                      (uint8_t)17
#define MI_READERR                      (uint8_t)18
#define MI_OVFLERR                      (uint8_t)19
#define MI_POLLING                      (uint8_t)20
#define MI_FRAMINGERR                   (uint8_t)21
#define MI_ACCESSERR                    (uint8_t)22
#define MI_UNKNOW_COMMAND              	(uint8_t)23
#define MI_COLLERR                      (uint8_t)24
#define MI_RESETERR                     (uint8_t)25
#define MI_INITERR                      (uint8_t)25
#define MI_INTERFACEERR                 (uint8_t)26
#define MI_ACCESSTIMEOUT                (uint8_t)27
#define MI_NOBITWISEANTICOLL            (uint8_t)28
#define MI_QUIT                         (uint8_t)30
#define MI_NOTAGERR                     (uint8_t)31
#define MI_CHK_FAILED                   (uint8_t)32
#define MI_CHK_COMPERR                  (uint8_t)33
#define MI_RECBUF_OVERFLOW              (uint8_t)34 
#define MI_SENDBYTENR                   (uint8_t)35
	
#define MI_SENDBUF_OVERFLOW             (uint8_t)36
#define MI_BAUDRATE_NOT_SUPPORTED       (uint8_t)37
#define MI_SAME_BAUDRATE_REQUIRED       (uint8_t)38

#define MI_WRONG_PARAMETER_VALUE        (uint8_t)39

#define MI_BREAK                        (uint8_t)40
#define MI_NY_IMPLEMENTED               (uint8_t)41
#define MI_NO_MFRC                      (uint8_t)42
#define MI_MFRC_NOTAUTH                 (uint8_t)43
#define MI_WRONG_DES_MODE               (uint8_t)44
#define MI_HOST_AUTH_FAILED             (uint8_t)45

#define MI_WRONG_LOAD_MODE              (uint8_t)46
#define MI_WRONG_DESKEY                 (uint8_t)47
#define MI_MKLOAD_FAILED                (uint8_t)48
#define MI_FIFOERR                      (uint8_t)49
#define MI_WRONG_ADDR                   (uint8_t)50
#define MI_DESKEYLOAD_FAILED            (uint8_t)51

#define MI_WRONG_SEL_CNT                (uint8_t)52
#define MI_CASCLEVEX                    (uint8_t)52

#define MI_WRONG_TEST_MODE              (uint8_t)53
#define MI_TEST_FAILED                  (uint8_t)54
#define MI_TOC_ERROR                    (uint8_t)55
#define MI_COMM_ABORT                   (uint8_t)56
#define MI_INVALID_BASE                 (uint8_t)57
#define MI_MFRC_RESET                   (uint8_t)58
#define MI_WRONG_VALUE                  (uint8_t)59
#define MI_VALERR                       (uint8_t)60
#define MI_SAKERR                 		(uint8_t)61	

#define TK2_BIN_TO_DEC          0
#define TK2_DEC                 1
#define TK2_BCD                 2
#define TK2_DIRECT              3
#define TK2_BYTE_TO_DEC         4

#endif	// _CARD_PROCESS_DEFINE_
