/********************************** (C) COPYRIGHT *******************************
 * File Name          : button.c
 * Description        : Debounced push-button on PB22, sending a 4-byte
 *                       timestamp event over EP1 IN on each press.
 *
 *  PB22 is the CH592F's BOOT pin (sampled at reset to enter the ISP
 *  bootloader when held low): it is broken out on essentially every
 *  CH592F board/module and is convenient to reuse here as the demo's
 *  push-button, matching the request to use "the button on boot"
 *  (PB22/PB23) instead of adding a dedicated GPIO. It is configured as an
 *  input with the internal pull-up enabled, so it reads HIGH when idle and
 *  LOW when the button shorts it to GND - same polarity as PA1 on the
 *  CH32L103 sibling firmware (../../ch32l103/User/button.c).
 *
 *  Debounce strategy: identical to the CH32L103 firmware - a periodic
 *  scheduler task (see shed.h) samples the pin every
 *  BUTTON_DEFAULT_DEBOUNCE_MS milliseconds (5ms by default) and only
 *  reports a press once the pin has read LOW for two consecutive samples
 *  in a row while the previous stable state was "released".
 *******************************************************************************/
#include "button.h"
#include "sched.h"
#include "systick.h"
#include "ch32v30x_usbhs_device.h"

#define BUTTON_TASK_NAME   "btn"

#define BUTTON_PIN         GPIO_Pin_1   /* unpopulated on this board */

/* Number of consecutive LOW samples required before accepting a press,
 * and consecutive HIGH samples required before accepting a release. */
#define BUTTON_STABLE_SAMPLES   2

static uint32_t s_debounce_ms = BUTTON_DEFAULT_DEBOUNCE_MS;
static uint8_t  s_stableState = 1;   /* 1 = released (pulled up), 0 = pressed */
static uint8_t  s_candidateState = 1;
static uint8_t  s_candidateCount = 0;

static uint8_t Button_ReadPin(void)
{
    return GPIO_ReadInputDataBit(GPIOB, BUTTON_PIN) ? 1 : 0;
}

static void Button_SendEvent(void)
{
    uint32_t now_ms = SysTick_Millis();
    uint8_t  payload[4];

    payload[0] = (uint8_t)(now_ms & 0xFF);
    payload[1] = (uint8_t)((now_ms >> 8) & 0xFF);
    payload[2] = (uint8_t)((now_ms >> 16) & 0xFF);
    payload[3] = (uint8_t)((now_ms >> 24) & 0xFF);

    if (USBHS_DevEnumStatus)
    {
        USBD_EP1_SendData(payload, sizeof(payload));
    }
}

static void Button_PollTask(void)
{
    uint8_t pinState = Button_ReadPin(); /* 1=released, 0=pressed */

    if (pinState == s_candidateState)
    {
        if (s_candidateCount < 0xFF)
        {
            s_candidateCount++;
        }
    }
    else
    {
        s_candidateState = pinState;
        s_candidateCount = 1;
    }

    if ((s_candidateCount >= BUTTON_STABLE_SAMPLES) && (s_candidateState != s_stableState))
    {
        s_stableState = s_candidateState;
        if (s_stableState == 0) /* just became pressed */
        {
            Button_SendEvent();
        }
    }
}

void Button_Init(void)
{
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = BUTTON_PIN;
    g.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &g);

    s_stableState    = Button_ReadPin();
    s_candidateState = s_stableState;
    s_candidateCount = 0;

    sched_add(BUTTON_TASK_NAME, Button_PollTask, s_debounce_ms, 1 /* repeat */);
}

void Button_SetDebounceMs(uint32_t debounce_ms)
{
    if (debounce_ms == 0)
    {
        return;
    }
    s_debounce_ms = debounce_ms;
    sched_remove(BUTTON_TASK_NAME);
    sched_add(BUTTON_TASK_NAME, Button_PollTask, s_debounce_ms, 1 /* repeat */);
}

uint8_t Button_ReadRaw(void)
{
    return Button_ReadPin();
}
