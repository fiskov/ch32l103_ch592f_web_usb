/********************************** (C) COPYRIGHT *******************************
 * File Name          : usbd_winusb.h
 * Description        : Vendor-request handling for MS OS 2.0 descriptors and
 *                       the LED brightness control requests used by both the
 *                       Windows WinUSB driver and the WebUSB demo page.
 *                       Identical interface to
 *                       ../../ch32l103/User/usbd_winusb.[ch].
 *******************************************************************************/
#ifndef APP_USBD_WINUSB_H_
#define APP_USBD_WINUSB_H_

#include "CH58x_common.h"

/* Result codes returned by WinUSB_ProcessVendorRequest() */
#define WINUSB_REQ_STALL        0xFF   /* request not supported -> STALL          */
#define WINUSB_REQ_HANDLED_ACK  0x00   /* handled, no IN data (status stage ACK)  */
#define WINUSB_REQ_HANDLED_DATA 0x01   /* handled, output params hold IN data     */

/*********************************************************************
 * @fn      WinUSB_ProcessVendorRequest
 *
 * @brief   Handles all vendor-type (bmRequestType.Type == Vendor) control
 *          requests: the Microsoft OS 2.0 descriptor request, the WebUSB
 *          GET_URL request, and the application-specific LED/button/file
 *          transfer requests.
 *
 * @param   bRequest  - setup packet bRequest field
 * @param   wValue    - setup packet wValue field
 * @param   wIndex    - setup packet wIndex field
 * @param   wLength   - setup packet wLength field
 * @param   ppDescr   - [out] set to point at the data to return to the host
 *                       when the return value is WINUSB_REQ_HANDLED_DATA
 * @param   pLen      - [out] set to the number of bytes available at *ppDescr
 *
 * @return  WINUSB_REQ_STALL / WINUSB_REQ_HANDLED_ACK / WINUSB_REQ_HANDLED_DATA
 */
uint8_t WinUSB_ProcessVendorRequest(uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                     uint16_t wLength, const uint8_t **ppDescr, uint16_t *pLen);

#endif /* APP_USBD_WINUSB_H_ */
