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
#include "sched.h"
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
/* ---- CPU utilization measurement (same design as CH32V307) ----
 * TMR3 counts at 1 us (60 MHz / 60). Vendor request 0x0C returns
 * [idle_us, busy_us, isr_us, isr_count, timer_now]. */
#include "CH59x_timer.h"

#define PROF_TMR3_CNT  (*(volatile uint32_t *)0x40002C08)

volatile uint32_t prof_isr_us  = 0;
volatile uint32_t prof_isr_cnt = 0;
static volatile uint32_t prof_busy_us = 0;

static void Prof_Init(void)
{
    TMR3_TimerInit(0xFFFFFFFF); /* free-running, max period */
    /* TMR3_TimerInit sets the period; the clock is Fsys (60 MHz).
     * For 1 us resolution we need prescaler 60, but CH59x TMR3 has
     * no prescaler - it counts at Fsys directly. So each tick = 1/60 us.
     * We accumulate raw ticks and convert on read. */
}

static uint32_t Prof_Now(void) { return PROF_TMR3_CNT; }

/* called from the USB ISR instrumentation in ch592_usbd_device.c */
volatile uint32_t g_isrEntryTick = 0;

static uint8_t s_profBuf[20];
const uint8_t *Prof_GetData(void)
{
    /* read-and-clear: each vendor request 0x0C returns the values
     * accumulated since the last read, then resets them */
    uint32_t vals[5];
    PFIC_DisableIRQ(USB_IRQn);
    vals[0] = 0;
    vals[1] = prof_busy_us / 60u;
    vals[2] = prof_isr_us / 60u;
    vals[3] = prof_isr_cnt;
    vals[4] = Prof_Now() / 60u;
    prof_busy_us = 0;
    prof_isr_us = 0;
    prof_isr_cnt = 0;
    PFIC_EnableIRQ(USB_IRQn);
    memcpy(s_profBuf, vals, 20);
    return s_profBuf;
}

int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    DebugInit();
    printf("\nWinUSB/WebUSB LED demo (PB23 TMR0 PWM) v%u.%u.%u\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    Prof_Init();
    LED_PWM_Init();
    SysTick_InitMillis();

    sched_add("ledblink", LED_PWM_HeartbeatTick, 50, 1); /* 10 Hz toggle */

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
        sched_update(SysTick_Millis());
        FileXfer_Pump();
    }
}
