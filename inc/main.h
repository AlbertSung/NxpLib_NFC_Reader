/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H__
#define __MAIN_H__

/* Includes ------------------------------------------------------------------*/

/* USER CODE BEGIN Includes */
#include "ph_TypeDefs.h"
#include "MB_config.h"
#include "mb_errno.h"
#include "string.h"
#include "cmd_protocol.h"
#include "queue.h"

#define UART_QUEUE_SIZE                                     (255)

#define  PERIOD_VALUE       (uint32_t)(182 - 1)  /* Period Value  */
#define  PULSE1_VALUE       (uint32_t)(PERIOD_VALUE/2)        /* Capture Compare 1 Value  */
#define  PULSE2_VALUE       (uint32_t)(PERIOD_VALUE*37.5/100) /* Capture Compare 2 Value  */
#define  PULSE3_VALUE       (uint32_t)(PERIOD_VALUE/4)        /* Capture Compare 3 Value  */
#define  PULSE4_VALUE       (uint32_t)(PERIOD_VALUE*12.5/100) /* Capture Compare 4 Value  */


//! Application starting offset - Verify with bootloader footprint*/
#define APP_START_OFFSET             0x6000
//! Application starting address in Flash
#define APP_START_ADDRESS            (FLASH_ADDR + APP_START_OFFSET)

// 512 Bytes offset from APP_START_OFFSET for ISP Parameters
#define PAR_START_OFFSET             (APP_START_OFFSET - 0x0200)
#define PAR_START_ADDRESS            (FLASH_ADDR + PAR_START_OFFSET)

// 512 Bytes offset for Application Configurations (GNetPlus)
#define CONFIG_PAGE_ADDR             (FLASH_NB_OF_PAGES - 8) * NVMCTRL_PAGE_SIZE
// 256 Bytes offset for product Features Setting
#define FEATURE_PAGE_ADDR            (FLASH_NB_OF_PAGES - 4) * NVMCTRL_PAGE_SIZE


#define DEVICE_SERIAL_NUMBER_SIZE       9

#define DEVICE_SECURITY_MODE_SIZE       (1+16)
#define DEVICE_SECURITY_OFFSET_SIZE     (DEVICE_SERIAL_NUMBER_SIZE)

#define RELAY_CONTROL_SIZE              2
#define RELAY_CONTROL_OFFSET_SIZE       (DEVICE_SERIAL_NUMBER_SIZE+DEVICE_SECURITY_MODE_SIZE)

#define DEVICE_EEPROM_OFFSET_SIZE       (RELAY_CONTROL_SIZE+DEVICE_SERIAL_NUMBER_SIZE+DEVICE_SECURITY_MODE_SIZE)

#define LED_TIMEOUT_MS          (1 * 1000)//(3 * 1000)

#define PHHAL_HW_MFC_ALL                0x00U   /**< MIFARE Classic key type A/B. */

#define DI_BOUNCE_MS        (20)
#define KEY_BOUNCE_MS       (5)

// Bit definition
#define Bit0			(1<<0)
#define Bit1			(1<<1)
#define Bit2			(1<<2)
#define Bit3			(1<<3)
#define Bit4			(1<<4)
#define Bit5			(1<<5)
#define Bit6			(1<<6)
#define Bit7			(1<<7)
#define Bit8			(1<<8)
#define Bit9			(1<<9)
#define Bit10			(1<<10)
#define Bit11			(1<<11)
#define Bit12			(1<<12)
#define Bit13			(1<<13)
#define Bit14			(1<<14)
#define Bit15			(1<<15)
#define Bit16			(1<<16)
#define Bit17			(1<<17)
#define Bit18			(1<<18)
#define Bit19			(1<<19)
#define Bit20			(1<<20)
#define Bit21			(1<<21)
#define Bit22			(1<<22)
#define Bit23			(1<<23)
#define Bit24			(1<<24)
#define Bit25			(1<<25)
#define Bit26			(1<<26)
#define Bit27			(1<<27)
#define Bit28			(1<<28)
#define Bit29			(1<<29)
#define Bit30			(1<<30)
#define Bit31			(1<<31)

typedef enum 
{
  HAL_OK       = 0x00,
  HAL_ERROR    = 0x01,
  HAL_BUSY     = 0x02,
  HAL_TIMEOUT  = 0x03
} HAL_StatusTypeDef;

typedef enum
{
		LOOP_DISABLE  = 0,
		LOOP_ENABLE  = 1,

		LOOP_SENTEVT  = 2,
} Loop_StatusSet;

