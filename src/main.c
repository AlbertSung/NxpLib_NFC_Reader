/*************************************************************************************
 *
 *
 *************************************************************************************/

#include <asf.h>

#include "main.h"

#include "Porting_SAMD21.h"
#include "queue.h"

#include "Wiegand_out.h"
#ifdef STANDARD_DF750_760A
#include "tk_output.h"
#endif



extern struct usart_module usart_instance;

//
uint8_t bLoopBUZZ = LOOP_DISABLE;
uint8_t bLoopLEDS = LOOP_DISABLE;
uint8_t bLoopDIWR = LOOP_DISABLE;
uint8_t bLoopLIVE = LOOP_DISABLE;
#ifdef STANDARD_DF750_760A
uint8_t bLoopKYPD = LOOP_DISABLE;
#endif

uint8_t bBUZZ_Step = BUZZ_STEP_ON;
uint8_t bBUZZ_Ctrl = BUZZ_CTRL_EN, bLEDS_Ctrl = LEDS_CTRL_NSYNC;

uint8_t m_bDI_Pulse = 0;
bool bDI0_TRIG = false, bDI1_TRIG = false;
#ifdef STANDARD_DF750_760A
bool bConfigMode = false, bVerifyMode = false;
uint8_t bConfig_TimeSec;
uint8_t bKYPD_Col = 0, bKYPD_Row = 0;
uint32_t dwPassCode = 0;
#endif

uint32_t ulBUZZ_Tick, ulBUZZ_Pulse_Tick, ulBUZZ_Timeout;
uint32_t ulLEDS_Tick, ulLEDS_Timeout;
uint32_t ulCMD_Tick, ulAUTO_Tick;
uint32_t ulDIWR_Tick, ulBounceDI0_Tick, ulBounceDI1_Tick;
uint32_t ulALIVE_Tick, ulALIVE_Timeout;
#ifdef STANDARD_DF750_760A
uint32_t ulKYSCAN_Tick;
uint32_t ulBounceKYPD_Tick;
uint32_t ulCONFIG_Tick;
#endif

uint8_t g_CmdSource = CMD_SOURCE_NONE;
uint16_t g_wPartNum;

uint8_t bOldStatus = LEDBUZ_STATUS_RESET;
uint8_t bData_Output = 0;

uint8_t m_cLastLED;

uint8_t bRxPutBuf[MAX_RX_BUFFER_LENGTH], bRxGetBuf[MAX_RX_BUFFER_LENGTH];
Queue_CreateBuffer(g_bRS232Queue, UART_QUEUE_SIZE);

#ifdef STANDARD_DF750_760A
const uint8_t Keypad_AscCode[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#'};
const uint8_t Keypad_NumCode[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x00, 0x0B};

const uint8_t Keypad_CfgCode[] = {'*', '0', '0', '#'};
#endif



// *********************************************************************************************** //

u8 LedCheck(void)
{
    u8 bLastLED = 0;

    if(phDriver_PinRead(PHDRIVER_PIN_LEDG, PH_DRIVER_PINFUNC_OUTPUT) == LED_G_ON_LEVEL)
        bLastLED |= Bit0;

    if(phDriver_PinRead(PHDRIVER_PIN_LEDR, PH_DRIVER_PINFUNC_OUTPUT) == LED_R_ON_LEVEL)
        bLastLED |= Bit1;

    return bLastLED;
}

void LedOff(void)
{
    phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_OFF_LEVEL);
    phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_OFF_LEVEL);
}

void LedUpdate(u8 bLedData)
{
    LedOff();

    if(bLedData & Bit0)
        phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_ON_LEVEL);

    if(bLedData & Bit1)
        phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_ON_LEVEL);
}

void LedRestore(u8 bLedData, u32 ulLedTime)
{
    ulLEDS_Timeout = ulLedTime;
    ulLEDS_Tick = phDriver_GetTick();
    bLoopLEDS = LOOP_ENABLE;

    LedUpdate(bLedData);
}

