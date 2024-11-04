/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ams_types.h"
#include "MB_config.h"

#include "Porting_SAMD21.h"

/* Defined constants ---------------------------------------------------------*/

#define BUZZER_UNIT_MS		        100
#define BUZZER_UNIT_S		        1000

#define GET_MIN(a,b) ((a<b)?(a):(b))

#define CMD_GP_MAX_DATA_SIZE					255

#define _CR_                    		        0x0D
#define _LF_		                            0x0A

#define CMD_GP_HEADER_SOH						0x01
#define CMD_GP_HEADER_COLON						0x3A

#define CMD_GP_HEADER_LEN						1
#define CMD_GP_ADDRESS_LEN						1
#define CMD_GP_OPCODE_LEN						1
#define CMD_GP_LENGTH_LEN						1
#define CMD_GP_CRC16_LEN						2

#define CMD_GP_BEFORE_DATA_SIZE			(CMD_GP_HEADER_LEN + CMD_GP_ADDRESS_LEN + CMD_GP_OPCODE_LEN + CMD_GP_LENGTH_LEN)	// 4
#define CMD_GP_ADDR_LENGTH_LEN			(CMD_GP_ADDRESS_LEN + CMD_GP_OPCODE_LEN + CMD_GP_LENGTH_LEN)		// 3
#define CMD_GP_FULL_NO_DATA_LEN			(CMD_GP_HEADER_LEN + CMD_GP_ADDR_LENGTH_LEN + CMD_GP_CRC16_LEN)		// 6

#define REP_GP_MAX_DATA_SIZE					255

#define REP_GP_HEADER_LEN						1
#define REP_GP_ADDRESS_LEN						1
#define REP_GP_OPCODE_LEN						1
#define REP_GP_LENGTH_LEN						1
#define REP_GP_CRC16_LEN						2

#define REP_GP_ADDR_LENGTH_LEN			(REP_GP_ADDRESS_LEN + REP_GP_OPCODE_LEN + REP_GP_LENGTH_LEN)		// 3
#define REP_GP_FULL_NO_DATA_LEN			(REP_GP_HEADER_LEN + REP_GP_ADDR_LENGTH_LEN + REP_GP_CRC16_LEN)		// 6

#define DEF_CRC_PRESET            0xFFFF
#define DEF_CRC_POLYNOM           0xA001

#define CMD_REPLY_ACK            	0x06		// Success
#define CMD_REPLY_NAK            	0x15		// Error
#define CMD_REPLY_EVENT				0x12		// Event

#define MAX_NODE  20
#define MAX_BUF   16

#define I2C_XFER_TIMEOUT_MAX    		300//800
/* Maximum number of trials for HAL_I2C_IsDeviceReady() function */
#define EEPROM_MAX_TRIALS       		300//800

#define EEPROM_ADDRESS							0xA2			// 1010 0 A1 A0 R/W
#define EEPROM_PAGESIZE							128				// RF EEPROM ANT7-M24LR used

// Setting codes for Public/Private settings
#define PBL_SETTING         0x0002
#define PRV_FEATURE         0x0102
#define PRV_SERLNUM         0x0103

#define BROADCAST_ADDR      0x80

