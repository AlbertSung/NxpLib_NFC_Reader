/**
  ******************************************************************************
  * @file    MB_config.h
  * @author  
  * @version V1.0.0
  * @date    
  * @brief   Header for gen2_config.c module
  ******************************************************************************
	*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MB_CONFIG_H
#define __MB_CONFIG_H

#ifdef __cplusplus
 extern "C" {
#endif

	 
/* Includes ------------------------------------------------------------------*/
#include "ams_types.h"
#include "CardProcess.h"
#include "Porting_SAMD21.h"


/* Exported definitions ------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef union
{
    struct
    {
        #if 0                    // Could be replaced with wCheckCRC for DF750AK
        u16   Head;              // Struct Check Head (0xAA55)

        #else
        u16   wCheckCRC;         // Used to compare with RID_ADDR which is defined in current protocol

        #endif

        u8    bDeviceId;         // Reader ID

        u8    bSector;           // Non-MAD Sector
        UINT_T   wAID;           // Application ID (for MAD)

        u8    bOffset;           // Offset
        u8    bLength;           // Data Length, If Length==0, Search by User-Data

        u8    bAPP_KEY[6];       // DES Key

        u8    bEnableUID;        // Enable UID (for Unknow/Error Card), 0=Disable , 1=Enable, 2=UID Only, b7:0=KEY_A, 1=KEY_B

        u8    bInterface;        // Interface: 0=Wiegand, 1=TK2, 2=Serial,

        u8    bW_Flag;           // Wiegand Flag (b0:0=MSB 1=LSB first; b1:0=with parity/1=without parity; b2:0=ISO/1=Non-ISO; b7=1=Include RID)
        u8    bW_Bits;           // Wiegand Bits

        u8    bT_Flag;           // TK2 Flag (b0: 0=MSB 1=LSB for output; b7: 1=Include RID, b1: 0=MSB, 1=LSB for input)
        u8    bT_Format;         // 0: Binary (Default), 1=Dec, 2=BCD
        u8    bT_Byte;           // TK2 Length (Byte)

        u8    bS_Flag;           // Serial Flag (b0: 0=LSB 1=MSB first; b7=Include RID, b6=Head, b5=LEN b4=CR b3=LF b2=Tailer, b1=1=HexString)
        u8    bS_Baud;           // Baudrate 0=Default
        u8    bS_Header;         // Serial Data Header
        u8    bS_Tailer;         // Serial Data Tailer

        u8    bLED_Flag;         // LED Flag 0 (b0=Control by RS232, b1:00= High, 01=Low; b2=1=Disable, b3=1=Anytime, b7=2 wires mode)
        u8    bLED_Normal;       // LED Normal  (b0=Green, b1=Red)
        u8    bLED_Valid;        // LED Valid   (b0=Green, b1=Red, b4~7:Beep)
        u8    bLED_Invalid;      // LED Invalid (b0=Green, b1=Red, b4~7:Beep)
        u8    bLED_External;     // LED External(b0=Green, b1=Red, b4~7:Beep)

        u8    bEncryptValue;     // 30: EncryptValue
        u8    bContinue;         // 31: Output method 1=Continue, 0=One Time (Default)

        #ifdef STANDARD_DF750_760A
        u8    blueLED;           // 32: 2007/02/05, Blue LED control flag, b0: 1=ON, 0=OFF; b7=Finger Scan Success Beep 1=On, 0=Off
        u8    Alignment;         // 33: 填補用, 使struct大小為偶數 , b0:Manager Card 1=Enable; b4~7:Relay Control Period Time (second), b1=Card Only Enable
                                 // b2=1 buffering, b3=1 enable configure card
        #else
        u8    S_InputDataType;   // 32: 0: Binary(Default) 1:String (ended with 0x00)
        u8    CFG_FLlg;          // 33: b3: Config card function 0=Disable, 1=Enable

        #endif

        // for DF700 +++
        u8    bSite_Code;        // 34: 0~255
        u8    bKeyPad_Flag;      // 35: 0: WDG 4bit, 1:WDG 6bit, 2:WDG 8bit, 3:ASCII Hex Digits, 4:Buffering 26 bits with Site Code, b7=1 have to check, 0=auto
        u8    bLED_Value[2];     // 36: LED Brightness

        u8    dfAID[4];          // 38:
        u8    dfKeyNo;           // 42:
        u8    dfFileID;          // 43:
        u8    dfOffset[2];       // 44:
        u16   dfLength;          // 46:
        u8    dfKeyValue[16];    // 48:
        u8    dfDataType;        // 64: 0: bin, 1:BCD, 2:Visable Hex, 3: Visable DEC, 7:0=Disable Mifare, 6:1=Disable Desfire

        u8    bCfgKey[6];        // 65: configure card's KEY
        u8    bWp_Len;           // 71: wiegand preamble bits length
        u8    bWp_Bits;          // 72: wiegand preamble bits
        // for DF700 ---

        u8    ncsSN[4];          // 73:
        u8    bBaudrate;         // 77: 2009/2/24, baudrate
        u8    bAlive;            // 78: reserved

        // for DF700 +++
        u8    dfOptions;         // 79: 0~3: 0=Auto, 1=AES, 2=3K3DES, 3=DES / 4~7: 0=Auto, 1=Plain, 2=MACing, 3=Full
        u8    dfFileType;        // 80: 0xFF: Auto
        // for DF700 ---

        u8    bControl1;         // 81: b0: IR-Sensor 1=Enable 0=Disable
                                 //     b1: G-Sensor 1=Enable 0=Disable
        u8    bControl2;         // 82: reserved
        u8    bHeartbeatTime;    // 83: Heartbeat Event Time, Unit: 0.5 minute, 0 is disable

        u8    bReservedAll[62];  // 84-145: not used by DF700-00 project and added only for SW alignment

        u8    bS_CSN_Format;     // 146: Serial CSN format: 0=Binary(default), 1=Visible Hex, 2=Decimal

    } s1;
    u8 bRegisters[255];

} HfRegisters_t;

typedef union
{
    struct
    {
        u8    bPartName[12];     // 0: Product name string converted from Part Number ID (Including Null character)

        u8    bSerialNum[5];     // 12: Product serial number

    } Settings;
    u8 bData[255];

} PrvSettings_t;

/* Exported constants --------------------------------------------------------*/


/* Exported variables --------------------------------------------------------*/
extern HfRegisters_t g_tRegisters;
extern PrvSettings_t g_tPrvSettings;

extern volatile const HfRegisters_t DEFALUT_REGISTERS;
extern volatile const PrvSettings_t DEFAULT_PRV_SETTINGS;


#ifdef __cplusplus
}
#endif

#endif /* __MB_CONFIG_H */



