/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/07/31
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2024 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/* @Note
 * UDisk Example:
 * This program provides examples of UDisk.Supports external SPI Flash and internal
 * Flash, selected by STORAGE_MEDIUM at SW_UDISK.h.
 *
 * If the USB is set to high-speed, an external crystal oscillator is recommended for the clock source.
 *  */

#include <ch585_usbhs_device.h>
#include "CH58x_common.h"
#include "Internal_Flash.h"
#include "SPI_FLASH.h"
#include "SW_UDISK.h"
#include "systick.h"
#include "shed.h"

/*********************************************************************
 * @fn      DebugInit
 *
 * @brief   调试初始化
 *
 * @return  none
 */
void DebugInit(void)
{
    /* UART0 debug: TX on PB7, RX on PB4. */
    GPIOB_SetBits(GPIO_Pin_7);
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
}

/* On-board LED on PB17: toggled once per second from the scheduler as a
 * visible sign of life (plain GPIO blink, no PWM needed for MSC). */
#define LED_PIN   GPIO_Pin_17

static void LED_BlinkTask(void)
{
    GPIOB_InverseBits(LED_PIN);
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    SetSysClock(SYSCLK_FREQ);
    DebugInit();

    GPIOB_SetBits(LED_PIN); /* LED idle high, blink starts after 500 ms */
    GPIOB_ModeCfg(LED_PIN, GPIO_ModeOut_PP_5mA);
    SysTick_InitMillis();
    shed_add("ledblink", LED_BlinkTask, 500, 1 /* repeat */);

    PRINT("LED heartbeat: PB17\r\n");

#if (STORAGE_MEDIUM == MEDIUM_SPI_FLASH)
    PRINT( "USBHS UDisk Demo\r\nStorage Medium: SPI FLASH \r\n" );
    /* SPI flash init */
    FLASH_Port_Init( );
    /* FLASH ID check */
    FLASH_IC_Check( );

#elif (STORAGE_MEDIUM == MEDIUM_INTERAL_FLASH)
    PRINT( "USBHS UDisk Demo\r\nStorage Medium: INTERNAL FLASH \r\n" );
    Flash_Sector_Count = IFLASH_UDISK_SIZE  / DEF_UDISK_SECTOR_SIZE;
    Flash_Sector_Size = DEF_UDISK_SECTOR_SIZE;
#endif

    /* Enable Udisk */
    Udisk_Capability = Flash_Sector_Count;
    Udisk_Status |= DEF_UDISK_EN_FLAG;

    USBHS_Device_Init(ENABLE);
    PFIC_EnableIRQ( USB2_DEVICE_IRQn );

    while (1)
    {
        shed_update(SysTick_Millis());
    }
}
