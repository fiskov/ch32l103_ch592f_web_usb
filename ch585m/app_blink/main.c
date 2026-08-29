/* Minimal CH585M bring-up: LED blink on PB23, no USB/scheduler/UART.
 * Internal oscillator (no HSE dependency). g_heart counts toggles and is
 * readable over SWIO as an execution indicator. */
#include "CH58x_common.h"

volatile uint32_t g_heart = 0;

int main(void)
{
    SetSysClock(CLK_SOURCE_HSI_PLL_62_4MHz);

    GPIOB_SetBits(GPIO_Pin_23);
    GPIOB_ModeCfg(GPIO_Pin_23, GPIO_ModeOut_PP_20mA);

    while (1)
    {
        for (volatile uint32_t i = 0; i < 300000; i++)
        {
            __asm volatile("nop");
        }
        GPIOB_InverseBits(GPIO_Pin_23);
        g_heart++;
    }
}
