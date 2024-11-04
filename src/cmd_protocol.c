/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "string.h"
#include <ctype.h>
#include "stdio.h"
#include "CardProcess.h"
#include "mb_errno.h"

#include <asf.h>
#include "Porting_SAMD21.h"

#define MAX_BLOCK_SIZE	240
#define MAX_BLKBUF_SIZE 152

#define _min_(a, b)((a<b)?(a):(b))
#define _max_(a, b)((a>b)?(a):(b))


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
CmdProtocol_t cmd_parser;

GnetPlus_ASCII_t ASCII_parser;

#if defined(ORG_BUZZ_MODE)
Control_Buzzer_t control_buz;
#else   //defined(MULTI_BUZZ_MODE)
Multiple_Buzzer_t Multi_buz;
#endif

u8 bGnetPlusMode = 0;
bool bSTX;
u8 cmd[4], cmdlen;

u8 m_nCurrentAddr;

bool m_bHalt = false;

#ifdef STANDARD_DF750_760A
const char sFWVer[] = "PGM-T2172 V1.0R0 (231013)";
#elif defined(STANDARD_DF7X0A)
#ifdef DF700A_00
const char sFWVer[] = "PGM-T2120 V1.0R1 (230905)";
#elif defined(DF700A_NCS)
const char sFWVer[] = "PGM-T2209 V1.0R0 (230822)";
#elif defined(DF710A_00)
const char sFWVer[] = "PGM-T2173 V1.0R1 (230905)";
#endif
#endif
const char szClass[] = "DF5";

// String for Feature Part Number
// Note: the (string length + 1) must not exceed the size of bPartName[])
const char sName_DF700A_00[]  = "DF700A-00";
const char sName_DF710A_00[]  = "DF710A-00";
const char sName_DF710A_U2[]  = "DF710A-U2";
const char sName_DF750AK_00[] = "DF750AK-00";
const char sName_DF750A_00[]  = "DF750A-00";
const char sName_DF760AK_00[] = "DF760AK-00";
const char sName_DF760A_00[]  = "DF760A-00";


extern u8 FlashErase(void);
extern u8 FlashWrite(uint8_t* pbBuffer, uint32_t uMemoryAddress, uint16_t* puLength);

extern phacDiscLoop_Sw_DataParams_t * pDiscLoop;
extern void *psalMFC;

extern struct usart_module usart_instance;

u8 bCmdTask = 0;

/******************************************************************************/
/**
  * @brief  
  * @param  
  * @retval 
  */
u16 CRC16_Calculate_GP(u8* pData, u16 data_len, u16 first_crc)
{
    u16 i;
    u8 j;

    volatile u16 CrcValue;

    CrcValue = first_crc;
    i = 0;

		do{

				CrcValue ^= pData[i];
        j = 0x80;

				do{
            if((CrcValue & 0x01) == 1)
            {
                CrcValue >>= 1;
                CrcValue ^= DEF_CRC_POLYNOM;
            }
            else
                CrcValue = (CrcValue>> 1);
            j >>= 1;
        }while(j);

        i++;

    }while(i < data_len);

    return CrcValue;
}

/**
  * @brief  
  * @param  
  * @retval 
  */
u8 CRC16_Verify_GP(u8* check_data, u16 check_len)
{
    u16 ulDataCRC;

    ulDataCRC = check_data[check_len - 2];
    ulDataCRC = ((ulDataCRC << 8) | (check_data[check_len - 1]));

    return !(ulDataCRC == CRC16_Calculate_GP(check_data, (check_len - 2), DEF_CRC_PRESET));
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void CRC16_Append_GP(u8* calculated_data, u16 calculated_len)
{
    volatile u16 crc;

    crc = CRC16_Calculate_GP(calculated_data, calculated_len, DEF_CRC_PRESET);

		calculated_data[calculated_len + 1] = (u8)(crc & 0xFF);
    calculated_data[calculated_len] = (u8)((crc >> 8) & 0xFF);
}

uint8_t Flashwritein(unsigned long Addr, unsigned char *data, uint16_t wLen)
{
    uint16_t wFStatus;
    uint8_t FlashStatus;
    uint8_t bCount = 0;

    do
    {
        wFStatus = phDriver_FLASHWrite(Addr, data, 0, wLen);     // Need to check Addr and Offset used by caller

        bCount++;
    } while ((wFStatus != PH_DRIVER_SUCCESS) && (bCount < 5));

    if (wFStatus == PH_DRIVER_SUCCESS)
        FlashStatus = HAL_OK;
    else
        FlashStatus = HAL_ERROR;

    return FlashStatus;
}

void FlashRead(unsigned long Addr, unsigned char *data, uint16_t wLen)
{
    phDriver_FLASHRead(Addr, data, wLen);
}

/******************************************************************************/
/**
  * @brief  
  * @param  
  * @retval 
  */
s8 FLASH_EEPROM_ReadBytes(uint8_t* pbBuffer, uint32_t ulStartAddress, uint32_t ulOffsetAddress, uint16_t* puLength)
{
    uint16_t wStatus;

    wStatus = phDriver_FLASHRead(ulStartAddress + ulOffsetAddress, pbBuffer, *puLength);

    if (wStatus == PH_DRIVER_SUCCESS)
        return HAL_OK;
    else
        return HAL_ERROR;
}

/**
  * @brief  
  * @param  
  * @retval 
  */
s8 FLASH_EEPROM_WriteBytes(uint8_t* pbBuffer, uint32_t ulStartAddress, uint32_t ulOffsetAddress, uint16_t* puLength)
{
    uint16_t wStatus;

    wStatus = phDriver_FLASHWrite(ulStartAddress, pbBuffer, ulOffsetAddress, *puLength);

    if (wStatus == PH_DRIVER_SUCCESS)
        return HAL_OK;
    else
        return HAL_ERROR;
}

/******************************************************************************/
#if USE_I2C
/**
  * @brief  
  * @param  
  * @retval 
  */
s8 EEPROM_ReadData(uint8_t* pbBuffer, uint16_t uMemoryAddress, uint16_t* puLength)
{
		s8 cResult;
		u16 uRemainBytes = *puLength;

        __disable_irq();
        cResult = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)EEPROM_ADDRESS, uMemoryAddress, I2C_MEMADD_SIZE_16BIT, (uint8_t *)pbBuffer, uRemainBytes, 5000);

        /* Reading process Error */
		if(cResult == HAL_OK)
        {
            /* Wait for the end of the transfer */
            while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);
        }
        
        __enable_irq();

		return cResult;
}

