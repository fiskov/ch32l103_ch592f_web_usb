/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : WinUSB / WebUSB LED + file-transfer demo, CH585M port
 *                       of ../../ch592f/app/main.c. Same USB descriptors,
 *                       vendor protocol and ring-buffered EP2 bulk path as
 *                       the CH32L103 and CH592F siblings, on the CH585's
 *                       USB2 full-speed device peripheral - so host-side
 *                       throughput numbers are directly comparable.
 *
 *                       LED: PA9 (TMR0 hardware PWM), button: PB22 (input pull-up),
 *                       debug UART: UART0, TX on PB7, RX on PB4.
 *******************************************************************************/

#include "CH58x_common.h"
#include "ch585_usbhs_device.h"
#include "usb_desc.h"
#include "led_pwm.h"
#include "version.h"
#include "systick.h"
#include "sched.h"
#include "button.h"
#include "filexfer.h"

/* UART0 debug output on the remapped pins TX: PA14, RX: PA15, matching
 * the working test_585_tmr pinout on this demoboard. */
static void DebugInit(void)
{
    GPIOA_SetBits(GPIO_Pin_14);
    GPIOPinRemap(ENABLE, RB_PIN_UART0);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
}

int main(void)
{
    HSECFG_Capacitance(HSECap_18p); /* board's 32 MHz crystal needs its load caps */
    SetSysClock(SYSCLK_FREQ);

    DebugInit();
    printf("\nWinUSB/WebUSB LED demo (CH585M, PA9 TMR0 PWM) v%u.%u.%u\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);


    extern volatile uint32_t g_dbgStage;
    g_dbgStage = 1;
    LED_PWM_Init();
    SysTick_InitMillis();
    g_dbgStage = 2;
    sched_add("ledblink", LED_PWM_HeartbeatTick, 50, 1); /* 10 Hz toggle */

    USBD_Device_Init();
    g_dbgStage = 3;

    Button_Init();
    FileXfer_Init();

    while (1)
    {
        /* All USB work happens in the USB interrupt handler; the scheduler
         * drives the LED heartbeat and the debounced button poll, and
         * FileXfer_Pump() keeps the EP2 bulk ring topped up. */
        sched_update(SysTick_Millis());
        FileXfer_Pump();
    }
}