/* Exported constants --------------------------------------------------------*/
// GnetPlus Command Set
typedef enum
{
    // Common
    GNET_FUNC_POLLING               = 0x00,
    GNET_FUNC_VERSION               = 0x01,

    GNET_FUNC_SETADDR               = 0x02,
    GNET_FUNC_LOGIN                 = 0x03,
    GNET_FUNC_LOGOUT                = 0x04,
    GNET_FUNC_SET_PASSWORD          = 0x05,

    GNET_FUNC_CLASS                 = 0x06,

    GNET_FUNC_SET_DATETIME          = 0x07,
    GNET_FUNC_GET_DATETIME          = 0x08,
    GNET_FUNC_GET_REGISTER          = 0x09,
    GNET_FUNC_SET_REGISTER          = 0x0A,

    GNET_FUNC_GET_COUNT             = 0x0B,
    GNET_FUNC_GET_FIRST             = 0x0C,
    GNET_FUNC_GET_NEXT              = 0x0D,
    GNET_FUNC_ERASE                 = 0x0E,
    GNET_FUNC_ADD                   = 0x0F,
    GNET_FUNC_RECOVER               = 0x10,

    GNET_FUNC_DO                    = 0x11,
    GNET_FUNC_DI                    = 0x12,

    GNET_FUNC_AI                    = 0x13,
    GNET_FUNC_TEMP                  = 0x14,
    GNET_FUNC_GET_NODE              = 0x15,     // for buffering node
    GNET_FUNC_GET_SN                = 0x16,     // get serial number

    GNET_FUNC_HALT                  = 0x17,
    GNET_FUNC_BUZZER                = 0x18,
    GNET_FUNC_AUTO                  = 0x19,

    GNET_FUNC_GET_TIMEADJ           = 0x1A,
    GNET_FUNC_ECHO                  = 0x1B,
    GNET_FUNC_SET_TIMEADJ           = 0x1C,

    GNET_FUNC_DEBUG                 = 0x1D,
    GNET_FUNC_RESET                 = 0x1E,
    GNET_FUNC_GOTOISP               = 0x1F,

    GNET_FUNC_GNET_OFF              = 0x20,

    #if defined(GNET_MIFARE_CMD)
    // Mifare
    GNET_FUNC_MF_REQUEST            = 0x20,
    GNET_FUNC_MF_ANTICOLL           = 0x21,     // (Get Card ID)
    GNET_FUNC_MF_SELECT             = 0x22,
    GNET_FUNC_MF_AUTHENTICATION     = 0x23,     // (with EEPROM key)
    GNET_FUNC_MF_READ_BLOCK         = 0x24,
    GNET_FUNC_MF_WRITE_BLOCK        = 0x25,

    GNET_FUNC_MF_VALUE              = 0x26,
    GNET_FUNC_MF_READ_VALUE         = 0x27,
    GNET_FUNC_MF_FORMAT_VALUE       = 0x28,

    GNET_FUNC_MF_ACCESS             = 0x29,     // Access Condition
    GNET_FUNC_MF_HALT               = 0x2A,

    GNET_FUNC_SAVE_KEY              = 0x2B,     // Save Key To EEPROM

    GNET_FUNC_GET_ID2               = 0x2C,     // Get ID 2 (Level2 PICC2 for Mifare UltraLight
    GNET_FUNC_GET_ACCESS            = 0x2D,     // Get Access Condition with MAD-GPB
    GNET_FUNC_AUTHENTICATION_FOR_KEY= 0x2E,     // Authentication and Key From Host
    GNET_FUNC_REQUEST_CARD          = 0x2F,     // Request All Card

    GNET_FUNC_VALUE_NO_TRANSFER     = 0x32,
    GNET_FUNC_TRANSFER              = 0x33,
    GNET_FUNC_RESTORE               = 0x34,

    GNET_FUNC_DUMP                  = 0x3B,
    GNET_FUNC_LED_BUZ_CONTROL       = 0x3C,
    GNET_FUNC_GET_MAD_AID           = 0x3D,
    GNET_FUNC_LED_CMD               = 0x3E,
    GNET_FUNC_AUTO_MODE             = 0x3F,
    #endif

    #if defined(GNET_DESFIRE_CMD)
    // DesFire
    GNET_FUNC_DF_REQUEST                        = 0x40,     // Request & Anticollion
    GNET_FUNC_DF_TCL_RATS                       = 0x41,     // TCL RATS/ATS
    GNET_FUNC_DF_DESELECT                       = 0x42,
    GNET_FUNC_DF_AUTHENTICATION                 = 0x43,
    GNET_FUNC_DF_KET_SETTINGS                   = 0x44,
    GNET_FUNC_DF_CHANGE_KET                     = 0x45,
    GNET_FUNC_DF_GET_KEY_VERSION                = 0x46,
    GNET_FUNC_DF_CREATE_APP                     = 0x47,
    GNET_FUNC_DF_DELETE_APP                     = 0x48,
    GNET_FUNC_DF_GET_APP_ID                     = 0x49,
    GNET_FUNC_DF_SELECT_APP                     = 0x4A,
    GNET_FUNC_DF_FORMAT_PICC                    = 0x4B,
    GNET_FUNC_DF_GET_CARD_VERSION               = 0x4C,
    GNET_FUNC_DF_GET_FILE_ID                    = 0x4D,
    GNET_FUNC_DF_GET_FILE_SETTINGS              = 0x4E,
    GNET_FUNC_DF_CREATE_STANDARD_DATA_FILE      = 0x4F,
    GNET_FUNC_DF_CREATE_BACKUP_DATA_FILE        = 0x50,
    GNET_FUNC_DF_CREATE_VALUE_FILE              = 0x51,
    GNET_FUNC_DF_CREATE_LINEAR_RECORD_FILE      = 0x52,
    GNET_FUNC_DF_CREATE_CYCLIC_RECORD_FILE      = 0x53,
    GNET_FUNC_DF_DELETE_FILE                    = 0x54,
    GNET_FUNC_DF_READ_DATA_FILE                 = 0x55,
    GNET_FUNC_DF_WRITE_DATA_FILE                = 0x56,
    GNET_FUNC_DF_GET_VALUE                      = 0x57,
    GNET_FUNC_DF_CREDIT                         = 0x58,
    GNET_FUNC_DF_DEBIT                          = 0x59,
    GNET_FUNC_DF_LIMITED_CREDIT                 = 0x5A,
    GNET_FUNC_DF_WRITE_RECORD                   = 0x5B,
    GNET_FUNC_DF_READ_RECORD                    = 0x5C,
    GNET_FUNC_DF_CLEAR_RECORD_FILE              = 0x5D,
    GNET_FUNC_DF_COMMIT_TRANSACTION             = 0x5E,
    GNET_FUNC_DF_ABORT_TRANSACTION              = 0x5F,
    GNET_FUNC_DF_GET_BUFFER_ADDRESS             = 0x60,
    GNET_FUNC_DF_ANTICOLLISION_LEVEL_SETTINGS   = 0x61,
    GNET_FUNC_DF_GET_UID                        = 0x62,
    GNET_FUNC_DF_SET_USER_CONFIGURE_PARAMETERS  = 0x63,
    GNET_FUNC_DF_GET_FREE_MEMORY                = 0x64,
    GNET_FUNC_DF_GET_ISO_FILE_NAME              = 0x65,

    GNET_FUNC_DF_SAVE_KEY_NO_0_13               = 0x6B,
    GNET_FUNC_DF_SET_FILE_COMMUNICATION_MODE    = 0x6C,

    GNET_FUNC_DF_ISO7816_COMMAND                = 0x6F,

    GNET_FUNC_GET_BUFFERING_DATA                = 0xFA,
    #endif

} GnetPlus_CmdSet;

