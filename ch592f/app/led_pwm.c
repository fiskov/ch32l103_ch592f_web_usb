/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.c
 * Description        : LED brightness control on PA4 via a software PWM
 *                       driven from the TMR1 interrupt.
 *
 *  The on-board LED is wired active-low on PA4 (same pin used by the
 *  original ch592_usbhid firmware, see ../../ch592f original README:
 *  "LED | PA4 (active-low)"). None of CH592's fixed-function PWM4-PWM11
 *  hardware channels are routed to PA4 (see CH592SFR.h bPWM4../bPWM8..
 *  definitions - they live on PA6/PA7/PA12/PA13/PB0/PB1/PB3/PB4), so unlike
 *  the CH32L103 sibling firmware (../../ch32l103/User/led_pwm.c, PB8 =
 *  TIM4_CH3 hardware PWM) brightness here is produced by toggling the pin
 *  in software from a fast, free-running TMR1 interrupt:
 *
 *  - TMR1 is configured as a free-running timer reloading every
 *    LED_PWM_TICK_CYCLES system clocks (~device Tsys, see TMR1_TimerInit()).
 *  - Each interrupt increments an 8-bit counter (0..255) and drives PA4
 *    according to counter < brightness (inverted for active-low wiring),
 *    producing a standard 8-bit-resolution PWM waveform.
 *  - At SystemCoreClock = 60MHz and a ~468-cycle reload (~7.8us/tick), the
 *    full 256-step cycle repeats at roughly 500Hz - comfortably above the
 *    flicker-fusion threshold - while keeping ISR overhead low (~1 short
 *    interrupt every 7.8us).
 *
 *  The public API (0=off, 255=full brightness) matches led_pwm.c on the
 *  CH32L103 sibling firmware exactly, so usbd_winusb.c is identical on
 *  both targets.
 *******************************************************************************/

#include "led_pwm.h"

/* PA4 is the on-board LED pin (active-low), matching the original
 * ch592_usbhid firmware's DebugInit()/LED_SET()/LED_RESET() macros. */
#define LED_PORT_PIN        GPIO_Pin_4

/* TMR1 reload value: SystemCoreClock/128000 gives a ~128kHz tick rate, i.e.
 * a ~500Hz 8-bit PWM carrier (128000/256 ~= 500Hz) - flicker-free and low
 * enough overhead for a background LED brightness driver. */
#define LED_PWM_TICK_HZ      128000u

static volatile uint8_t s_brightness = 0;
static volatile uint8_t s_pwmCounter = 0;

void TMR1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static inline void LED_SetPinLevel(uint8_t on)
{
    /* Active-low: pin low turns the LED on. */
    if (on)
    {
        GPIOA_ResetBits(LED_PORT_PIN);
    }
    else
    {
        GPIOA_SetBits(LED_PORT_PIN);
    }
}

void TMR1_IRQHandler(void)
{
    if (TMR1_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR1_ClearITFlag(TMR0_3_IT_CYC_END);

        LED_SetPinLevel(s_pwmCounter < s_brightness);
        s_pwmCounter++;
    }
}

void LED_PWM_Init(void)
{
    GPIOA_ModeCfg(LED_PORT_PIN, GPIO_ModeOut_PP_5mA);
    LED_SetPinLevel(0);

    s_brightness = 0;
    s_pwmCounter = 0;

    TMR1_TimerInit(GetSysClock() / LED_PWM_TICK_HZ);
    TMR1_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR1_IRQn);
}

void LED_PWM_SetBrightness(uint8_t brightness)
{
    s_brightness = brightness;
}

uint8_t LED_PWM_GetBrightness(void)
{
    return s_brightness;
}
