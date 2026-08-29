/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : WinUSB / WebUSB LED + file-transfer demo, CH32V307
 *                       port of ../ch585m/app/main.c. Same descriptors,
 *                       vendor protocol and zero-copy EP2 bulk path on the
 *                       USBHS high-speed peripheral. LED pin is a #define
 *                       in led_pwm.c (board LED not identified yet).
 *******************************************************************************/

#include "ch32v30x.h"
#include "debug.h"
#include "ch32v30x_usbhs_device.h"
#include "usb_desc.h"
#include "led_pwm.h"
#include "version.h"
#include "systick.h"
#include "sched.h"
#include "button.h"
#include "filexfer.h"

int main(void)
{
    /* SystemInit() ran from startup: 144 MHz HSE PLL */

    USART_Printf_Init(115200);
    printf("\nWinUSB/WebUSB LED demo (CH32V307) v%u.%u.%u\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    LED_PWM_Init();
    SysTick_InitMillis();
    sched_add("ledblink", LED_PWM_HeartbeatTick, 50, 1);

    USBD_Device_Init();

    Button_Init();
    FileXfer_Init();

    while (1)
    {
        sched_update(SysTick_Millis());
        FileXfer_Pump();
    }
}
