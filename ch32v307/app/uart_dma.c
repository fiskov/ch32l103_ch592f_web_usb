/********************************** (C) COPYRIGHT *******************************
 * File Name          : uart_dma.c
 * Description        : Non-blocking DMA-backed UART3 TX for the CH32V307.
 *                       printf goes through a 512-byte ring buffer; DMA
 *                       Channel 2 feeds USART3 in the background. The CPU
 *                       never blocks on a UART character.
 *
 *                       DMA1_Channel2 is the fixed USART3_TX mapping.
 *                       The USART3 DMAT bit in CTLR3 gates the request.
 *******************************************************************************/
#include "ch32v30x.h"
#include "uart_dma.h"
#include <string.h>

#define UART_DMA_BUF_SIZE  512   /* power of 2 */
#define UART_DMA_BUF_MASK  (UART_DMA_BUF_SIZE - 1)

__attribute__((aligned(4))) static char s_txBuf[UART_DMA_BUF_SIZE];
static volatile uint16_t s_head = 0;  /* producer (printf) write index */
static volatile uint16_t s_tail = 0;  /* consumer (DMA completion) read index */
static volatile uint8_t  s_dmaActive = 0;

/* DMA transfer-complete interrupt: advance tail, chain next chunk if pending */
void DMA1_Channel2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void UartDma_StartChunk(void)
{
    uint16_t avail = (s_head - s_tail) & UART_DMA_BUF_MASK;
    uint16_t chunk;
    uint16_t tail = s_tail;

    if (avail == 0)
    {
        s_dmaActive = 0;
        return;
    }

    /* chunk up to buffer-end (avoid wrap in a single DMA transfer) */
    chunk = UART_DMA_BUF_SIZE - tail;
    if (chunk > avail) chunk = avail;

    DMA1_Channel2->CFGR &= ~DMA_CFGR1_EN;
    DMA1_Channel2->CNTR = chunk;
    DMA1_Channel2->MADDR = (uint32_t)&s_txBuf[tail];
    DMA1_Channel2->CFGR |= DMA_CFGR1_EN;
    s_dmaActive = 1;
}

/* Called from the DMA ISR: a chunk finished transmitting */
void DMA1_Channel2_IRQHandler(void)
{
    if (DMA1->INTFR & DMA1_FLAG_TC2)
    {
        DMA1->INTFCR = DMA1_FLAG_TC2;
        s_tail = (s_tail + DMA1_Channel2->CNTR) & UART_DMA_BUF_MASK;
        /* wait for USART3 to finish shifting the last byte */
        while (!(USART3->STATR & USART_FLAG_TC)) {}
        UartDma_StartChunk();  /* chain next chunk or go idle */
    }
}

/* Blocking-safe write: copies into the ring, kicks DMA if idle.
 * Drops data if the ring is full (debug output: better to lose a
 * line than block the CPU). Returns bytes accepted. */
int UartDma_Write(const char *buf, int len)
{
    int i;
    uint16_t head = s_head;

    for (i = 0; i < len; i++)
    {
        uint16_t next = (head + 1) & UART_DMA_BUF_MASK;
        if (next == s_tail) break;  /* ring full: drop the rest */
        s_txBuf[head] = buf[i];
        head = next;
    }
    s_head = head;

    if (!s_dmaActive)
    {
        NVIC_DisableIRQ(DMA1_Channel2_IRQn);
        UartDma_StartChunk();
        NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    }
    return i;
}

void UartDma_Init(void)
{
    DMA_InitTypeDef d = {0};
    NVIC_InitTypeDef n = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* USART3 TX on DMA1_Channel2 (fixed hardware mapping) */
    DMA_DeInit(DMA1_Channel2);
    d.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DATAR;
    d.DMA_MemoryBaseAddr = (uint32_t)s_txBuf;
    d.DMA_DIR = DMA_DIR_PeripheralDST;
    d.DMA_BufferSize = 1;             /* set per chunk in StartChunk() */
    d.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    d.DMA_MemoryInc = DMA_MemoryInc_Enable;
    d.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    d.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    d.DMA_Mode = DMA_Mode_Normal;
    d.DMA_Priority = DMA_Priority_Low;
    d.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel2, &d);

    /* enable transfer-complete interrupt */
    DMA_ITConfig(DMA1_Channel2, DMA_IT_TC, ENABLE);
    n.NVIC_IRQChannel = DMA1_Channel2_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority = 1;
    n.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&n);

    /* enable USART3 DMA transmit request */
    USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);

    s_head = 0;
    s_tail = 0;
    s_dmaActive = 0;
}

int UartDma_Busy(void)
{
    return s_dmaActive;
}

uint16_t UartDma_Space(void)
{
    return (UART_DMA_BUF_MASK - ((s_head - s_tail) & UART_DMA_BUF_MASK));
}
