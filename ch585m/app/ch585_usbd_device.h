/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch585_usbd_device.h
 * Description        : Header file for ch585_usbd_device.c. CH592F port of
 *                       ../../ch32l103/User/ch32l103_usbfs_device.[ch],
 *                       built on top of the CH592 StdPeriphDriver's native
 *                       USB device registers (R8_USB_*, R8_UEPn_*) instead
 *                       of the CH32L103's USBFSD/USBFSH register blocks.
 *******************************************************************************/
#ifndef APP_CH592_USBD_DEVICE_H_
#define APP_CH592_USBD_DEVICE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "CH58x_common.h"
#include "usb_desc.h"

/* end-point number */
#define DEF_UEP_IN                    0x80
#define DEF_UEP_OUT                   0x00
#define DEF_UEP0                      0x00
#define DEF_UEP1                      0x01
#define DEF_UEP2                      0x02

/*******************************************************************************/
/* Variable Definition */
extern const uint8_t    *pUSBD_Descr;

extern volatile uint8_t  USBD_DevConfig;
extern volatile uint8_t  USBD_DevAddr;
extern volatile uint8_t  USBD_DevSleepStatus;
extern volatile uint8_t  USBD_DevEnumStatus;

/* Set when an EP1 IN transfer is in flight; cleared once the host has
 * acknowledged it. Used by button.c to avoid overwriting a pending packet. */
extern volatile uint8_t  USBD_EP1_TxBusy;

/* Callback used to fill each outgoing EP2 (bulk IN) packet on demand.
 * Called both to arm the very first packet (via USBD_EP2_StartTransfer)
 * and to refill subsequent packets as soon as the host ACKs the previous
 * one, keeping the pipe continuously full for maximum bulk throughput.
 * 'buf' has room for up to 'maxlen' bytes (== DEF_USBD_UEP2_SIZE); return
 * the number of bytes actually written, or 0 to indicate end-of-data
 * (EP2 will then NAK until USBD_EP2_StartTransfer() is called again).      */
typedef uint16_t (*USBD_EP2_FillCallback)(uint8_t *buf, uint16_t maxlen);

/******************************************************************************/
/* external functions */

/* Initializes USB device endpoints/registers and enables the USB IRQ. */
extern void USBD_Device_Init(void);

/* Queues 'len' bytes from 'pbuf' for transmission on EP1 IN. Returns 0 on
 * success, 1 if a previous EP1 transfer is still pending (not yet ACKed).  */
extern uint8_t USBD_EP1_SendData(const uint8_t *pbuf, uint8_t len);

/* Registers the callback used to fill EP2 (bulk IN) packets on demand. */
extern void USBD_EP2_SetFillCallback(USBD_EP2_FillCallback cb);

/* (Re)starts an EP2 bulk IN transfer from the beginning: calls the fill
 * callback for the first packet and arms EP2 to send it. Call this once
 * per logical "download" (e.g. on the START_FILE_TRANSFER vendor
 * request); subsequent packets are then generated and sent automatically
 * from the USB interrupt handler as the host ACKs each one.               */
extern void USBD_EP2_StartTransfer(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CH592_USBD_DEVICE_H_ */
