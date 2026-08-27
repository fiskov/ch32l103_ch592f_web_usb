/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32l103_usbfs_device.h
 * Description        : Header file for ch32l103_usbfs_device.c
 *******************************************************************************/
#ifndef USER_CH32L103_USBFS_DEVICE_H_
#define USER_CH32L103_USBFS_DEVICE_H_
#ifdef __cplusplus
 extern "C" {
#endif

#include "debug.h"
#include "string.h"
#include "ch32l103_usb.h"
#include "usb_desc.h"

/******************************************************************************/
#ifndef __PACKED
  #define __PACKED   __attribute__((packed))
#endif

/* end-point number */
#define DEF_UEP_IN                    0x80
#define DEF_UEP_OUT                   0x00
#define DEF_UEP0                      0x00
#define DEF_UEP1                      0x01
#define DEF_UEP2                      0x02
#define DEF_UEP_NUM                   8

/* Setup Request Packet */
#define pUSBFS_SetupReqPak            ((PUSB_SETUP_REQ)USBFS_EP0_Buf)

/*******************************************************************************/
/* Variable Definition */
extern const uint8_t    *pUSBFS_Descr;

extern volatile uint8_t  USBFS_SetupReqCode;
extern volatile uint8_t  USBFS_SetupReqType;
extern volatile uint16_t USBFS_SetupReqValue;
extern volatile uint16_t USBFS_SetupReqIndex;
extern volatile uint16_t USBFS_SetupReqLen;

extern volatile uint8_t  USBFS_DevConfig;
extern volatile uint8_t  USBFS_DevAddr;
extern volatile uint8_t  USBFS_DevSleepStatus;
extern volatile uint8_t  USBFS_DevEnumStatus;

extern __attribute__ ((aligned(4))) uint8_t USBFS_EP0_Buf[];
extern __attribute__ ((aligned(4))) uint8_t USBFS_EP1_Buf[];
extern __attribute__ ((aligned(4))) uint8_t USBFS_EP2_Buf[];

/* Set when an EP1 IN transfer is in flight; cleared once the host has
 * acknowledged it. Used by button.c to avoid overwriting a pending packet. */
extern volatile uint8_t  USBFS_EP1_TxBusy;

/* Callback used to fill each outgoing EP2 (bulk IN) packet on demand.
 * Called from the USB interrupt handler both to arm the very first packet
 * (via USBFS_EP2_StartTransfer) and to refill subsequent packets as soon
 * as the host ACKs the previous one, so the pipe is kept continuously
 * full for maximum bulk throughput. 'buf' has room for up to 'maxlen'
 * bytes (== DEF_USBD_UEP2_SIZE); return the number of bytes actually
 * written, or 0 to indicate end-of-data (EP2 will then NAK until
 * USBFS_EP2_StartTransfer() is called again).                             */
typedef uint16_t (*USBFS_EP2_FillCallback)(uint8_t *buf, uint16_t maxlen);

/******************************************************************************/
/* external functions */
extern void USBFS_Device_Init( FunctionalState sta );
extern void USBFS_Device_Endp_Init(void);
extern void USBFS_RCC_Init(void);
extern void USBFS_Send_Resume(void);

/* Queues 'len' bytes from 'pbuf' for transmission on EP1 IN. Returns 0 on
 * success, 1 if a previous EP1 transfer is still pending (not yet ACKed).  */
extern uint8_t USBFS_EP1_SendData(const uint8_t *pbuf, uint8_t len);

/* Registers the callback used to fill EP2 (bulk IN) packets on demand. */
extern void USBFS_EP2_SetFillCallback(USBFS_EP2_FillCallback cb);

/* (Re)starts an EP2 bulk IN transfer from the beginning: calls the fill
 * callback for the first packet and arms EP2 to send it. Call this once
 * per logical "download" (e.g. on the START_FILE_TRANSFER vendor
 * request); subsequent packets are then generated and sent automatically
 * from the USB interrupt handler as the host ACKs each one.               */
extern void USBFS_EP2_StartTransfer(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_CH32L103_USBFS_DEVICE_H_ */