/**
  * @brief  
  * @param  
  * @retval 
  */
s8 EEPROM_WriteData(uint8_t* pbBuffer, uint16_t uMemoryAddress, uint16_t* puLength)
{
		s8 cResult;
		u16 uWriteBytes, uRemainBytes = *puLength, uBufOffset = 0;

        __disable_irq();
		HAL_FLASHEx_DATAEEPROM_Unlock();
		/* Since page size is 128 bytes, the write procedure will be done in a loop */
		while(uRemainBytes > 0)
		{
				if(uRemainBytes >= EEPROM_PAGESIZE)
						uWriteBytes = EEPROM_PAGESIZE;
				else
						uWriteBytes = uRemainBytes;

				/* Check boundary of page address */
                if(((uMemoryAddress % EEPROM_PAGESIZE) + uWriteBytes) > EEPROM_PAGESIZE)
                    uWriteBytes = (EEPROM_PAGESIZE - (uMemoryAddress % EEPROM_PAGESIZE));

				/* Write EEPROM_PAGESIZE */
                cResult = HAL_I2C_Mem_Write(&hi2c1 , (uint16_t)EEPROM_ADDRESS, uMemoryAddress, I2C_MEMADD_SIZE_16BIT, (uint8_t*)(pbBuffer + uBufOffset), uWriteBytes, 5000);
        
				/* Writing process Error */
				if(cResult != HAL_OK)
						break;
                else
                {
                    /* Wait for the end of the transfer */
                    /*  Before starting a new communication transfer, you need to check the current   
                        state of the peripheral; if it is busy you need to wait for the end of current
                        transfer before starting a new one.
                        For simplicity reasons, this example is just waiting till the end of the 
                        transfer, but application may perform other tasks while transfer operation
                        is ongoing. */
                    while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);

                    /* Check if the EEPROM is ready for a new operation */
                    while (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDRESS, EEPROM_MAX_TRIALS, I2C_XFER_TIMEOUT_MAX) == HAL_TIMEOUT);

                    /* Wait for the end of the transfer */
                    while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);

                    /* Update Remaining bytes and Memory Address values */
                    uRemainBytes -= uWriteBytes;
                    uMemoryAddress += uWriteBytes;
                    uBufOffset += uWriteBytes;
                }
		}

        __enable_irq();
		HAL_FLASHEx_DATAEEPROM_Lock();
		return cResult;
}
#endif // USE_I2C

//----------------------------------------------------------------------------//
/**
  * @brief  
  * @param  
  * @retval None
  */
void CommandInit_GP(CmdProtocol_t* ptCommand)
{
    ptCommand->uLen = 0;
}

BYTE Hex2Val(BYTE ASC)
{
	if (ASC>=0x30 && ASC<=0x39 )
		return (ASC-0x30);
	else if ( ASC>='A' && ASC<='F' )
		return (ASC-'A')+10;

	return 0;		
}

BYTE Hex2Val_B(BYTE ascH,BYTE ascL)
{
	return Hex2Val(ascH)*16 + Hex2Val(ascL);
}

void KEY_ENCODE(void)
{
	char i;
	for (i=0; i<5; i++)
	{
		g_tRegisters.s1.bAPP_KEY[5-i] ^= g_tRegisters.s1.bAPP_KEY[4-i];
	}
}

