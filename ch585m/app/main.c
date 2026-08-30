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

/* ---- CPU utilization measurement (TMR3 at 1 us) ---- */
#include "CH58x_timer.h"
#define PROF_TMR3_CNT  (*(volatile uint32_t *)0x40002C08)

volatile uint32_t prof_isr_us  = 0;
volatile uint32_t prof_isr_cnt = 0;
volatile uint32_t prof_busy_us = 0;

static void Prof_Init(void)
{
    TMR3_TimerInit(0xFFFFFFFF); /* free-running */
}

static uint8_t s_profBuf[20];
const uint8_t *Prof_GetData(void)
{
    uint32_t vals[5];
    PFIC_DisableIRQ(USB2_DEVICE_IRQn);
    vals[0] = 0;
    vals[1] = prof_busy_us / 78u;
    vals[2] = prof_isr_us / 78u;  /* 78 MHz ticks to us */
    vals[3] = prof_isr_cnt;
    vals[4] = PROF_TMR3_CNT / 78u;
    prof_busy_us = 0;
    prof_isr_us = 0;
    prof_isr_cnt = 0;
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
    memcpy(s_profBuf, vals, 20);
    return s_profBuf;
}

/* periodic CPU stats via UART (1 Hz) */
static void CPU_StatsTask(void)
{
    extern volatile uint32_t mainLoops;
    extern volatile uint32_t maxGap;
    /* approximate: at 78MHz, TMR3 ticks / 78 = microseconds */
    printf("cpu: loops=%lu maxGap=%luus(%.1luraw)\r\n",
           mainLoops, maxGap / 78u, maxGap);
    mainLoops = 0;
    maxGap = 0;
}

/* Measure LED blink periods to find the pause */
static uint32_t s_blinkCount = 0;
static uint32_t s_lastBlinkMs = 0;
static uint32_t s_maxBlinkGap = 0;
static uint32_t s_minBlinkGap = 0xFFFFFFFF;

static void LED_BlinkMeasure(void)
{
    uint32_t now = SysTick_Millis();
    uint32_t gap = now - s_lastBlinkMs;
    if (gap > s_maxBlinkGap) s_maxBlinkGap = gap;
    if (gap < s_minBlinkGap && gap > 0) s_minBlinkGap = gap;
    s_lastBlinkMs = now;
    s_blinkCount++;

    /* report every 5 seconds */
    if (s_blinkCount % 100 == 0)  /* 100 blinks × 50ms = 5s */
    {
        printf("blink: n=%lu gap=%lu..%lu (exp 50)\r\n",
               s_blinkCount, s_minBlinkGap, s_maxBlinkGap);
        s_maxBlinkGap = 0;
        s_minBlinkGap = 0xFFFFFFFF;
    }
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
    sched_add("blinkdbg", LED_BlinkMeasure, 50, 1);      /* same period, measures gap */

    USBD_Device_Init();
    g_dbgStage = 3;

    Button_Init();
    FileXfer_Init();

    while (1)
    {
        /* All USB work happens in the USB interrupt handler; the scheduler
         * drives the LED heartbeat and the debounced button poll, and
         * FileXfer_Pump() keeps the EP2 bulk ring topped up. */
        uint32_t t0 = PROF_TMR3_CNT;
        sched_update(SysTick_Millis());
        FileXfer_Pump();
        prof_busy_us += PROF_TMR3_CNT - t0;
    }
}