// GnetPlus Error Set
typedef enum
{
    GNET_OK                 = 0x00,
    GNET_ERROR_DENY         = 0xE0,
    GNET_ERROR_EMPTY        = 0xE1,
    GNET_ERROR_ILLEGAL      = 0xE4,
    GNET_ERROR_OVERRUN      = 0xE6,
    GNET_ERROR_CRCERROR     = 0xE7,
    GNET_ERROR_IO           = 0xEA,
    GNET_ERROR_NOSUPPORT    = 0xEC,
    GNET_ERROR_OUTOFMEM     = 0xED,
    GNET_ERROR_OUTOFRANGE   = 0xEE,
    GNET_ERROR_UNKNOW       = 0xEF,

    GNET_ERROR_DONT_RESPONSE= 0xFF,

} GnetPlus_ErrSet;

// Product Feature Part Number IDs
typedef enum
{
    PART_NUM_DF700A_00      = 0x0000,
    PART_NUM_DF710A_00      = 0x0001,
    PART_NUM_DF710A_U2      = 0x0002,
    PART_NUM_DF750AK_00     = 0x0003,
    PART_NUM_DF750A_00      = 0x0004,
    PART_NUM_DF760AK_00     = 0x0005,
    PART_NUM_DF760A_00      = 0x0006,

    PART_NUM_UNKNOWN,

} Feature_PartNum;

typedef struct 
{
//u8    Reader_Number;
//u8    Tone_Code;
  u32   On_Time;
  u32   Off_Time;
  u8    Count;	
}Control_Buzzer_t;