void KEY_DECODE(void)
{
	char i;

	for(i=1; i<6; i++)
	{
		g_tRegisters.s1.bAPP_KEY[i] ^= g_tRegisters.s1.bAPP_KEY[i-1];
	}
}

void LED_Control(u8 bdata)
{
    u8 bTmpBuf[2] = {0, 0};

    switch(bdata)
    {
    case 0x02:    // STX
        bSTX = true;
        cmdlen = 0;
        break;
    case _CR_:    // CR
        if (cmdlen > 1)
        {
            switch(cmd[0])
            {
            case 'j':    // LED / BUZZER Control <STX>+<J>+CMD+<CR>
            case 'J':
                if(g_tRegisters.s1.bLED_Flag & Bit0) //RS232 control
                {
                    switch(cmd[1])
                    {
                    case '0':    // Return Default
                        phDriver_PWMStop();

                        LedOff();
                        m_cLastLED = 0;
                        break;
                    case '1':    // GREEN LED ON
                        phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_ON_LEVEL);
                        m_cLastLED = LedCheck();
                        break;
                    case '2':    // GREEN LED OFF
                        phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_OFF_LEVEL);
                        m_cLastLED = LedCheck();
                        break;
                    case '3':    // RED LED ON
                        phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_ON_LEVEL);
                        m_cLastLED = LedCheck();
                        break;
                    case '4':    // RED LED OFF
                        phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_OFF_LEVEL);
                        m_cLastLED = LedCheck();
                        break;
                    case '5':    // BUZZER BEEP 1 TIME
                        #if defined(ORG_BUZZ_MODE)
                        control_buz.Count = 1;
                        control_buz.On_Time = 2 * BUZZER_UNIT_MS;
                        control_buz.Off_Time = 1 * BUZZER_UNIT_MS;
                        ulBUZZ_Timeout = (control_buz.On_Time + control_buz.Off_Time) * control_buz.Count;
                        #else   //defined(MULTI_BUZZ_MODE)
                        Multi_buz.Group_Num = 1;
                        Multi_buz.Wave_Group[0].On_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Off_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Count = 1;
                        ulBUZZ_Timeout = (Multi_buz.Wave_Group[0].On_Time + Multi_buz.Wave_Group[0].Off_Time) * Multi_buz.Wave_Group[0].Count;
                        #endif

                        ulBUZZ_Tick = phDriver_GetTick();
                        ulBUZZ_Pulse_Tick = ulBUZZ_Tick;

                        phDriver_PWMStart();
                        bBUZZ_Ctrl = BUZZ_CTRL_EN;

                        m_cLastLED = LedCheck();
                        #if defined(ORG_BUZZ_MODE)
                        LedRestore(0, control_buz.On_Time);
                        #else   //defined(MULTI_BUZZ_MODE)
                        LedRestore(0, Multi_buz.Wave_Group[0].On_Time);
                        #endif
                        bLEDS_Ctrl = LEDS_CTRL_NSYNC;

                        bBUZZ_Step = BUZZ_STEP_ON;
                        bLoopBUZZ = LOOP_ENABLE;
                        break;
                    case '6':     // BUZZER BEEP 3 TIME
                        #if defined(ORG_BUZZ_MODE)
                        control_buz.Count = 3;
                        control_buz.On_Time = 2 * BUZZER_UNIT_MS;
                        control_buz.Off_Time = 1 * BUZZER_UNIT_MS;
                        ulBUZZ_Timeout = (control_buz.On_Time + control_buz.Off_Time) * control_buz.Count;
                        #else   //defined(MULTI_BUZZ_MODE)
                        Multi_buz.Group_Num = 1;
                        Multi_buz.Wave_Group[0].On_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Off_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Count = 3;
                        ulBUZZ_Timeout = (Multi_buz.Wave_Group[0].On_Time + Multi_buz.Wave_Group[0].Off_Time) * Multi_buz.Wave_Group[0].Count;
                        #endif

                        ulBUZZ_Tick = phDriver_GetTick();
                        ulBUZZ_Pulse_Tick = ulBUZZ_Tick;

                        phDriver_PWMStart();
                        bBUZZ_Ctrl = BUZZ_CTRL_EN;

                        m_cLastLED = LedCheck();
                        #if defined(ORG_BUZZ_MODE)
                        LedRestore(0, control_buz.On_Time);
                        #else   //defined(MULTI_BUZZ_MODE)
                        LedRestore(0, Multi_buz.Wave_Group[0].On_Time);
                        #endif
                        bLEDS_Ctrl = LEDS_CTRL_NSYNC;

                        bBUZZ_Step = BUZZ_STEP_ON;
                        bLoopBUZZ = LOOP_ENABLE;
                        break;
                    case '7':     // GLED ON and Beep 1 TIME
                        #if defined(ORG_BUZZ_MODE)
                        control_buz.Count = 1;
                        control_buz.On_Time = 2 * BUZZER_UNIT_MS;
                        control_buz.Off_Time = 1 * BUZZER_UNIT_MS;
                        ulBUZZ_Timeout = (control_buz.On_Time + control_buz.Off_Time) * control_buz.Count;
                        #else   //defined(MULTI_BUZZ_MODE)
                        Multi_buz.Group_Num = 1;
                        Multi_buz.Wave_Group[0].On_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Off_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Count = 1;
                        ulBUZZ_Timeout = (Multi_buz.Wave_Group[0].On_Time + Multi_buz.Wave_Group[0].Off_Time) * Multi_buz.Wave_Group[0].Count;
                        #endif

                        ulBUZZ_Tick = phDriver_GetTick();
                        ulBUZZ_Pulse_Tick = ulBUZZ_Tick;

                        phDriver_PWMStart();
                        bBUZZ_Ctrl = BUZZ_CTRL_EN;

                        m_cLastLED = Bit0;
                        #if defined(ORG_BUZZ_MODE)
                        LedRestore(0, control_buz.On_Time);
                        #else   //defined(MULTI_BUZZ_MODE)
                        LedRestore(0, Multi_buz.Wave_Group[0].On_Time);
                        #endif
                        bLEDS_Ctrl = LEDS_CTRL_NSYNC;

                        bBUZZ_Step = BUZZ_STEP_ON;
                        bLoopBUZZ = LOOP_ENABLE;
                        break;
                    case '8':    // RLED ON and Beep 3 TIME
                        #if defined(ORG_BUZZ_MODE)
                        control_buz.Count = 3;
                        control_buz.On_Time = 2 * BUZZER_UNIT_MS;
                        control_buz.Off_Time = 1 * BUZZER_UNIT_MS;
                        ulBUZZ_Timeout = (control_buz.On_Time + control_buz.Off_Time) * control_buz.Count;
                        #else   //defined(MULTI_BUZZ_MODE)
                        Multi_buz.Group_Num = 1;
                        Multi_buz.Wave_Group[0].On_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Off_Time = 2 * BUZZER_UNIT_MS;
                        Multi_buz.Wave_Group[0].Count = 3;
                        ulBUZZ_Timeout = (Multi_buz.Wave_Group[0].On_Time + Multi_buz.Wave_Group[0].Off_Time) * Multi_buz.Wave_Group[0].Count;
                        #endif

                        ulBUZZ_Tick = phDriver_GetTick();
                        ulBUZZ_Pulse_Tick = ulBUZZ_Tick;

                        phDriver_PWMStart();
                        bBUZZ_Ctrl = BUZZ_CTRL_EN;

                        m_cLastLED = Bit1;
                        #if defined(ORG_BUZZ_MODE)
                        LedRestore(0, control_buz.On_Time);
                        #else   //defined(MULTI_BUZZ_MODE)
                        LedRestore(0, Multi_buz.Wave_Group[0].On_Time);
                        #endif
                        bLEDS_Ctrl = LEDS_CTRL_NSYNC;

                        bBUZZ_Step = BUZZ_STEP_ON;
                        bLoopBUZZ = LOOP_ENABLE;
                        break;
                    case '9':    // Orange ON
                        phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_ON_LEVEL);
                        phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_ON_LEVEL);
                        m_cLastLED = LedCheck();
                        break;
                    default:
                        bTmpBuf[0] = CMD_REPLY_NAK;
                        AsciiPacker_GP(bTmpBuf, 1);
                        return;
                    }
                }
                bTmpBuf[0] = CMD_REPLY_ACK;
                AsciiPacker_GP(bTmpBuf, 1);
                break;

            default:
                bTmpBuf[0] = CMD_REPLY_NAK;
                AsciiPacker_GP(bTmpBuf, 1);
                break;
            }
        }
        bSTX = false;
        cmdlen = 0;
        cmd_parser.uLen = 0;
        break;
    default:
        if (bSTX && (cmdlen < sizeof(cmd)))
        {
            cmd[cmdlen] = bdata;
            cmdlen++;
        }
        break;
    }
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void CommandParser(u8 bData)
{
    u8 bErrorMsg;

    if((cmd_parser.uLen == 0) && (bGnetPlusMode == 0))
    {
        if(bData == CMD_GP_HEADER_SOH)
        {
            bGnetPlusMode = 0;
            cmd_parser.bBuffer[cmd_parser.uLen++] = bData;
        }
        else if(bData == CMD_GP_HEADER_COLON)
        {
            bGnetPlusMode = 1;
            ASCII_parser.flag = 0;
        }
        else if (bData != _LF_)
        {
             LED_Control(bData);
        }
    }
    else
    {
        if(bGnetPlusMode == 0)
        {
            if(cmd_parser.uLen != 0)
            {
                cmd_parser.bBuffer[cmd_parser.uLen++] = bData;

                if(cmd_parser.uLen >= CMD_GP_BEFORE_DATA_SIZE)
                {
                    if(cmd_parser.uLen >= (cmd_parser.tFormat.bLength + CMD_GP_FULL_NO_DATA_LEN))
                    {
                        // check CRC
                        if(CRC16_Verify_GP(&cmd_parser.tFormat.bAddress, (cmd_parser.tFormat.bLength + (CMD_GP_FULL_NO_DATA_LEN - CMD_GP_HEADER_LEN))) == 0)
                        {
                            if((m_bHalt == false) || (cmd_parser.tFormat.bCmdCode == GNET_FUNC_HALT))
                            {
                                CommandExecute_GP();
                            }
                        }
                        else
                        {
                            bErrorMsg = GNET_ERROR_CRCERROR;
                            ResponseNakData_GP(&bErrorMsg, 1);
                        }

                        cmd_parser.uLen = 0;
                    }
                }
            }
        }
        else
        {
            if(isxdigit(bData))	
            {
                if(g_tRegisters.s1.bDeviceId == 0)
                {
                    AsciiPacker_GP(&bData, 1);
                }

                if((ASCII_parser.flag & 0x01))
                {
                    ASCII_parser.bAscL = bData;
                    cmd_parser.bBuffer[cmd_parser.uLen++] = Hex2Val_B(ASCII_parser.bAscH, ASCII_parser.bAscL);
                }
                else
                    ASCII_parser.bAscH = bData;

                ASCII_parser.flag = ASCII_parser.flag ^ 0x01;
            }
            else if(bData == _CR_)	// End Of ASCII Qurey
            {
                if(g_tRegisters.s1.bDeviceId == 0)
                {
                    AsciiPacker_GP(&bData, 1);
                }

                if((g_tRegisters.s1.bDeviceId == cmd_parser.tFormat.bAddress) || (cmd_parser.tFormat.bAddress == 0))
                {
                    if((m_bHalt == false) || (cmd_parser.tFormat.bCmdCode == GNET_FUNC_HALT))
                    {
                        CommandExecute_GP();
                    }

                    cmd_parser.uLen = 0;
                }
                else
                {
                    cmd_parser.uLen = 0;
                }

                bGnetPlusMode = 0;
            }
            else
            {
                cmd_parser.uLen = 0;
                bGnetPlusMode = 0;
            }
        }
    }
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void CommandPacker_GP(u8 bRepCode, const u8* pbArgument, u8 bArgumentLength)
{
    // Clear previous rep_packer data
    memset((void*) &cmd_parser, 0x00, sizeof(cmd_parser));

    if(pbArgument == NULL)
        bArgumentLength = 0;

    cmd_parser.tFormat.bPreamble = CMD_GP_HEADER_SOH;
	cmd_parser.tFormat.bAddress = g_tRegisters.s1.bDeviceId;
    cmd_parser.tFormat.bCmdCode = bRepCode;
    cmd_parser.tFormat.bLength = bArgumentLength;

    if(bArgumentLength > 0)
        memcpy(cmd_parser.tFormat.bPayload, pbArgument, bArgumentLength);

    CRC16_Append_GP(&cmd_parser.tFormat.bAddress, (bArgumentLength + CMD_GP_ADDR_LENGTH_LEN));

    cmd_parser.uLen = REP_GP_FULL_NO_DATA_LEN + bArgumentLength;
    
    if(g_CmdSource == CMD_SOURCE_RS232)
    {
        RS232_Output(cmd_parser.bBuffer, cmd_parser.uLen);
    }
    else if(g_CmdSource == CMD_SOURCE_RS485)
    {
        RS485_Output(cmd_parser.bBuffer, cmd_parser.uLen);
    }
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void AsciiPacker_GP(const u8* pbArgument, u8 bArgumentLength)
{
    memset((void*) &cmd_parser, 0x00, sizeof(cmd_parser));
    
    cmd_parser.uLen = bArgumentLength;
    memcpy(cmd_parser.bBuffer, pbArgument, bArgumentLength);

    if(g_CmdSource == CMD_SOURCE_RS232)
    {
        RS232_Output(cmd_parser.bBuffer, cmd_parser.uLen);
    }
    else if(g_CmdSource == CMD_SOURCE_RS485)
    {
        RS485_Output(cmd_parser.bBuffer, cmd_parser.uLen);
    }
}

//----------------------------------------------------------------------------//
/**
  * @brief  
  * @param  None
  * @retval None
  */
void ResponseAck_GP(void)
{
    CommandPacker_GP(CMD_REPLY_ACK, NULL, 0);
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void ResponseAckData_GP(const u8* pbData, u8 bDataLength)
{
    CommandPacker_GP(CMD_REPLY_ACK, pbData, bDataLength);
}

/**
  * @brief  
  * @param  None
  * @retval None
  */
void ResponseNak_GP(void)
{
    CommandPacker_GP(CMD_REPLY_NAK, NULL, 0);
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void ResponseNakData_GP(const u8* pbData, u8 bDataLength)
{
    CommandPacker_GP(CMD_REPLY_NAK, pbData, bDataLength);
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void ResponseEvent_GP(const u8* pbData, u8 bDataLength)
{
    CommandPacker_GP(CMD_REPLY_EVENT, pbData, bDataLength);
}

/**
  * @brief  
  * @param  
  * @retval None
  */
void ResponseData_GP(u8 bCmdCode, const u8* pbData, u8 bDataLength)
{
    CommandPacker_GP(bCmdCode, pbData, bDataLength);
}

u8 Halt(bool bFlag, u8 nExclude)
{
    if (g_tRegisters.s1.bDeviceId == nExclude && nExclude!=0)
        bFlag = !bFlag;

    m_bHalt = false;
    m_bHalt = bFlag;
    
    return bFlag;
}

///////////////////////////////////////
// RC500 API Function: Make AccessBit
// CB0 : Block 0 Access control
// CB1 : Block 1 Access control
// CB2 : Block 2 Access control
// CB3 : Block 3 Access control
///////////////////////////////////////
void MF5_API_MakeAccessBit(BYTE CB0, BYTE CB1, BYTE CB2, BYTE CB3, BYTE* AB)
{
	BYTE C1, C2, C3;
	
	memset(AB, 0, 4);	// initialize Access Bit
	
	C1 = (CB0 & 0x01) | ((CB1 & 0x01)<<1) | ((CB2 & 0x01)<<2) | ((CB3 & 0x01)<<3);
	C2 = ((CB0 & 0x02)>>1) | (CB1 & 0x02) | ((CB2 & 0x02)<<1) | ((CB3 & 0x02)<<2);
	C3 = ((CB0 & 0x04)>>2) | ((CB1 & 0x04)>>1) | (CB2 & 0x04) | ((CB3 & 0x04)<<1);
	
	AB[0] = ((~C1) & 0x0F) | (~(C2<<4) & 0xF0);
	AB[1] = (~C3 & 0x0F) | (C1<<4);
	AB[2] =  C2 | (C3<<4);
	AB[3] = 0x69; // MAD default Value
}

void MF5_API_MakeAccessBitEx(BYTE CB0, BYTE CB1, BYTE CB2, BYTE CB3, BYTE GPB, BYTE* AB)
{
	BYTE C1, C2, C3;

	memset(AB, 0, 4);	// initialize Access Bit
	
	C1 = (CB0 & 0x01) | ((CB1 & 0x01)<<1) | ((CB2 & 0x01)<<2) | ((CB3 & 0x01)<<3);
	C2 = ((CB0 & 0x02)>>1) | (CB1 & 0x02) | ((CB2 & 0x02)<<1) | ((CB3 & 0x02)<<2);
	C3 = ((CB0 & 0x04)>>2) | ((CB1 & 0x04)>>1) | (CB2 & 0x04) | ((CB3 & 0x04)<<1);
	
	AB[0] = ((~C1) & 0x0F) | (~(C2<<4) & 0xF0);
	AB[1] = (~C3 & 0x0F) | (C1<<4);
	AB[2] =  C2 | (C3<<4);
	AB[3] = GPB; 			// MAD-GPB
}

u8 GNET_SetRegister(u16 Addr, u8* buf, u8 len)
{
    u8 RetryCount = 0;
    u8 bResult = GNET_ERROR_UNKNOW;
    u16 wLen, wPartNum;

	if(Addr == PBL_SETTING)
	{
		wLen = (len>sizeof(g_tRegisters.s1)) ? sizeof(g_tRegisters.s1) : len;
		memcpy(g_tRegisters.bRegisters+2, buf, wLen);			
		KEY_ENCODE();

        wLen = sizeof(g_tRegisters.s1);
        g_tRegisters.s1.wCheckCRC = CRC16_Calculate_GP(g_tRegisters.bRegisters+2, wLen-2, DEF_CRC_PRESET);

        g_tRegisters.s1.dfLength = _min_(MAX_USER_DATA, g_tRegisters.s1.dfLength);

        do
        {
            bResult = FLASH_EEPROM_WriteBytes(g_tRegisters.bRegisters, CONFIG_PAGE_ADDR, 0, &wLen);
            RetryCount++;
        }
        while((bResult != GNET_OK)&&(RetryCount < 5)); 
    
		if(bResult != GNET_OK)
            bResult = GNET_ERROR_UNKNOW;

		if(Addr == PBL_SETTING)
			KEY_DECODE();

        if((g_tRegisters.s1.bLED_Flag & Bit1) == 0)
            config_di(DI_ACT_HIGH_LEVEL, DI_ACT_HIGH_LEVEL);
        else
            config_di(DI_ACT_LOW_LEVEL, DI_ACT_LOW_LEVEL);
	}
    else if(Addr == PRV_FEATURE)
    {
        if(len == 5)
        {
            wPartNum = (buf[0] * 256) + buf[1];

            // Check valid Part Number
            if(wPartNum < PART_NUM_UNKNOWN)
            {
                bResult = GNET_OK;
                wLen = sizeof(g_tPrvSettings.Settings.bPartName);

                switch(wPartNum)
                {
                  #ifdef STANDARD_DF7X0A
                  case PART_NUM_DF700A_00:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF700A_00, wLen);
                    break;

                  case PART_NUM_DF710A_00:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF710A_00, wLen);
                    break;

                  case PART_NUM_DF710A_U2:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF710A_U2, wLen);
                    break;

                  #elif defined(STANDARD_DF750_760A)
                  case PART_NUM_DF750AK_00:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF750AK_00, wLen);
                    break;

                  case PART_NUM_DF750A_00:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF750A_00, wLen);
                    break;

                  case PART_NUM_DF760AK_00:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF760AK_00, wLen);
                    break;

                  case PART_NUM_DF760A_00:

                    memcpy(g_tPrvSettings.Settings.bPartName, sName_DF760A_00, wLen);
                    break;
                  #endif

                  default:

                    bResult = GNET_ERROR_NOSUPPORT;
                    break;
                }
            }
            else
                bResult = GNET_ERROR_ILLEGAL;

            // Program Product Part Name String
            if(bResult == GNET_OK)
            {
                wLen = sizeof(g_tPrvSettings.Settings);

                do
                {
                    bResult = FLASH_EEPROM_WriteBytes(g_tPrvSettings.bData, FEATURE_PAGE_ADDR, 0, &wLen);
                    RetryCount++;
                }
                while((bResult != GNET_OK) && (RetryCount < 5));

                if(bResult != GNET_OK)
                    bResult = GNET_ERROR_UNKNOW;
            }
        }
        else
            bResult = GNET_ERROR_ILLEGAL;
    }
    else if(Addr == PRV_SERLNUM)
    {
        if(len == 5)
        {
            wLen = sizeof(g_tPrvSettings.Settings.bSerialNum);
            memcpy(g_tPrvSettings.Settings.bSerialNum, buf, wLen);			

            wLen = sizeof(g_tPrvSettings.Settings);

            do
            {
                bResult = FLASH_EEPROM_WriteBytes(g_tPrvSettings.bData, FEATURE_PAGE_ADDR, 0, &wLen);
                RetryCount++;
            }
            while((bResult != GNET_OK) && (RetryCount < 5));

            if(bResult != GNET_OK)
                bResult = GNET_ERROR_UNKNOW;
        }
        else
            bResult = GNET_ERROR_ILLEGAL;
    }
	else 
        bResult = GNET_ERROR_ILLEGAL;

    return bResult;
}

