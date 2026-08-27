/********************************** (C) COPYRIGHT *******************************
 * File Name          : systick.c
 * Description        : Free-running 1ms tick counter, implemented with TIM2.
 *
 *  NOTE: this does NOT use the SysTick hardware peripheral, even though the
 *  module is named systick.[ch] for historical/API-naming reasons. The WCH
 *  Standard Peripheral Library's Delay_Us()/Delay_Ms() (in Debug/debug.c),
 *  which are called during USBFS_Device_Init(), reconfigure SysTick in a
 *  blocking one-shot mode and explicitly clear its enable bit (CTLR bit 0)
 *  when the delay completes. Since that enable bit is shared with any
 *  free-running SysTick-based millisecond counter, the two uses collide:
 *  the very first Delay_Us()/Delay_Ms() call after SysTick_InitMillis()
 *  silently disables the counter, permanently freezing SysTick_Millis().
 *
 *  TIM2 is unused elsewhere in this project (TIM4 drives the LED PWM), so
 *  it is used here instead to avoid any conflict with the delay functions.
 *******************************************************************************/
#include "systick.h"

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static volatile uint32_t s_millis = 0;

void SysTick_InitMillis(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    NVIC_InitTypeDef        NVIC_InitStructure = {0};

    s_millis = 0;

    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);

    /* Prescale the APB1 timer clock (== SystemCoreClock here, same
     * assumption used for TIM4 in led_pwm.c) down to 1MHz, then count
     * 1000 ticks for an exact 1ms period. */
    TIM_TimeBaseInitStructure.TIM_Period        = 1000 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler     = (SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    s_millis++;
}

uint32_t SysTick_Millis(void)
{
    return s_millis;
}