#if 0//defined(MULTI_BUZZ_MODE)
typedef struct 
{
    u8  Group_Num;
    Control_Buzzer_t Wave_Group[4];

}Multiple_Buzzer_t;
#endif

// Buzzer Step
typedef enum
{
    BUZZ_STEP_ON    = 0,
    BUZZ_STEP_OFF,

} Buzz_StepSet;

// Buzzer Control
typedef enum
{
    BUZZ_CTRL_EN    = 0,
    BUZZ_CTRL_DIS,

} Buzz_CtrlSet;

// LEDs Control
typedef enum
{
    LEDS_CTRL_NSYNC = 0,    // Non-Sync with Buzzer
    LEDS_CTRL_SYNC,         // Sync with Buzzer
    LEDS_CTRL_DIS,

} Leds_CtrlSet;

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    u8 bPreamble;                                           // SOH: 0x01
    u8 bAddress;                                            // Device Address(Machine ID)
    u8 bCmdCode;                                            // Command code
    u8 bLength;                                             // Parameters length
    u8 bPayload[CMD_GP_MAX_DATA_SIZE];                      // Payload
    u16 uCrc16;                                             // CRC
} CmdFormat_t;

typedef struct
{
	BYTE flag;
	char bAscH, bAscL;
} GnetPlus_ASCII_t;

#pragma anon_unions
typedef struct
{
    u16 uLen;
    union
    {
        CmdFormat_t tFormat;
        u8 bBuffer[sizeof(CmdFormat_t)];
    };
} CmdProtocol_t;

typedef void (*pFunction)(void);

/* Exported variables --------------------------------------------------------*/
extern CmdProtocol_t cmd_parser;

extern GnetPlus_ASCII_t ASCII_parser;

#if defined(ORG_BUZZ_MODE)
extern Control_Buzzer_t control_buz;
#else   //defined(MULTI_BUZZ_MODE)
extern Multiple_Buzzer_t Multi_buz;
#endif

extern u8 bGnetPlusMode;

extern const char sFWVer[];
extern bool m_bLogin;
extern bool m_bHalt;

extern const char sName_DF700A_00[], sName_DF710A_00[], sName_DF710A_U2[];
extern const char sName_DF750AK_00[], sName_DF750A_00[], sName_DF760AK_00[], sName_DF760A_00[];


/* Exported functions --------------------------------------------------------*/
void CommandParser(u8 bData);
void CommandExecute_GP(void);

void ResponseNak_GP(void);
void ResponseNakData_GP(const u8* pbData, u8 bDataLength);
void ResponseEvent_GP(const u8* pbData, u8 bDataLength);
void ResponseData_GP(u8 bCmdCode, const u8* pbData, u8 bDataLength);
void ResponseAck_GP(void);
void ResponseAckData_GP(const u8* pbData, u8 bDataLength);
void AsciiPacker_GP(const u8* pbArgument, u8 bArgumentLength);

void cfg_save(void);
void KEY_DECODE(void);
void KEY_ENCODE(void);

u8 GNET_SetRegister(u16 Addr, u8* buf, u8 len);

extern s8 FLASH_EEPROM_ReadBytes(uint8_t* pbBuffer, uint32_t ulStartAddress, uint32_t ulOffsetAddress, uint16_t* puLength);
extern s8 FLASH_EEPROM_WriteBytes(uint8_t* pbBuffer, uint32_t ulStartAddress, uint32_t ulOffsetAddress, uint16_t* puLength);

u16 CRC16_Calculate_GP(u8* pData, u16 data_len, u16 first_crc);
u8 CRC16_Verify_GP(u8* check_data, u16 check_len);
void CRC16_Append_GP(u8* calculated_data, u16 calculated_len);

void MF5_API_MakeAccessBit(BYTE CB0, BYTE CB1, BYTE CB2, BYTE CB3, BYTE* AB);
void MF5_API_MakeAccessBitEx(BYTE CB0, BYTE CB1, BYTE CB2, BYTE CB3, BYTE GPB, BYTE* AB);

BYTE Hex2Val(BYTE ASC);
BYTE Hex2Val_B(BYTE ascH,BYTE ascL);

void LED_Control(u8 bdata);

void CommandPacker_GP(u8 bRepCode, const u8* pbArgument, u8 bArgumentLength);