/**
  * @brief  
  * @param  None
  * @retval None
  */
void CommandExecute_GP(void)
{
    u8 cResult = GNET_ERROR_UNKNOW;
    u8 bData_Buf[255];
    u16 uData_Len = 0;

    switch(cmd_parser.tFormat.bCmdCode)
    {
    case GNET_FUNC_POLLING:
        cResult = GNET_OK;
        break;

    case GNET_FUNC_VERSION:
        memcpy(bData_Buf, sFWVer, sizeof(sFWVer));
        uData_Len = (sizeof(sFWVer) - 1);
        cResult = GNET_OK;
        break;

    case GNET_FUNC_SETADDR:
        if(cmd_parser.tFormat.bLength == 1)
        {
            g_tRegisters.s1.bDeviceId = cmd_parser.tFormat.bPayload[0];
            cResult = GNET_OK;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
        break;

    case GNET_FUNC_CLASS:
        if ( m_nCurrentAddr>0 )
        {
            memcpy(bData_Buf, szClass, sizeof(szClass));
            uData_Len = (sizeof(szClass)-1);
            cResult = GNET_OK;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
        
        break;

    case GNET_FUNC_GET_REGISTER:
        if(cmd_parser.tFormat.bLength == 2)
        {
            u16 Addr;
			Addr = (cmd_parser.tFormat.bPayload[0] * 256) + cmd_parser.tFormat.bPayload[1];

            if(Addr == PRV_FEATURE)
            {
                bData_Buf[0] = (g_wPartNum >> 8) & 0xFF;
                bData_Buf[1] =  g_wPartNum & 0xFF;
                bData_Buf[2] = 0xFF;
                bData_Buf[3] = 0xFF;
                bData_Buf[4] = 0xFF;
                uData_Len = 5;

                cResult = GNET_OK;
            }
            else if(Addr == PRV_SERLNUM)
            {
                memcpy(bData_Buf, g_tPrvSettings.Settings.bSerialNum, sizeof(g_tPrvSettings.Settings.bSerialNum));
                uData_Len = sizeof(g_tPrvSettings.Settings.bSerialNum);

                cResult = GNET_OK;
            }
            else
                cResult = GNET_ERROR_ILLEGAL;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;

    case GNET_FUNC_SET_REGISTER:
        if(cmd_parser.tFormat.bLength > 2)
        {
            u16 Addr;
			Addr = (cmd_parser.tFormat.bPayload[0] * 256) + cmd_parser.tFormat.bPayload[1];
			cResult = GNET_SetRegister(Addr, cmd_parser.tFormat.bPayload + 2, cmd_parser.tFormat.bLength - 2);

            // Added to reboot reader by itself
            ResponseAck_GP();
            phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 100, NULL);

            if((Addr == PBL_SETTING) || (Addr == PRV_FEATURE))
            {
                NVIC_SystemReset();

                cResult = GNET_ERROR_UNKNOW;				// unknown error happened if code executed here
            }
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;
    
    case GNET_FUNC_GET_NODE:
        cResult = GNET_ERROR_DONT_RESPONSE;
    break;

    case GNET_FUNC_ECHO:
        if(cmd_parser.tFormat.bLength > 0)
        {
            memcpy(bData_Buf, &cmd_parser.tFormat.bPayload, cmd_parser.tFormat.bLength);
            uData_Len = cmd_parser.tFormat.bLength;
            
            cResult = GNET_OK;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;

    case GNET_FUNC_DEBUG:
    break;

    case GNET_FUNC_RESET:
        if(cmd_parser.tFormat.bLength == 0)
        {
            ResponseAck_GP();
            phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 100, NULL);

            NVIC_SystemReset();

            cResult = GNET_ERROR_UNKNOW;				// unknown error happened if code executed here
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;

    case GNET_FUNC_GOTOISP:
        if(cmd_parser.tFormat.bLength == 0)
        {
            #ifdef  USE_BOOT_LOADER
            uint8_t FlashStatus;
            uint8_t bCount = 0;
            uint8_t bJumpFlag = 0xFF;//0x55;

            do
            {
                FlashStatus = phDriver_FLASHWrite(PAR_START_ADDRESS, &bJumpFlag, 0, 1);

                bCount++;
            } while ((FlashStatus != PH_DRIVER_SUCCESS) && (bCount < 5));
            #endif

            ResponseAck_GP();
            phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 100, NULL);

            NVIC_SystemReset();

            cResult = GNET_ERROR_UNKNOW;				// unknown error happened if code executed here
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;

    case GNET_FUNC_GNET_OFF:
        if(cmd_parser.tFormat.bLength == 0)
        {
            bGnetPlusMode = 0;

            cResult = GNET_OK;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;

    #if defined(GNET_MIFARE_CMD)
    //Mifare Classic
    case GNET_FUNC_MF_REQUEST:
        {
            bAutoMode = false;
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
    break;

    case GNET_FUNC_MF_ANTICOLL:
    case GNET_FUNC_MF_SELECT:
    case GNET_FUNC_MF_AUTHENTICATION:
    case GNET_FUNC_MF_READ_BLOCK:
    case GNET_FUNC_MF_WRITE_BLOCK:
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
    break;

    case GNET_FUNC_MF_VALUE:
        if(cmd_parser.tFormat.bLength == 6)
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
    break;

    case GNET_FUNC_MF_READ_VALUE:
        if(cmd_parser.tFormat.bLength != 5)
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
    break;

    case GNET_FUNC_MF_FORMAT_VALUE:
    case GNET_FUNC_MF_ACCESS:
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
    break;

    case GNET_FUNC_MF_HALT:
        if(cmd_parser.tFormat.bLength == 0)
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;

    case GNET_FUNC_SAVE_KEY:
        if(cmd_parser.tFormat.bLength == 8)
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
        
		break;
    case GNET_FUNC_GET_ID2:
    case GNET_FUNC_GET_ACCESS:
    case GNET_FUNC_AUTHENTICATION_FOR_KEY:
    case GNET_FUNC_REQUEST_CARD:
        {
            bCmdTask = cmd_parser.tFormat.bCmdCode;
            cResult = GNET_ERROR_DONT_RESPONSE;
        }
    break;

    case GNET_FUNC_AUTO_MODE:
        if(cmd_parser.tFormat.bLength <= 1)
        {
            if(cmd_parser.tFormat.bLength == 0)
                bAutoMode = true;
            else
                bAutoMode=((cmd_parser.tFormat.bPayload[0]==0) ? false: true);
            
            bCmdTask = 0;
            cResult = GNET_OK;
            bData_Buf[uData_Len++] = bAutoMode;
        }
        else
            cResult = GNET_ERROR_ILLEGAL;
    break;
    #endif  // defined(GNET_MIFARE_CMD)

    default:
        cResult = GNET_ERROR_NOSUPPORT;
    }

    if(cResult != GNET_ERROR_DONT_RESPONSE)
    {
        if(cResult == GNET_OK)
            ResponseAckData_GP(bData_Buf, uData_Len);
        else
            ResponseNakData_GP((u8*)&cResult, 1);
    }
}

