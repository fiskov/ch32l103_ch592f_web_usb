/********************************** (C) COPYRIGHT *******************************
 * File Name          : uart_tx.c
 * Description        : Non-blocking interrupt-driven UART1 TX.
 *                       Software ring buffer + THR-empty interrupt.
 *                       Uses the UART1 hardware FIFO (7 bytes deep).
 *                       printf never blocks the CPU.
 *******************************************************************************/
#include "CH59x_common.h"
#include "uart_tx.h"

#define TX_BUF_SIZE  256
#define TX_BUF_MASK  (TX_BUF_SIZE - 1)

static char s_txBuf[TX_BUF_SIZE];
static volatile uint16_t s_head = 0;   /* producer (printf) write index */
static volatile uint16_t s_tail = 0;   /* consumer (ISR) read index */

void UART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void UartTx_Init(void)
{
    /* enable FIFO + trigger THR-empty interrupt when FIFO has space */
    R8_UART1_FCR = (2 << 6) | RB_FCR_FIFO_EN | RB_FCR_RX_FIFO_CLR | RB_FCR_TX_FIFO_CLR;
    /* note: RB_FCR_TX_FIFO_CLR and RX_FIFO_CLR are one-shot, self-clearing */

    s_head = 0;
    s_tail = 0;

    /* don't enable TX interrupt yet — only when we have data to send */
    PFIC_EnableIRQ(UART1_IRQn);
}

int UartTx_Write(const char *buf, int len)
{
    int i;
    uint16_t head = s_head;

    for (i = 0; i < len; i++)
    {
        uint16_t next = (head + 1) & TX_BUF_MASK;
        if (next == s_tail) break;  /* ring full: drop the rest */
        s_txBuf[head] = buf[i];
        head = next;
    }
    s_head = head;

    /* kick the interrupt if not already running */
    if (s_head != s_tail && (R8_UART1_IER & RB_IER_THR_EMPTY) == 0)
    {
        R8_UART1_IER |= RB_IER_THR_EMPTY;  /* fires immediately if FIFO has space */
    }
    return i;
}

int UartTx_Busy(void)
{
    return s_head != s_tail;
}

void UART1_IRQHandler(void)
{
    /* fill the hardware FIFO from the software ring */
    while (s_head != s_tail && R8_UART1_TFC < 7)  /* while data + FIFO space */
    {
        R8_UART1_THR = s_txBuf[s_tail];
        s_tail = (s_tail + 1) & TX_BUF_MASK;
    }

    if (s_head == s_tail)
    {
        /* ring empty: disable TX interrupt (re-enabled by Write) */
        R8_UART1_IER &= ~RB_IER_THR_EMPTY;
    }

    /* THR-empty is level-based: it clears itself when the FIFO
     * has data or the interrupt is disabled. No explicit clear needed. */
}
