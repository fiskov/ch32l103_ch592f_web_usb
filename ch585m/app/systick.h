/********************************** (C) COPYRIGHT *******************************
 * File Name          : systick.h
 * Description        : Free-running 1ms tick counter, used as the time
 *                       base for the cooperative scheduler (shed.[ch]).
 *                       CH592F port of ../../ch32l103/User/systick.[ch]
 *                       (there implemented with TIM2; here with TMR2, the
 *                       CH592's equivalent general-purpose timer).
 *******************************************************************************/
#ifndef APP_SYSTICK_H_
#define APP_SYSTICK_H_

#include "CH58x_common.h"

/* Configures TMR2 to interrupt every 1ms and starts the millisecond
 * counter returned by SysTick_Millis(). Must be called once at startup. */
void SysTick_InitMillis(void);

/* Milliseconds elapsed since SysTick_InitMillis() was called. Wraps around
 * approximately every 49.7 days (uint32_t), which is not a concern for a
 * cooperative scheduler using 16/32-bit delta arithmetic.                */
uint32_t SysTick_Millis(void);

#endif /* APP_SYSTICK_H_ */
