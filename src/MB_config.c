/**
  ******************************************************************************
  * @file    MB_config.c
  * @author  
  * @version V1.0.0
  * @date    
  * @brief   Gen2 configuration
  ******************************************************************************
	*/

/* Includes ------------------------------------------------------------------*/
#include "MB_config.h"
#include "main.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
HfRegisters_t g_tRegisters;
PrvSettings_t g_tPrvSettings;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/

/**
  * @brief  This union/structure is for registers' default settings
  * @param  None
  * @retval None
  */
volatile const HfRegisters_t DEFALUT_REGISTERS =
{
    {
        #if 0               // Could be replaced with wCheckCRC for DF750AK
        .Head = 0xAA55,
        #else
        .wCheckCRC = 0,
        #endif

        .bDeviceId = 0x00,
        .bSector = 0x00,
        .wAID.wInt = 0x0000,
        .bOffset = 0,
        .bLength = 0,
        .bAPP_KEY = {0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0},
        .bEnableUID = 2,
        .bInterface = 0,

        .bW_Flag = 0x00,
        .bW_Bits = 26,

        .bT_Flag = 0,
        .bT_Format = 0,
        .bT_Byte = 0,

        .bS_Flag = (Bit1 + Bit3 + Bit4 + Bit6),
        .bS_Baud = (19200/2400),
        .bS_Header = 0,
        .bS_Tailer = 0,

        .bLED_Flag = 0x00,
        .bLED_Normal = 0x03,
        .bLED_Valid =  0x00,
        .bLED_Invalid = 0x00,
        .bLED_External = 0x00,

        .bEncryptValue = 0x00,
        .bContinue = 0x00,

        #ifdef STANDARD_DF750_760A
        #else
        .S_InputDataType = 0x00,
        .CFG_FLlg = 0x00,
        #endif

        .bSite_Code = 0,
        .bKeyPad_Flag = 0x00,

        .bCfgKey = {0x10, 0x27, 0xC6, 0x4D, 0x4A, 0x94},
    }
};


