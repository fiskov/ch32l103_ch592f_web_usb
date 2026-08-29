/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : WinUSB / WebUSB LED + file-transfer demo, CH585M port
 *                       of ../../ch592f/app/main.c. Same USB descriptors,
 *                       vendor protocol and ring-buffered EP2 bulk path as
 *                       the CH32L103 and CH592F siblings, on the CH585's
 *                       USB2 full-speed device peripheral - so host-side
 *                       throughput numbers are directly comparable.
 *
 *                       LED: PB23 (TMR0 hardware PWM), button: PB22 (input pull-up),
 *                       debug UART: UART0, TX on PB7, RX on PB4.
 *******************************************************************************/

#include "CH58x_common.h"
#include "ch585_usbd_device.h"
#include "usb_desc.h"
#include "led_pwm.h"
#include "version.h"
#include "systick.h"
#include "shed.h"
#include "button.h"
#include "filexfer.h"

/* UART0 debug output (TX: PB7, RX: PB4), matching the EVT MSC example's
 * debug wiring on the CH585M demoboard. */
static void DebugInit(void)
{
    GPIOB_SetBits(GPIO_Pin_7);
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
}

int main(void)
{
    SetSysClock(SYSCLK_FREQ);

    DebugInit();
    printf("\nWinUSB/WebUSB LED demo (CH585M, PB23 TMR0 PWM) v%u.%u.%u\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    LED_PWM_Init();
    SysTick_InitMillis();
    shed_add("ledblink", LED_PWM_HeartbeatTick, 500, 1);

    USBD_Device_Init();

    Button_Init();
    FileXfer_Init();

    while (1)
    {
        /* All USB work happens in the USB interrupt handler; the scheduler
         * drives the LED heartbeat and the debounced button poll, and
         * FileXfer_Pump() keeps the EP2 bulk ring topped up. */
        shed_update(SysTick_Millis());
        FileXfer_Pump();
    }
}
