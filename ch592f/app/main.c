/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Description        : Main program body.
 *
 * @Note
 * WinUSB / WebUSB LED demo (CH592F port of ../../ch32l103/User/main.c):
 *   - USB full-speed device exposing a single vendor-specific interface
 *     with EP0 control transfers (LED brightness) plus one interrupt IN
 *     endpoint (EP1, button-press notifications) and one bulk IN endpoint
 *     (EP2, synthetic file download for throughput testing).
 *   - Advertises a BOS "Microsoft OS 2.0" platform capability descriptor so
 *     Windows automatically binds the WinUSB (winusb.sys) driver with no
 *     .inf file needed, alongside a WebUSB platform capability descriptor.
 *   - Because no kernel driver is required, the same device can also be
 *     opened directly from a browser using the WebUSB API.
 *   - LED brightness on PB23 (hardware PWM via remapped TMR0, see led_pwm.c)
 *     is controlled via two vendor control requests:
 *       SET_LED (bRequest=0x02): wValue = brightness 0..255, no data stage
 *       GET_LED (bRequest=0x03): device returns 1 byte with current value
 *   - A push-button on PB22 - the CH592F's BOOT pin (internal pull-up,
 *     shorts to GND when pressed) - is debounced in software (5ms poll by
 *     default) and sends a 4-byte millisecond timestamp event over EP1 IN
 *     on every press.
 *******************************************************************************/

#include "CH59x_common.h"
#include "ch592_usbd_device.h"
#include "usb_desc.h"
#include "led_pwm.h"
#include "version.h"
#include "systick.h"
#include "shed.h"
#include "button.h"
#include "filexfer.h"

/*********************************************************************
 * @fn      DebugInit
 * @brief   UART1 debug output (TX: PA9, RX: PA8), matching the original
 *          ch592_usbhid firmware's debug wiring. PA9 stays on UART1 TX; the
 *          on-board LED uses PB23 (remapped TMR0 PWM, see led_pwm.c).
 * @return  none
 */
static void DebugInit(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
}

/*********************************************************************
 * @fn      main
 * @brief   Main program.
 * @return  none
 */
int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    DebugInit();
    printf("\nWinUSB/WebUSB LED demo (PB23 TMR0 PWM) v%u.%u.%u\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    LED_PWM_Init();
    SysTick_InitMillis();

    shed_add("ledblink", LED_PWM_HeartbeatTick, 500, 1);

    USBD_Device_Init();

    Button_Init();
    FileXfer_Init();

    while (1)
    {
        /* LED PWM runs autonomously from the TMR0 hardware channel and most
         * USB work happens in the USB interrupt handler; the scheduler drives
         * periodic tasks such as the debounced button poll that sends
         * events over EP1. FileXfer_Pump() keeps the EP2 bulk-transfer
         * packet ring topped up in the background so the USB interrupt
         * handler only has to memcpy a pre-built packet, maximizing bulk
         * throughput. */
        shed_update(SysTick_Millis());
        FileXfer_Pump();
    }
}
