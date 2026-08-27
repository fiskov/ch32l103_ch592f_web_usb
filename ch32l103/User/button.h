/********************************** (C) COPYRIGHT *******************************
 * File Name          : button.h
 * Description        : Debounced push-button on PA1 (wired to GND, using the
 *                       internal pull-up), sending a 4-byte timestamp event
 *                       over EP1 IN each time it is pressed.
 *******************************************************************************/
#ifndef USER_BUTTON_H_
#define USER_BUTTON_H_

#include "debug.h"

/* Default debounce interval, in milliseconds. Adjustable at runtime via
 * Button_SetDebounceMs() / the SET_DEBOUNCE_MS vendor request.            */
#define BUTTON_DEFAULT_DEBOUNCE_MS   5u

/* Configures PA1 as an input with internal pull-up and registers the
 * periodic debounce-poll task with the scheduler (see shed.h). Must be
 * called once after the scheduler/SysTick are ready. */
void Button_Init(void);

/* Changes the debounce polling interval (takes effect on the next poll). */
void Button_SetDebounceMs(uint32_t debounce_ms);

/* Returns the raw (non-debounced) instantaneous pin state: 1 = released
 * (pulled up), 0 = pressed (shorted to GND). Useful for diagnostics. */
uint8_t Button_ReadRaw(void);

#endif /* USER_BUTTON_H_ */
