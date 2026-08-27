/********************************** (C) COPYRIGHT *******************************
 * File Name          : usbd_winusb.c
 * Description        : Vendor-request handling for MS OS 2.0 descriptors and
 *                       the LED brightness control requests. Identical logic
 *                       to ../../ch32l103/User/usbd_winusb.c.
 *******************************************************************************/
#include "usbd_winusb.h"
#include "usb_desc.h"
#include "led_pwm.h"
#include "version.h"
#include "button.h"
#include "systick.h"
#include "filexfer.h"

/* Single-byte scratch buffer used to answer GET_LED requests. */
static uint8_t s_getLedBuf[1];
/* 4-byte scratch buffer used to answer GET_MILLIS requests (diagnostic). */
static uint8_t s_millisBuf[4];
/* 4-byte scratch buffer used to answer GET_FILE_SIZE requests. */
static uint8_t s_fileSizeBuf[4];

/* 3-byte scratch buffer used to answer GET_VERSION requests: major/minor/patch. */
static const uint8_t s_versionBuf[3] = { FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH };

uint8_t WinUSB_ProcessVendorRequest(uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                     uint16_t wLength, const uint8_t **ppDescr, uint16_t *pLen)
{
    (void)wLength;

    /* Microsoft OS 2.0 Descriptor Set request:
     * bRequest must match the vendor code advertised in the BOS Platform
     * Capability descriptor, and wIndex must be 0x0007 as fixed by the
     * MS OS 2.0 specification. */
    if ((bRequest == MS_OS_20_VENDOR_CODE) && (wIndex == MS_OS_20_DESCRIPTOR_INDEX))
    {
        *ppDescr = MyMSOS20Descr;
        *pLen    = DEF_USBD_MSOS20_DESC_LEN;
        return WINUSB_REQ_HANDLED_DATA;
    }

    /* WebUSB GET_URL request:
     * bRequest must match the vendor code advertised in the BOS WebUSB
     * Platform Capability descriptor, and wIndex must be 0x0002 as fixed
     * by the WebUSB specification. wValue carries the descriptor index
     * (iLandingPage from the capability descriptor). */
    if ((bRequest == WEBUSB_VENDOR_CODE) && (wIndex == WEBUSB_GET_URL_INDEX))
    {
        if ((uint8_t)(wValue & 0xFF) == WEBUSB_LANDING_PAGE_INDEX)
        {
            *ppDescr = MyWebUSBURLDescr;
            *pLen    = DEF_USBD_WEBUSB_URL_DESC_LEN;
            return WINUSB_REQ_HANDLED_DATA;
        }
        return WINUSB_REQ_STALL;
    }

    switch (bRequest)
    {
        case VENDOR_REQUEST_SET_LED:
            /* Brightness is carried directly in wValue (0..255), no data stage. */
            LED_PWM_SetBrightness((uint8_t)(wValue & 0xFF));
            return WINUSB_REQ_HANDLED_ACK;

        case VENDOR_REQUEST_GET_LED:
            s_getLedBuf[0] = LED_PWM_GetBrightness();
            *ppDescr = s_getLedBuf;
            *pLen    = 1;
            return WINUSB_REQ_HANDLED_DATA;

        case VENDOR_REQUEST_GET_VERSION:
            *ppDescr = s_versionBuf;
            *pLen    = sizeof(s_versionBuf);
            return WINUSB_REQ_HANDLED_DATA;

        case VENDOR_REQUEST_SET_DEBOUNCE_MS:
            /* wValue carries the debounce polling interval directly, in ms. */
            Button_SetDebounceMs((uint32_t)(wValue & 0xFFFF));
            return WINUSB_REQ_HANDLED_ACK;

        case VENDOR_REQUEST_GET_BUTTON_RAW:
            s_getLedBuf[0] = Button_ReadRaw();
            *ppDescr = s_getLedBuf;
            *pLen    = 1;
            return WINUSB_REQ_HANDLED_DATA;

        case VENDOR_REQUEST_GET_MILLIS:
        {
            uint32_t ms = SysTick_Millis();
            s_millisBuf[0] = (uint8_t)(ms & 0xFF);
            s_millisBuf[1] = (uint8_t)((ms >> 8) & 0xFF);
            s_millisBuf[2] = (uint8_t)((ms >> 16) & 0xFF);
            s_millisBuf[3] = (uint8_t)((ms >> 24) & 0xFF);
            *ppDescr = s_millisBuf;
            *pLen    = 4;
            return WINUSB_REQ_HANDLED_DATA;
        }

        case VENDOR_REQUEST_GET_FILE_SIZE:
        {
            uint32_t sz = FILEXFER_TOTAL_SIZE;
            s_fileSizeBuf[0] = (uint8_t)(sz & 0xFF);
            s_fileSizeBuf[1] = (uint8_t)((sz >> 8) & 0xFF);
            s_fileSizeBuf[2] = (uint8_t)((sz >> 16) & 0xFF);
            s_fileSizeBuf[3] = (uint8_t)((sz >> 24) & 0xFF);
            *ppDescr = s_fileSizeBuf;
            *pLen    = 4;
            return WINUSB_REQ_HANDLED_DATA;
        }

        case VENDOR_REQUEST_START_FILE_XFER:
            FileXfer_Start();
            return WINUSB_REQ_HANDLED_ACK;

        default:
            return WINUSB_REQ_STALL;
    }
}
