/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.c
 * Description        : LED control for CH32V307 (plain GPIO toggle - board
 *                       LED pin not identified yet; TODO below). Same API
 *                       as the sibling projects: brightness quantized to
 *                       off/on, heartbeat until first SET_LED.
 *******************************************************************************/
#include "led_pwm.h"

/* TODO: set to the demoboard's LED pin. */
#define LED_PORT   GPIOA
#define LED_PIN    GPIO_Pin_1

static volatile uint8_t s_brightness = 0;
static volatile uint8_t s_heartbeat = 1;

static void LED_PWM_Apply(void)
{
    GPIO_WriteBit(LED_PORT, LED_PIN, s_brightness ? Bit_SET : Bit_RESET);
}

void LED_PWM_Init(void)
{
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = LED_PIN;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(LED_PORT, &g);
    s_brightness = 0;
    LED_PWM_Apply();
}

void LED_PWM_HeartbeatTick(void)
{
    if (!s_heartbeat) return;
    s_brightness = (s_brightness == 0) ? 0xFF : 0x00;
    LED_PWM_Apply();
}

void LED_PWM_SetBrightness(uint8_t brightness)
{
    s_brightness = brightness;
    s_heartbeat = 0;
    LED_PWM_Apply();
}

uint8_t LED_PWM_GetBrightness(void) { return s_brightness; }
