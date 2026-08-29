/********************************** (C) COPYRIGHT *******************************
 * File Name          : led_pwm.h
 * Description        : LED control on PB23 (plain GPIO, quantized brightness).
 *                       CH585M port of ../../ch592f/app/led_pwm.[ch]; the
 *                       API matches so usbd_winusb.c is identical on both
 *                       targets (there PB23 is driven by TMR0 hardware PWM,
 *                       here 0 = off and any other value = fully on).
 *******************************************************************************/
#ifndef APP_LED_PWM_H_
#define APP_LED_PWM_H_

#include "CH58x_common.h"

/* Configures PB23 as a push-pull output, LED off. */
void LED_PWM_Init(void);

/* Sets LED brightness, 0 (off) .. 255 (full brightness; on this board any
 * non-zero value = fully on). The first call (typically a SET_LED vendor
 * request from the host) also permanently disables the boot heartbeat. */
void LED_PWM_SetBrightness(uint8_t brightness);

/* Returns the last brightness value set via LED_PWM_SetBrightness(). */
uint8_t LED_PWM_GetBrightness(void);

/* Scheduler task body: toggles the LED between fully off and fully on while
 * the heartbeat is active; no-op after the first LED_PWM_SetBrightness(). */
void LED_PWM_HeartbeatTick(void);

#endif /* APP_LED_PWM_H_ */