// Command Source Set
typedef enum
{
		CMD_SOURCE_NONE = 0,

		CMD_SOURCE_RS232 = 1,
		CMD_SOURCE_RS485 = 2,
} Cmd_SourceSet;

typedef enum
{
		LED_COLOR_OFF = 0,
		LED_COLOR_RED = 1,
		LED_COLOR_GREEN = 2,
		LED_COLOR_YELLOW = 3,
    
} HF_LedColor;

typedef enum
{
		BUZ_SILENT = 0x00,
		BUZ_OFF = 0x01,
		BUZ_DEFAULT = 0x02,
		BUZ_TBD = 0x03,

} Buz_EvtSet;

typedef enum
{
        LEDBUZ_STATUS_NORMAL = 0,
        LEDBUZ_STATUS_VALID,
        LEDBUZ_STATUS_INVALID,
        LEDBUZ_STATUS_EXTERNAL,

        #ifdef STANDARD_DF750_760A
        LEDBUZ_STATUS_KEYPAD,
        LEDBUZ_STATUS_CONFIG,
        LEDBUZ_STATUS_VERIFY,
        #endif
        #ifdef DF700A_NCS
        LEDBUZ_STATUS_1BEEP,
        #endif

        LEDBUZ_STATUS_RESET = 0xFF,

} LedBuz_StatusSet;

extern u8 g_CmdSource;
extern uint16_t g_wPartNum;

extern u32 ulCMD_Tick, ulAUTO_Tick;

#ifdef STANDARD_DF750_760A
extern u8 bLoopKYPD;
#endif

extern u32 ulDIWR_Tick, ulBounceDI0_Tick, ulBounceDI1_Tick;
extern u32 ulALIVE_Tick, ulALIVE_Timeout;
#ifdef STANDARD_DF750_760A
extern u32 ulBounceKYPD_Tick;
extern u32 ulCONFIG_Tick;
#endif

extern u8 m_bDI_Pulse;
extern bool bDI0_TRIG, bDI1_TRIG;
#ifdef STANDARD_DF750_760A
extern bool bConfigMode, bVerifyMode;
extern uint8_t bConfig_TimeSec;
extern uint8_t bKYPD_Col, bKYPD_Row;
extern uint32_t dwPassCode;
#endif

extern u8 bLoopBUZZ, bLoopLEDS, bLoopDIWR, bLoopLIVE;
extern u8 bBUZZ_Step;
extern u8 bBUZZ_Ctrl, bLEDS_Ctrl;
extern u32 ulBUZZ_Tick, ulBUZZ_Pulse_Tick, ulBUZZ_Timeout;

extern u8 bLedState;
extern u32 ulLEDS_Tick, ulLEDS_Timeout;
extern u8 bData_Output;
extern u8 m_cLastLED;
extern u8 bOldStatus;
extern uint8_t bRxPutBuf[], bRxGetBuf[];
extern Queue_Buffer_t g_bRS232Queue;

extern void SwapValue(void *val, u8 size);

extern bool Is_Ours_Mfc_Setkey(uint8_t *pKey, uint8_t bKeyType, uint8_t bKeyName, uint8_t bMF_KeyType);
bool MfDf_KeyStore_Setkey(uint8_t *pKey, uint16_t wKeyType, uint16_t wKeyNum, uint16_t wKeyVer);

extern u8 LedCheck(void);
extern void LedOff(void);
extern void LedUpdate(u8 bLedData);
extern void LedRestore(u8 bLedData, u32 ulLedTime);

void LedStatusCheck(u8 bStatus, bool bForce);
void GPIO_DI_Init(u8 bLEVEL);

void NVIC_SetVectorTable(void);
void ReadFeature(void);
void ReadConfig(void);
void WriteConfig(void);
void ProtocolCheck(void);

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/

/* USER CODE END Includes */

#define _offset_(x,y,max) ((x>=y)? x-y : max+x-y)  //max-y+x

bool is_timeout_ms(u32 top, u32 lastcount);
#define IsTimeout_ms(TICKS, MILLISECONDS)               is_timeout_ms(TICKS, MILLISECONDS)

/* Private define ------------------------------------------------------------*/

/* USER CODE END Private defines */

#ifdef __cplusplus
 extern "C" {
#endif
void _Error_Handler(char *, int);

#define Error_Handler() _Error_Handler(__FILE__, __LINE__)
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
