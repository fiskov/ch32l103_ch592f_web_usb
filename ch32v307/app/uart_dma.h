#ifndef UART_DMA_H_
#define UART_DMA_H_
#include <stdint.h>

/* Initialize DMA1_Channel2 + USART3 DMAT for non-blocking TX. */
void UartDma_Init(void);

/* Non-blocking write: copies into ring, kicks DMA if idle.
 * Returns bytes accepted (may drop if ring is full). */
int UartDma_Write(const char *buf, int len);

/* True while DMA is actively transmitting. */
int UartDma_Busy(void);

/* Bytes of free space in the TX ring. */
uint16_t UartDma_Space(void);

#endif