void LedStatusCheck(u8 bStatus, bool bForce)
{
    if((bOldStatus != bStatus) || bForce)
    {
        if((!(g_tRegisters.s1.bLED_Flag & Bit0) && !(g_tRegisters.s1.bLED_Flag & Bit7)) || bForce)
        {
            switch(bStatus)
            {
              case LEDBUZ_STATUS_NORMAL:    // Normal Running
                bData_Output = 0;

                m_cLastLED = g_tRegisters.s1.bLED_Normal & 0x03;
                control_buz.Count = 0;
                break;

              case LEDBUZ_STATUS_VALID:     // Card Is Valid
                m_cLastLED = g_tRegisters.s1.bLED_Valid & 0x03;
                control_buz.Count = (g_tRegisters.s1.bLED_Valid >> 4) & 0x03;

                bBUZZ_Ctrl = BUZZ_CTRL_EN;
                bLEDS_Ctrl = LEDS_CTRL_NSYNC;
                break;

              case LEDBUZ_STATUS_INVALID:	// Card Is Invalid
                m_cLastLED = g_tRegisters.s1.bLED_Invalid & 0x03;
                control_buz.Count = (g_tRegisters.s1.bLED_Invalid >> 4) & 0x03;

                bBUZZ_Ctrl = BUZZ_CTRL_EN;
                bLEDS_Ctrl = LEDS_CTRL_NSYNC;
                break;

              case LEDBUZ_STATUS_EXTERNAL:	// External Alarm
                m_cLastLED = g_tRegisters.s1.bLED_External & 0x03;
                control_buz.Count = (g_tRegisters.s1.bLED_External >> 4) & 0x03;

                bBUZZ_Ctrl = BUZZ_CTRL_EN;
                bLEDS_Ctrl = LEDS_CTRL_NSYNC;
                break;

              #ifdef STANDARD_DF750_760A
              case LEDBUZ_STATUS_KEYPAD:    // Keypad Is Pressed
              case LEDBUZ_STATUS_CONFIG:    // Wait For Config Card
                m_cLastLED = g_tRegisters.s1.bLED_Normal & 0x03;
                control_buz.Count = 1;

                bBUZZ_Ctrl = BUZZ_CTRL_EN;
                bLEDS_Ctrl = LEDS_CTRL_DIS;
                break;
              case LEDBUZ_STATUS_VERIFY:    // Wait For Pass Code
                m_cLastLED = g_tRegisters.s1.bLED_Normal & 0x03;
                control_buz.Count = 1;

                bBUZZ_Ctrl = BUZZ_CTRL_DIS;
                bLEDS_Ctrl = LEDS_CTRL_NSYNC;
                break;
              #endif
              #ifdef DF700A_NCS
              case LEDBUZ_STATUS_1BEEP:
                m_cLastLED = g_tRegisters.s1.bLED_Normal & 0x03;
                control_buz.Count = 1;

                bBUZZ_Ctrl = BUZZ_CTRL_EN;
                bLEDS_Ctrl = LEDS_CTRL_DIS;
                break;
              #endif
            }

            if(control_buz.Count != 0)
            {
                control_buz.On_Time = 2 * BUZZER_UNIT_MS;
                control_buz.Off_Time = 1 * BUZZER_UNIT_MS;
                ulBUZZ_Timeout = (control_buz.On_Time + control_buz.Off_Time) * control_buz.Count;

                ulBUZZ_Tick = phDriver_GetTick();
                ulBUZZ_Pulse_Tick = ulBUZZ_Tick;

                if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                    phDriver_PWMStart();

                bBUZZ_Step = BUZZ_STEP_ON;
                bLoopBUZZ = LOOP_ENABLE;

                if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                    LedOff();
            }
            else
                LedRestore(m_cLastLED, LED_TIMEOUT_MS);

            bOldStatus = bStatus;
        }
        else    // Need to reply NAK to host when error happened in RS232 control
        {
            if (bStatus == LEDBUZ_STATUS_INVALID)
            {
//                ResponseNak_GP();     // Not needed to reply in network
            }
        }
    }
}

// *********************************************************************************************** //

void NVIC_SetVectorTable(void)
{
	/* Rebase the vector table base address */
	SCB->VTOR = ((uint32_t)APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);
}

void ReadFeature(void)
{
    u8 RetryCount = 0;
    u8 bResult;
    u16 wLen;

    wLen = sizeof(g_tPrvSettings.Settings);

    do
    {
        bResult = FLASH_EEPROM_ReadBytes(g_tPrvSettings.bData, FEATURE_PAGE_ADDR, 0, &wLen);
        RetryCount++;
    }
    while((bResult != HAL_OK) && (RetryCount < 5));

    if(bResult == HAL_OK)
    {
        #ifdef STANDARD_DF7X0A
        if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF700A_00) == 0)
            g_wPartNum = PART_NUM_DF700A_00;
        else if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF710A_00) == 0)
            g_wPartNum = PART_NUM_DF710A_00;
        else if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF710A_U2) == 0)
            g_wPartNum = PART_NUM_DF710A_U2;
        else
        {
            g_wPartNum = PART_NUM_DF700A_00;

            // Recover to default setting
            wLen = sizeof(g_tPrvSettings.Settings.bPartName);
            memcpy(g_tPrvSettings.Settings.bPartName, sName_DF700A_00, wLen);

            wLen = sizeof(g_tPrvSettings.Settings.bSerialNum);
            memset(g_tPrvSettings.Settings.bSerialNum, 0x00, wLen);

            wLen = sizeof(g_tPrvSettings.Settings);

            do
            {
                bResult = FLASH_EEPROM_WriteBytes(g_tPrvSettings.bData, FEATURE_PAGE_ADDR, 0, &wLen);
                RetryCount++;
            }
            while((bResult != GNET_OK) && (RetryCount < 5)); 
        }
        #elif defined(STANDARD_DF750_760A)
        if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF750AK_00) == 0)
            g_wPartNum = PART_NUM_DF750AK_00;
        else if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF750A_00) == 0)
            g_wPartNum = PART_NUM_DF750A_00;
        else if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF760AK_00) == 0)
            g_wPartNum = PART_NUM_DF760AK_00;
        else if(strcmp(g_tPrvSettings.Settings.bPartName, sName_DF760A_00) == 0)
            g_wPartNum = PART_NUM_DF760A_00;
        else
        {
            g_wPartNum = PART_NUM_DF750A_00;

            // Recover to default setting
            wLen = sizeof(g_tPrvSettings.Settings.bPartName);
            memcpy(g_tPrvSettings.Settings.bPartName, sName_DF750A_00, wLen);

            wLen = sizeof(g_tPrvSettings.Settings.bSerialNum);
            memset(g_tPrvSettings.Settings.bSerialNum, 0x00, wLen);

            wLen = sizeof(g_tPrvSettings.Settings);

            do
            {
                bResult = FLASH_EEPROM_WriteBytes(g_tPrvSettings.bData, FEATURE_PAGE_ADDR, 0, &wLen);
                RetryCount++;
            }
            while((bResult != GNET_OK) && (RetryCount < 5)); 
        }
        #endif
    }
    else
    {
        #ifdef STANDARD_DF7X0A
        g_wPartNum = PART_NUM_DF700A_00;

        // Recover to default setting
        wLen = sizeof(g_tPrvSettings.Settings.bPartName);
        memcpy(g_tPrvSettings.Settings.bPartName, sName_DF700A_00, wLen);
        #elif defined(STANDARD_DF750_760A)
        g_wPartNum = PART_NUM_DF750A_00;

        // Recover to default setting
        wLen = sizeof(g_tPrvSettings.Settings.bPartName);
        memcpy(g_tPrvSettings.Settings.bPartName, sName_DF750A_00, wLen);
        #endif

        wLen = sizeof(g_tPrvSettings.Settings.bSerialNum);
        memset(g_tPrvSettings.Settings.bSerialNum, 0x00, wLen);

        wLen = sizeof(g_tPrvSettings.Settings);

        do
        {
            bResult = FLASH_EEPROM_WriteBytes(g_tPrvSettings.bData, FEATURE_PAGE_ADDR, 0, &wLen);
            RetryCount++;
        }
        while((bResult != GNET_OK) && (RetryCount < 5)); 
    }
}

