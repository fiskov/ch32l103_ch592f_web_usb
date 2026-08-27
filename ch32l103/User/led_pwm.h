/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.h
 * Description        : LED brightness control on PB8 via TIM4 Channel 3 PWM.
 *******************************************************************************/
#ifndef USER_LED_PWM_H_
#define USER_LED_PWM_H_

#include "debug.h"

/* Initializes PB8 as TIM4_CH3 PWM output and starts the timer with 0% duty. */
void LED_PWM_Init(void);

/* Sets LED brightness, 0 (off) .. 255 (full brightness). */
void LED_PWM_SetBrightness(uint8_t brightness);

/* Returns the last brightness value set via LED_PWM_SetBrightness(). */
uint8_t LED_PWM_GetBrightness(void);

#endif /* USER_LED_PWM_H_ */
