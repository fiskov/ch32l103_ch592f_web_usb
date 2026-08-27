/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.c
 * Description        : LED brightness control on PB8 via TIM4 Channel 3 PWM.
 *
 *  PB8 is the default (non-remapped) location of TIM4_CH3 on CH32L103, so no
 *  AFIO remap is required. Timer is clocked from APB1 (RCC_PB1Periph_TIM4);
 *  GPIOB from APB2 (RCC_PB2Periph_GPIOB).
 *
 *  PWM frequency = TIMxCLK / ((PSC+1) * (ARR+1))
 *  With SystemCoreClock = 48 MHz (typical for these examples), APB1 timer
 *  clock (x1, since APB1 prescaler = 1) = 48 MHz.
 *  PSC = 47   -> 1 MHz counter clock
 *  ARR = 255  -> 8-bit resolution, ~3.9 kHz PWM (flicker-free on the eye)
 *
 *  The on-board LED is wired active-low (common for this kind of demo
 *  board: LED cathode toward the MCU pin, sourced from VCC through the
 *  resistor), so the timer output polarity is set to
 *  TIM_OCPolarity_Low. This makes brightness=0 drive the pin continuously
 *  high (LED fully OFF) and brightness=255 drive it (almost) continuously
 *  low (LED fully ON), while keeping the public API's 0=off/255=full-on
 *  convention unchanged from the caller's point of view.
 *******************************************************************************/

#include "led_pwm.h"

static uint8_t s_brightness = 0;

void LED_PWM_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef       TIM_OCInitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOB, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM4, ENABLE);

    /* PB8 = TIM4_CH3, alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period        = 255;   /* ARR: 8-bit duty resolution */
    TIM_TimeBaseInitStructure.TIM_Prescaler     = 47;    /* PSC: 48MHz / 48 = 1MHz     */
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode       = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState  = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse        = 0;             /* start at 0 = fully OFF (see polarity note above) */
    TIM_OCInitStructure.TIM_OCPolarity   = TIM_OCPolarity_Low;
    TIM_OC3Init(TIM4, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);

    s_brightness = 0;
}

void LED_PWM_SetBrightness(uint8_t brightness)
{
    s_brightness = brightness;
    TIM_SetCompare3(TIM4, brightness);
}

uint8_t LED_PWM_GetBrightness(void)
{
    return s_brightness;
}
