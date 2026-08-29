#ifndef APP_LED_PWM_H_
#define APP_LED_PWM_H_
#include "ch32v30x.h"
void LED_PWM_Init(void);
void LED_PWM_SetBrightness(uint8_t brightness);
uint8_t LED_PWM_GetBrightness(void);
void LED_PWM_HeartbeatTick(void);
#endif