void ReadConfig(void)
{
    s8 cResult;
    u16 wLen;
    u8 E2RetryCount = 0;
    
    wLen = sizeof(g_tRegisters.s1);
    do
    {
        cResult = FLASH_EEPROM_ReadBytes(g_tRegisters.bRegisters, CONFIG_PAGE_ADDR, 0, &wLen);
        E2RetryCount++;
    }
    while((cResult != HAL_OK)&&(E2RetryCount<5));
        
    if(cResult == HAL_OK)
    {
        if(g_tRegisters.s1.wCheckCRC != CRC16_Calculate_GP(g_tRegisters.bRegisters+2, wLen-2, DEF_CRC_PRESET))
        {
            u8 bResult;

            g_tRegisters = DEFALUT_REGISTERS;
            KEY_ENCODE();
            wLen = sizeof(g_tRegisters.s1);
            g_tRegisters.s1.wCheckCRC = CRC16_Calculate_GP(g_tRegisters.bRegisters+2, wLen-2, DEF_CRC_PRESET);
            do
            {
                bResult = FLASH_EEPROM_WriteBytes(g_tRegisters.bRegisters, CONFIG_PAGE_ADDR, 0, &wLen);
                E2RetryCount++;
            }
            while((bResult != GNET_OK)&&(E2RetryCount<5)); 
            KEY_DECODE();
        }
        else
            KEY_DECODE();
    }
    else
    {
        u8 bResult;

        g_tRegisters = DEFALUT_REGISTERS;
        KEY_ENCODE();
        wLen = sizeof(g_tRegisters.s1);
        g_tRegisters.s1.wCheckCRC = CRC16_Calculate_GP(g_tRegisters.bRegisters+2, wLen-2, DEF_CRC_PRESET);
        do
        {
            bResult = FLASH_EEPROM_WriteBytes(g_tRegisters.bRegisters, CONFIG_PAGE_ADDR, 0, &wLen);
            E2RetryCount++;
        }
        while((bResult != GNET_OK)&&(E2RetryCount<5)); 
        KEY_DECODE();
    }
}

void WriteConfig(void)
{
    u16 wLen;
    u8 E2RetryCount = 0;

    u8 bResult;

    KEY_ENCODE();
    wLen = sizeof(g_tRegisters.s1);
    g_tRegisters.s1.wCheckCRC = CRC16_Calculate_GP(g_tRegisters.bRegisters+2, wLen-2, DEF_CRC_PRESET);
    do
    {
        bResult = FLASH_EEPROM_WriteBytes(g_tRegisters.bRegisters, CONFIG_PAGE_ADDR, 0, &wLen);
        E2RetryCount++;
    }
    while((bResult != GNET_OK)&&(E2RetryCount<5)); 
    KEY_DECODE();
}

void ProtocolCheck(void)
{
    do
    {
        if(Queue_Get(&g_bRS232Queue, bRxGetBuf) == TRUE)
        {
            CommandParser(bRxGetBuf[0]);     // only for 1 byte
        }
        else
        {
            if(cmd_parser.uLen != 0)
            {
                if(IsTimeout_ms(ulCMD_Tick, 1000))
                {
                    memset(cmd_parser.bBuffer, 0x00, cmd_parser.uLen);
                    cmd_parser.uLen = 0;
                }
            }
        }

    } while(0);
}

