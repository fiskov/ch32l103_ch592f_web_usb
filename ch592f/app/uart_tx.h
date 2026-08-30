#ifndef UART_TX_H_
#define UART_TX_H_
#include <stdint.h>

/* Non-blocking interrupt-driven UART1 TX with ring buffer. */
void UartTx_Init(void);
int  UartTx_Write(const char *buf, int len);
int  UartTx_Busy(void);

#endif
