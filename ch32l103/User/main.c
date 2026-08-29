/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Main program body.
 *
 * @Note
 * WinUSB / WebUSB LED demo:
 *   - USB full-speed device exposing a single vendor-specific interface
 *     with EP0 control transfers (LED brightness) plus one interrupt IN
 *     endpoint (EP1, button-press notifications).
 *   - Advertises a BOS "Microsoft OS 2.0" platform capability descriptor so
 *     Windows automatically binds the WinUSB (winusb.sys) driver with no
 *     .inf file needed, alongside a WebUSB platform capability descriptor.
 *   - Because no kernel driver is required, the same device can also be
 *     opened directly from a browser using the WebUSB API (see the
 *     accompanying webusb-demo/ folder).
 *   - LED brightness on PB8 (TIM4_CH3 PWM) is controlled via two vendor
 *     control requests:
 *       SET_LED (bRequest=0x02): wValue = brightness 0..255, no data stage
 *       GET_LED (bRequest=0x03): device returns 1 byte with current value
 *   - A push-button on PA1 (internal pull-up, shorts to GND when pressed)
 *     is debounced in software (5ms poll by default) and sends a 4-byte
 *     millisecond timestamp event over EP1 IN on every press.
 *******************************************************************************/

#include "debug.h"
#include "ch32l103_usbfs_device.h"
#include "led_pwm.h"
#include "version.h"
#include "systick.h"
#include "sched.h"
#include "button.h"
#include "filexfer.h"

/*********************************************************************
 * @fn      main
 * @brief   Main program.
 * @return  none
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%lu\r\n", (unsigned long)SystemCoreClock);
    printf("ChipID:%08lx\r\n", (unsigned long)DBGMCU_GetCHIPID());
    printf("WinUSB/WebUSB LED demo (PB8 = TIM4_CH3 PWM) v%u.%u.%u\r\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    LED_PWM_Init();
    SysTick_InitMillis();

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE);

    Button_Init();
    FileXfer_Init();

    while (1)
    {
        /* LED PWM runs autonomously in hardware and most USB work happens
         * in the USB interrupt handler; the scheduler drives periodic
         * tasks such as the debounced button poll that sends events over
         * EP1. FileXfer_Pump() keeps the EP2 bulk-transfer packet ring
         * topped up in the background so the USB interrupt handler only
         * has to memcpy a pre-built packet, maximizing bulk throughput. */
        sched_update(SysTick_Millis());
        FileXfer_Pump();
    }
}
