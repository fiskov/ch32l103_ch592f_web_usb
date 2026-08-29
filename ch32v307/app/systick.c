/********************************** (C) COPYRIGHT *******************************
 * File Name          : systick.c
 * Description        : Free-running 1ms tick counter on TIM2 (CH32V307 port
 *                       of the ch585m systick: same API, peripheral timer
 *                       instead of a CH58x TMR).
 *******************************************************************************/
#include "ch32v30x.h"
#include "systick.h"

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static volatile uint32_t s_millis = 0;

void SysTick_InitMillis(void)
{
    s_millis = 0;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    {
        TIM_TimeBaseInitTypeDef t = {0};
        t.TIM_Prescaler = 144 - 1;            /* 144 MHz -> 1 MHz */
        t.TIM_Period = 1000 - 1;              /* 1 MHz -> 1 kHz   */
        t.TIM_ClockDivision = TIM_CKD_DIV1;
        t.TIM_CounterMode = TIM_CounterMode_Up;
        TIM_TimeBaseInit(TIM2, &t);
    }

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_ClearFlag(TIM2, TIM_IT_Update);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        s_millis++;
    }
}

uint32_t SysTick_Millis(void)
{
    return s_millis;
}
