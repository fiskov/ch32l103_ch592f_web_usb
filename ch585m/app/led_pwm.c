/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.c
 * Description        : LED control on PB23 (plain GPIO - the CH585M board's
 *                       LED pin carries no hardware PWM channel, so for this
 *                       throughput-comparison port the brightness API is
 *                       quantized: 0 = off, any other value = fully on).
 *                       Same public API as ../../ch592f/app/led_pwm.c, so
 *                       usbd_winusb.c is identical on both targets.
 *******************************************************************************/
#include "led_pwm.h"

/* PB23 is the on-board LED pin on the CH585M demoboard. */
#define LED_PORT_PIN        GPIO_Pin_23

static volatile uint8_t s_brightness = 0;

/* Heartbeat blink state: enabled at boot as a visible sign of life,
 * permanently disabled by the first explicit LED_PWM_SetBrightness()
 * call (i.e. a SET_LED vendor request from the host). */
static volatile uint8_t s_heartbeat = 1;

static void LED_PWM_Apply(void)
{
    if (s_brightness == 0)
    {
        GPIOB_ResetBits(LED_PORT_PIN);
    }
    else
    {
        GPIOB_SetBits(LED_PORT_PIN);
    }
}

void LED_PWM_Init(void)
{
    GPIOB_SetBits(LED_PORT_PIN);
    GPIOB_ModeCfg(LED_PORT_PIN, GPIO_ModeOut_PP_20mA);

    s_brightness = 0x7F; 
    LED_PWM_Apply();

    TMR0_TimerInit(0); /* unused; keeps parity with nothing - placeholder */
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
