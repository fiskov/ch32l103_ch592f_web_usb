/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.c
 * Description        : LED brightness control on PB23 via the TMR0 hardware PWM
 *                       channel. CH585M port of ../../ch592f/app/led_pwm.c:
 *                       PB23 carries TMR0's alternate PWM0 output on this
 *                       chip too (RB_PIN_TMR0), so the driver is identical -
 *                       8-bit brightness, ~58.6 kHz carrier at 62.4 MHz.
 *******************************************************************************/
#include "led_pwm.h"

/* PB23 is the on-board LED pin (active-low) and TMR0's alternate PWM0 output. */
#define LED_PORT_PIN        GPIO_Pin_23

/* TMR0 PWM period in system clocks. 1024 ticks @60MHz ~= 58.6kHz carrier.
 * Multiple of 256 so an 8-bit brightness maps to an integer pulse width. */
#define LED_PWM_CYCLE       1024u
#define LED_PWM_SCALE       (LED_PWM_CYCLE / 256u)

static volatile uint8_t s_brightness = 0;

/* Heartbeat blink state: enabled at boot as a visible sign of life,
 * permanently disabled by the first explicit LED_PWM_SetBrightness()
 * call (i.e. a SET_LED vendor request from the host). */
static volatile uint8_t s_heartbeat = 1;

static void LED_PWM_Apply(void)
{
    TMR0_PWMActDataWidth((uint32_t)s_brightness * LED_PWM_SCALE);
}

void LED_PWM_Init(void)
{
    /* Route TMR0/PWM0 to PB23 instead of the default PA9 (PA9 stays on UART1
     * TX for debug output). */
    GPIOPinRemap(ENABLE, RB_PIN_TMR0);

    /* Enable the pin's push-pull output driver; TMR0 PWM0_ takes over the pin. */
    GPIOB_ModeCfg(LED_PORT_PIN, GPIO_ModeOut_PP_5mA);

    /* Low_Level polarity -> active (low) pulse width = data width, matching the
     * active-low LED. PWM_Times_1: one effective pulse per cycle. */
    TMR0_PWMInit(Low_Level, PWM_Times_1);
    TMR0_PWMCycleCfg(LED_PWM_CYCLE);

    s_brightness = 0x7F;
    LED_PWM_Apply();

    TMR0_PWMEnable();
    TMR0_Enable();
}

void LED_PWM_HeartbeatTick(void)
{
    if (!s_heartbeat)
    {
        return;
    }
    s_brightness = (s_brightness == 0) ? 0xFF : 0x00;
    LED_PWM_Apply();
}

void LED_PWM_SetBrightness(uint8_t brightness)
{
    s_brightness = brightness;
    s_heartbeat = 0;
    LED_PWM_Apply();
}

uint8_t LED_PWM_GetBrightness(void)
{
    return s_brightness;
}
