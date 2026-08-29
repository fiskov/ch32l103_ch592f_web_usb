/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.h
 * Description        : LED brightness control on PB23 via the TMR0 hardware PWM
 *                       channel (alternate mapping). CH585M port of
 *                       ../../ch32l103/User/led_pwm.[ch] (there PB8/TIM4_CH3
 *                       hardware PWM; PB23 on CH585M carries TMR0's alternate
 *                       PWM0 output, so brightness is generated directly by the
 *                       TMR0 PWM peripheral with no ISR involvement).
 *******************************************************************************/
#ifndef APP_LED_PWM_H_
#define APP_LED_PWM_H_

#include "CH58x_common.h"

/* Remaps TMR0/PWM0 to PB23, configures it as a push-pull output and starts the
 * TMR0 hardware PWM with 0% duty (LED off). */
void LED_PWM_Init(void);

/* Sets LED brightness, 0 (off) .. 255 (full brightness). The first call
 * (typically a SET_LED vendor request from the host) also permanently
 * disables the boot heartbeat blink. */
void LED_PWM_SetBrightness(uint8_t brightness);

/* Scheduler task body: toggles the LED between fully off and fully on while
 * the heartbeat is active; no-op after the first LED_PWM_SetBrightness(). */
void LED_PWM_HeartbeatTick(void);

/* Returns the last brightness value set via LED_PWM_SetBrightness(). */
uint8_t LED_PWM_GetBrightness(void);

#endif /* APP_LED_PWM_H_ */