// *********************************************************************************************** //

bool is_timeout_ms(u32 lastcount, u32 top)
{
    if(_offset_(phDriver_GetTick(), lastcount, 0xFFFFFF) >= top)//nCount )		// Simple 24-bit timer.
        return true;
    return false;
}


// *********************************************************************************************** //

int main(void)
{
    uint8_t i;
    #if 1//0//defined(MULTI_BUZZ_MODE)
    uint8_t bBuzz_Group = 0;
    #endif
    #ifdef STANDARD_DF750_760A
    bool bKeyPress = false;
    uint8_t bKeyPos, bKeyFlag, bCfgIdx = 0;
    uint8_t bKeyBuf[6], bDataLen;
    uint32_t dwKeyData = 0;
    phStatus_t status;
    #endif
    uint8_t bLiveVal;

#ifdef  USE_BOOT_LOADER
    /* Begin here after jumpping from Boot Loader */
    NVIC_SetVectorTable();
#endif

    /* Initialization after power-on */
    DeviceInit();

    RF_INIT();

    Queue_Init(&g_bRS232Queue);

    cmd_parser.uLen = 0;

    ulAUTO_Tick = phDriver_GetTick();
    #ifdef STANDARD_DF750_760A
    ulKYSCAN_Tick = phDriver_GetTick();
    #endif


    /* Read FLASH data after phDriver_NVMInit() in DeviceInit() */
    ReadConfig();
    ReadFeature();


    /* Initialization after ReadConfig() */
    if((g_tRegisters.s1.bLED_Flag & Bit1) == 0)
        config_di(DI_ACT_HIGH_LEVEL, DI_ACT_HIGH_LEVEL);
    else
        config_di(DI_ACT_LOW_LEVEL, DI_ACT_LOW_LEVEL);

    if(g_tRegisters.s1.bInterface != DATA_OUTPUT_RS232)
    {
        config_data();

        if(g_tRegisters.s1.bInterface == DATA_OUTPUT_TK2)
            config_tkcp();
    }

    if((g_tRegisters.s1.bAlive != 0) && (g_tRegisters.s1.bInterface == DATA_OUTPUT_WIEGAND))
    {
        bLiveVal = 0xAA;
        Wiegand_MSB(&bLiveVal, 8, false);

        ulALIVE_Timeout = g_tRegisters.s1.bAlive * 1000;
        ulALIVE_Tick = phDriver_GetTick();
        bLoopLIVE = LOOP_ENABLE;
    }

    /* Initialization after ReadFeature() */
    if((g_wPartNum == PART_NUM_DF710A_00) || (g_wPartNum == PART_NUM_DF710A_U2)
    || (g_wPartNum == PART_NUM_DF760A_00) || (g_wPartNum == PART_NUM_DF760AK_00))
    {
        config_rts();
        g_CmdSource = CMD_SOURCE_RS485;
    }
    else
        g_CmdSource = CMD_SOURCE_RS232;

    if((g_wPartNum == PART_NUM_DF750AK_00) || (g_wPartNum == PART_NUM_DF760AK_00))
    {
        config_keypad();

        if(((g_tRegisters.s1.bKeyPad_Flag & 0x7F) < 0x03) || ((g_tRegisters.s1.bKeyPad_Flag & 0x7F) > 0x05))
        {
            config_data();

            if((g_tRegisters.s1.bKeyPad_Flag & 0x7F) == 0x20)
                config_tkcp();
        }
    }


    /* Audio for booting up */
    for (i = 0; i < 2; i++)
    {
        LedOff();
        if (i == 0)
            phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_ON_LEVEL);
        else
            phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_ON_LEVEL);
        phDriver_PWMStart();
        ulBUZZ_Pulse_Tick = phDriver_GetTick();
        while(!IsTimeout_ms(ulBUZZ_Pulse_Tick, 2 * BUZZER_UNIT_MS));
        phDriver_PWMStop();
        ulBUZZ_Pulse_Tick = phDriver_GetTick();
        while(!IsTimeout_ms(ulBUZZ_Pulse_Tick, 1 * BUZZER_UNIT_MS));
    }
    LedOff();

    if(!(g_tRegisters.s1.bLED_Flag & Bit0) && !(g_tRegisters.s1.bLED_Flag & Bit7))
        LedUpdate(g_tRegisters.s1.bLED_Normal & 0x03);


    /* Main tasks */
	while (true)
    {
        /* Check user command */
        ProtocolCheck();

        /* Restore to Auto Mode */
        if(bAutoMode == false)
        {
            if(IsTimeout_ms(ulCMD_Tick, 10 * 60 * 1000))
            {
                bAutoMode = true;
                bCmdTask = 0;
            }
        }

        /* Do RFID Task */
        if(bLoopBUZZ == LOOP_DISABLE)
            RF_task();

        /* Do Configuration Task */
        #ifdef STANDARD_DF750_760A
        if((bConfigMode == true) || (bVerifyMode == true))
        {
            if(IsTimeout_ms(ulCONFIG_Tick, 500))    // Every 500 ms
            {
                // Make beep sound and LED
                if(--bConfig_TimeSec > 0)
                {
                    // Count down led/sound
                    if(bConfigMode == true)
                    {
                        if(bConfig_TimeSec % 2 == 0)            // Every 1 sec
                            LedStatusCheck(LEDBUZ_STATUS_CONFIG, true);     // Forced to true since LED isn't changed to return normal status
                    }
                    else
                    {
                        LedStatusCheck(LEDBUZ_STATUS_VERIFY, false);
                    }

                    ulCONFIG_Tick = phDriver_GetTick();
                }
                else
                {
                    // Error sound
                    LedStatusCheck(LEDBUZ_STATUS_INVALID, false);

                    if(bConfigMode == true)
                        bConfigMode = false;
                    else
                    {
                        dwKeyData = 0;
                        dwPassCode = 0;
                        bVerifyMode = false;
                    }
                }
            }
        }
        #endif

        /* Do Buzzer Task */
        if(bLoopBUZZ != LOOP_DISABLE)
        {
            if(!IsTimeout_ms(ulBUZZ_Tick, ulBUZZ_Timeout))
            {
                #if defined(ORG_BUZZ_MODE)
                if(control_buz.Count != 0)
                {
                    if(bBUZZ_Step == BUZZ_STEP_ON)
                    {
                        if(IsTimeout_ms(ulBUZZ_Pulse_Tick, control_buz.On_Time))
                        {
                            if(control_buz.Off_Time > 0)
                            {
                                if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                    phDriver_PWMStop();
                                if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                                    LedRestore(m_cLastLED, control_buz.Off_Time);

                                ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                bBUZZ_Step = BUZZ_STEP_OFF;
                            }
                            else    // for long beep without buzzer off
                            {
                                control_buz.Count--;
                                if(control_buz.Count == 0)
                                {
                                    if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                        phDriver_PWMStop();
                                }
                                else
                                {
                                    ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                }
                            }
                        }
                    }
                    else if(bBUZZ_Step == BUZZ_STEP_OFF)
                    {
                        if(IsTimeout_ms(ulBUZZ_Pulse_Tick, control_buz.Off_Time))
                        {
                            control_buz.Count--;
                            if(control_buz.Count > 0)
                            {
                                if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                    phDriver_PWMStart();
                                if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                                    LedRestore(0, control_buz.On_Time);

                                ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                bBUZZ_Step = BUZZ_STEP_ON;
                            }
                        }
                    }
                }
                #else//defined(MULTI_BUZZ_MODE)
                if(Multi_buz.Group_Num != 0)
                {
                    if(Multi_buz.Wave_Group[bBuzz_Group].Count != 0)
                    {
                        if(bBUZZ_Step == BUZZ_STEP_ON)
                        {
                            if(IsTimeout_ms(ulBUZZ_Pulse_Tick, Multi_buz.Wave_Group[bBuzz_Group].On_Time))
                            {
                                if(Multi_buz.Wave_Group[bBuzz_Group].Off_Time > 0)
                                {
                                    if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                        phDriver_PWMStop();
                                    if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                                        LedRestore(m_cLastLED, Multi_buz.Wave_Group[bBuzz_Group].Off_Time);

                                    ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                    bBUZZ_Step = BUZZ_STEP_OFF;
                                }
                                else    // for long beep without buzzer off
                                {
                                    Multi_buz.Wave_Group[bBuzz_Group].Count--;
                                    if(Multi_buz.Wave_Group[bBuzz_Group].Count == 0)
                                    {
                                        if(--Multi_buz.Group_Num > 0)
                                        {
                                            bBuzz_Group++;

                                            if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                                            {
                                                if(Multi_buz.Wave_Group[bBuzz_Group].Off_Time > 0)
                                                    LedRestore(0, Multi_buz.Wave_Group[bBuzz_Group].On_Time);
                                                else
                                                    LedRestore(0, Multi_buz.Wave_Group[bBuzz_Group].On_Time * Multi_buz.Wave_Group[bBuzz_Group].Count);
                                            }

                                            ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                        }
                                        else //(--Multi_buz.Group_Num == 0)
                                        {
                                            bBuzz_Group = 0;

                                            if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                                phDriver_PWMStop();
                                        }
                                    }
                                    else
                                    {
                                        ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                    }
                                }
                            }
                        }
                        else if(bBUZZ_Step == BUZZ_STEP_OFF)
                        {
                            if(IsTimeout_ms(ulBUZZ_Pulse_Tick, Multi_buz.Wave_Group[bBuzz_Group].Off_Time))
                            {
                                Multi_buz.Wave_Group[bBuzz_Group].Count--;
                                if(Multi_buz.Wave_Group[bBuzz_Group].Count > 0)
                                {
                                    if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                        phDriver_PWMStart();
                                    if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                                        LedRestore(0, Multi_buz.Wave_Group[bBuzz_Group].On_Time);

                                    ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                    bBUZZ_Step = BUZZ_STEP_ON;
                                }
                                else if(--Multi_buz.Group_Num > 0)
                                {
                                    bBuzz_Group++;

                                    if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                                        phDriver_PWMStart();
                                    if(bLEDS_Ctrl == LEDS_CTRL_NSYNC)
                                        LedRestore(0, Multi_buz.Wave_Group[bBuzz_Group].On_Time);

                                    ulBUZZ_Pulse_Tick = phDriver_GetTick();
                                    bBUZZ_Step = BUZZ_STEP_ON;
                                }
                                else //(--Multi_buz.Group_Num == 0)
                                {
                                    bBuzz_Group = 0;
                                }
                            }
                        }
                    }
                }
                #endif
            }
            else
            {
                bBuzz_Group = 0;

                if(bBUZZ_Ctrl == BUZZ_CTRL_EN)
                    phDriver_PWMStop();

                bLoopBUZZ = LOOP_DISABLE;
            }
        }

        /* Do LED Task */
        if(bLoopLEDS != LOOP_DISABLE)
        {
            if((bLoopBUZZ == LOOP_DISABLE) && (bLoopDIWR == LOOP_DISABLE))       // only works when Buzzer & DI isn't working
            {
                if(IsTimeout_ms(ulLEDS_Tick, ulLEDS_Timeout))
                {
                    LedStatusCheck(LEDBUZ_STATUS_NORMAL, true);     // Forced to true in case some command is received to change LED color

                    bLoopLEDS = LOOP_DISABLE;
                }
            }
        }

        /* Do DI Task */
        if(bLoopDIWR != LOOP_DISABLE)
        {
            if(bLoopBUZZ == LOOP_DISABLE)       // only works when Buzzer isn't working
            {
                // Brown & Orange wire control (2-wire)
                if (g_tRegisters.s1.bLED_Flag & Bit7)
                {
                    // Check nDI level
                    if(bDI0_TRIG && IsTimeout_ms(ulBounceDI0_Tick, DI_BOUNCE_MS))
                    {
                        if(((!(g_tRegisters.s1.bLED_Flag & Bit1)) && (phDriver_PinRead(PHDRIVER_PIN_nDI, PH_DRIVER_PINFUNC_INPUT) == DI_ACT_HIGH_LEVEL))   // Active High
                        || (( (g_tRegisters.s1.bLED_Flag & Bit1)) && (phDriver_PinRead(PHDRIVER_PIN_nDI, PH_DRIVER_PINFUNC_INPUT) == DI_ACT_LOW_LEVEL )))  // Active Low
                        {
                            #ifdef DF700A_NCS
                            phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_ON_LEVEL);
                            #else
                            phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_ON_LEVEL);
                            #endif
                        }
                        else
                        {
                            #ifdef DF700A_NCS
                            phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_OFF_LEVEL);
                            #else
                            phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_OFF_LEVEL);
                            #endif

                            bDI0_TRIG = false;
                        }
                    }

                    // Check nDI1 level
                    if(bDI1_TRIG && IsTimeout_ms(ulBounceDI1_Tick, DI_BOUNCE_MS))
                    {
                        if(((!(g_tRegisters.s1.bLED_Flag & Bit1)) && (phDriver_PinRead(PHDRIVER_PIN_nDI1, PH_DRIVER_PINFUNC_INPUT) == DI_ACT_HIGH_LEVEL))   // Active High
                        || (( (g_tRegisters.s1.bLED_Flag & Bit1)) && (phDriver_PinRead(PHDRIVER_PIN_nDI1, PH_DRIVER_PINFUNC_INPUT) == DI_ACT_LOW_LEVEL )))  // Active Low
                        {
                            #ifdef DF700A_NCS
                            phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_ON_LEVEL);
                            #else
                            phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_ON_LEVEL);
                            #endif
                        }
                        else
                        {
                            #ifdef DF700A_NCS
                            phDriver_PinWrite(PHDRIVER_PIN_LEDG, LED_G_OFF_LEVEL);
                            #else
                            phDriver_PinWrite(PHDRIVER_PIN_LEDR, LED_R_OFF_LEVEL);
                            #endif

                            bDI1_TRIG = false;
                        }
                    }

                    if((bDI0_TRIG == false) && (bDI1_TRIG == false))
                        bLoopDIWR = LOOP_DISABLE;
                }
                // Brown wire control
                else
                {
                    if(IsTimeout_ms(ulDIWR_Tick, 1200))
                    {
                        if(((!(g_tRegisters.s1.bLED_Flag & Bit1)) && (phDriver_PinRead(PHDRIVER_PIN_nDI, PH_DRIVER_PINFUNC_INPUT) == DI_ACT_HIGH_LEVEL))   // Active High
                        || (( (g_tRegisters.s1.bLED_Flag & Bit1)) && (phDriver_PinRead(PHDRIVER_PIN_nDI, PH_DRIVER_PINFUNC_INPUT) == DI_ACT_LOW_LEVEL )))  // Active Low
                        {
                            LedStatusCheck(LEDBUZ_STATUS_EXTERNAL, false);
                        }
                        else
                        {
                            if(m_bDI_Pulse >= DI_PULSE_COUNT)
                                LedStatusCheck(LEDBUZ_STATUS_VALID, false);
                            else
                                LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
                        }

                        bData_Output = 0;

                        bLoopDIWR = LOOP_DISABLE;
                    }
                }
            }
        }

        /* Do Keypad Task */
        #ifdef STANDARD_DF750_760A
        if(bLoopKYPD == LOOP_ENABLE)
        {
            if(bLoopBUZZ == LOOP_DISABLE)       // only works when Buzzer isn't working
            {
                if(IsTimeout_ms(ulBounceKYPD_Tick, KEY_BOUNCE_MS))
                {
                    // Reset alive period counting
                    if(bLoopLIVE == LOOP_ENABLE)
                        ulALIVE_Tick = phDriver_GetTick();

                    bLoopKYPD = LOOP_SENTEVT;

                    // Make buzzer beep
                    if(g_tRegisters.s1.bLED_Flag & Bit5)
                        LedStatusCheck(LEDBUZ_STATUS_KEYPAD, true);     // Forced to true since LED isn't changed to return normal status

                    // Transfer the pressed key from position to index in key matrix
                    bKeyPos = (3 * (bKYPD_Row - 1)) + (bKYPD_Col - 1);

                    // Compare with key for configure card
                    if((bConfigMode == false) && (bVerifyMode == false))
                    {
                        if(Keypad_AscCode[bKeyPos] == Keypad_CfgCode[bCfgIdx])
                        {
                            bCfgIdx++;
                            if(bCfgIdx >= sizeof(Keypad_CfgCode))
                            {
                                bCfgIdx = 0;

                                if(g_tRegisters.s1.Alignment & Bit3)    // enable config card
                                {
                                    ulCONFIG_Tick = phDriver_GetTick();
                                    bConfig_TimeSec = 10 * 2;

                                    bConfigMode = true;
                                }
                                else
                                    LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
                            }
                        }
                        else
                            bCfgIdx = 0;
                    }
                    else if(bConfigMode == true)
                    {
                        bConfigMode = false;
                    }

                    // Determine output format
                    if((bVerifyMode == true) && (dwPassCode > 0))       // Verify mode is enabled by reading card
                    {
                        bKeyFlag = 0x04;
                    }
                    else
                    {
                        bKeyFlag = g_tRegisters.s1.bKeyPad_Flag & 0x7F;
                    }

                    switch(bKeyFlag)
                    {
                      case 0x20:    // TK2

                        bKeyBuf[0] = Keypad_NumCode[bKeyPos];
                        TK2_Output_LSB(bKeyBuf, 1);
                        break;

                      case 0x00:    // Wiegand 4 bits

                        bKeyBuf[0] = Keypad_NumCode[bKeyPos];
                        Wiegand_MSB(bKeyBuf, 4, false);
                        break;

                      case 0x01:    // Wiegand 6 bits (4 bits + 2 parity)

                        bKeyBuf[0] = Keypad_NumCode[bKeyPos];
                        Wiegand_MSB(bKeyBuf, 4, true);
                        break;

                      case 0x02:    // Wiegand 8 bits

                        bKeyBuf[0] = (((~Keypad_NumCode[bKeyPos]) << 4) & 0xF0) | Keypad_NumCode[bKeyPos];
                        Wiegand_MSB(bKeyBuf, 8, false);
                        break;

                      case 0x03:    // ASCII

                        bKeyBuf[0] = Keypad_AscCode[bKeyPos];
                        bKeyBuf[1] = 0x00;
                        bKeyBuf[2] = g_tRegisters.s1.bSite_Code;

                        m_bSdlen = 3;
                        memset(m_bTmpbuf, 0x00, MAX_USER_BUF);
                        memcpy(m_bTmpbuf, bKeyBuf, m_bSdlen);
                        status = PH_ERR_SUCCESS;

                        Data_Output(status);
                        break;

                      case 0x04:    // Key buffering with DEC
                      case 0x05:    // Key buffering with BCD

                        if((Keypad_AscCode[bKeyPos] >= '0') && (Keypad_AscCode[bKeyPos] <= '9'))
                        {
                            if(bVerifyMode == false)
                            {
                                // Enable verify mode by pressing key
                                if(Keypad_AscCode[bKeyPos] != '0')
                                {
                                    ulCONFIG_Tick = phDriver_GetTick();
                                    bConfig_TimeSec = 10 * 2;

                                    bVerifyMode = true;
                                }
                            }

                            if((bVerifyMode == true) && (dwPassCode == 0))          // Verify mode is enabled by pressing key
                            {
                                if(bKeyFlag == 0x04)
                                {
                                    dwKeyData *= 10;
                                    dwKeyData += Keypad_AscCode[bKeyPos] - '0';

                                    if(dwKeyData > 0xFFFF)
                                    {
                                        dwKeyData = 0;
                                        bVerifyMode = false;

                                        LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
                                    }
                                }
                                else    // (bKeyFlag == 0x05)
                                {
                                    dwKeyData *= 16;
                                    dwKeyData += Keypad_AscCode[bKeyPos] - '0';

                                    if(dwKeyData > 0x99999999)
                                    {
                                        dwKeyData = 0;
                                        bVerifyMode = false;

                                        LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
                                    }
                                }
                            }
                            else if((bVerifyMode == true) && (dwPassCode > 0))     // Verify mode is enabled by reading card
                            {
                                dwKeyData *= 10;
                                dwKeyData += Keypad_AscCode[bKeyPos] - '0';

                                if(dwKeyData > 99999999)
                                {
                                    dwKeyData = 0;
                                    dwPassCode = 0;
                                    bVerifyMode = false;

                                    LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
                                }
                            }
                        }
                        else if(Keypad_AscCode[bKeyPos] == '#')         // OK
                        {
                            if((bVerifyMode == true) && (dwPassCode == 0))          // Verify mode is enabled by pressing key
                            {
                                if(g_tRegisters.s1.Alignment & Bit2)    // it's buffering
                                {
                                    /* Not yet supported */
                                }
                                else
                                {
                                    if(bKeyFlag == 0x04)
                                        bDataLen = 2;
                                    else    // (bKeyFlag == 0x05)
                                        bDataLen = 4;

                                    memcpy(bKeyBuf, &dwKeyData, bDataLen);

                                    if(g_tRegisters.s1.bSite_Code != 0)
                                        bKeyBuf[bDataLen++] = g_tRegisters.s1.bSite_Code;

                                    m_bSdlen = bDataLen;
                                    memset(m_bTmpbuf, 0x00, MAX_USER_BUF);
                                    memcpy(m_bTmpbuf, bKeyBuf, m_bSdlen);
                                    status = PH_ERR_SUCCESS;

                                    Data_Output(status);
                                }

                                dwKeyData = 0;
                                bVerifyMode = false;

                                LedStatusCheck(LEDBUZ_STATUS_KEYPAD, true);     // Forced to true since LED isn't changed to return normal status
                            }
                            else if((bVerifyMode == true) && (dwPassCode > 0))     // Verify mode is enabled by reading card
                            {
                                if(dwKeyData == dwPassCode)
                                    status = PH_ERR_SUCCESS;
                                else
                                    status = PH_ERR_KEY;

                                status = Data_Output(status);

                                phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 50, NULL);

                                Status_Output(status);

                                dwKeyData = 0;
                                dwPassCode = 0;
                                bVerifyMode = false;
                            }
                        }
                        else if(Keypad_AscCode[bKeyPos] == '*')         // Cancel
                        {
                            if(bVerifyMode == true)
                            {
                                dwKeyData = 0;
                                dwPassCode = 0;
                                bVerifyMode = false;

                                LedStatusCheck(LEDBUZ_STATUS_INVALID, false);
                            }
                        }
                        break;
                    }
                }
            }
        }
        else if(bLoopKYPD == LOOP_SENTEVT)
        {
            // Check C1~C3 are all released
            if ((phDriver_PinRead(PHDRIVER_PIN_C1, PH_DRIVER_PINFUNC_INPUT) == COL_KEY_RELEASE_LEVEL)
             && (phDriver_PinRead(PHDRIVER_PIN_C2, PH_DRIVER_PINFUNC_INPUT) == COL_KEY_RELEASE_LEVEL)
             && (phDriver_PinRead(PHDRIVER_PIN_C3, PH_DRIVER_PINFUNC_INPUT) == COL_KEY_RELEASE_LEVEL))
            {
                bLoopKYPD = LOOP_DISABLE;
                ulKYSCAN_Tick = phDriver_GetTick();
            }
        }
        else
        {
            if (IsTimeout_ms(ulKYSCAN_Tick, 5))     // refer to 5 ms used by DF750K
            {
                // Continue to scan keypad
                if (++bKYPD_Row > 4)
                    bKYPD_Row = 1;

                switch (bKYPD_Row)
                {
                  case 1:
                    phDriver_PinWrite(PHDRIVER_PIN_R4, ROW_SCAN_DIS_LEVEL);
                    phDriver_PinWrite(PHDRIVER_PIN_R1, ROW_SCAN_EN_LEVEL);
                    break;
                  case 2:
                    phDriver_PinWrite(PHDRIVER_PIN_R1, ROW_SCAN_DIS_LEVEL);
                    phDriver_PinWrite(PHDRIVER_PIN_R2, ROW_SCAN_EN_LEVEL);
                    break;
                  case 3:
                    phDriver_PinWrite(PHDRIVER_PIN_R2, ROW_SCAN_DIS_LEVEL);
                    phDriver_PinWrite(PHDRIVER_PIN_R3, ROW_SCAN_EN_LEVEL);
                    break;
                  case 4:
                    phDriver_PinWrite(PHDRIVER_PIN_R3, ROW_SCAN_DIS_LEVEL);
                    phDriver_PinWrite(PHDRIVER_PIN_R4, ROW_SCAN_EN_LEVEL);
                    break;
                  default:
                    break;
                }

                ulKYSCAN_Tick = phDriver_GetTick();
            }
        }
        #endif

        /* Do Alive Task */
        if(bLoopLIVE != LOOP_DISABLE)
        {
            if(IsTimeout_ms(ulALIVE_Tick, ulALIVE_Timeout))
            {
                Wiegand_MSB(&bLiveVal, 8, false);

                ulALIVE_Tick = phDriver_GetTick();
            }
        }

	}

}


