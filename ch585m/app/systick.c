/********************************** (C) COPYRIGHT *******************************
 * File Name          : systick.c
 * Description        : Free-running 1ms tick counter, implemented with TMR2.
 *
 *  NOTE: this does NOT use the RISC-V SysTick hardware peripheral, even
 *  though the module is named systick.[ch] for naming symmetry with the
 *  CH32L103 sibling firmware (../../ch32l103/User/systick.c), where the
 *  same rationale applies to TIM2 vs the WCH SysTick peripheral: mDelayuS()/
 *  mDelaymS() (CH59x_sys.c), used elsewhere during init, use their own
 *  hardware and must not collide with a free-running millisecond counter.
 *
 *  TMR2 is unused elsewhere in this project (TMR1 drives the LED software
 *  PWM, TMR0/TMR3 are unused), so it is used here for the 1ms tick.
 *******************************************************************************/
#include "systick.h"

void TMR2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static volatile uint32_t s_millis = 0;

void SysTick_InitMillis(void)
{
    s_millis = 0;

    /* TMR2_TimerInit() takes a reload value in system-clock cycles; program
     * it for exactly 1ms. */
    TMR2_TimerInit(GetSysClock() / 1000u);

    TMR2_ClearITFlag(TMR0_3_IT_CYC_END);
    TMR2_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR2_IRQn);
}

void TMR2_IRQHandler(void)
{
    if (TMR2_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR2_ClearITFlag(TMR0_3_IT_CYC_END);
        s_millis++;
    }
}

uint32_t SysTick_Millis(void)
{
    return s_millis;
}
