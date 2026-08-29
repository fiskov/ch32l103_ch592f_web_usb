/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch592_usbd_device.c
 * Description        : USB device stack: enumeration (EP0 control
 *                       transfers), MS OS 2.0/WebUSB descriptor support,
 *                       the application vendor requests (LED get/set,
 *                       etc.), EP1 interrupt IN (button events) and EP2
 *                       bulk IN (synthetic file download for throughput
 *                       testing). CH592F port of
 *                       ../../ch32l103/User/ch32l103_usbfs_device.c, using
 *                       the CH592 StdPeriphDriver's native USB device
 *                       registers (R8_USB_*, R8_UEPn_*) in place of the
 *                       CH32L103's USBFSD/USBFSH register blocks.
 *******************************************************************************/

#include "ch592_usbd_device.h"
#include "usbd_winusb.h"
#include <string.h>

/*******************************************************************************/
/* Variable Definition */

const uint8_t    *pUSBD_Descr;

static volatile uint8_t  s_SetupReqCode;
static volatile uint8_t  s_SetupReqType;
static volatile uint16_t s_SetupReqLen;

volatile uint8_t  USBD_DevConfig;
volatile uint8_t  USBD_DevAddr;
volatile uint8_t  USBD_DevSleepStatus;
volatile uint8_t  USBD_DevEnumStatus;

/* Endpoint RAM buffers: layout matches pEP0/1/2_RAM_Addr expectations of
 * the StdPeriphDriver (64 OUT + 64 IN per endpoint used). EP3 is unused by
 * this device but the 4-endpoint USB hardware still requires its DMA
 * pointer to be programmed to *something* valid at init time. */
__attribute__((aligned(4))) static uint8_t s_EP0_Buf[64 + 64 + 64]; /* ep0(64)+ep4_out(64)+ep4_in(64) */
__attribute__((aligned(4))) static uint8_t s_EP1_Buf[64 + 64];      /* ep1_out(64)+ep1_in(64) */
__attribute__((aligned(4))) static uint8_t s_EP2_Buf[64 + 64];      /* ep2_out(64)+ep2_in(64) */
__attribute__((aligned(4))) static uint8_t s_EP3_Buf[64 + 64];      /* ep3_out(64)+ep3_in(64), unused */

#define pUSBD_SetupReqPak    ((PUSB_SETUP_REQ)s_EP0_Buf)
#define pUSBD_EP0_DataBuf    (s_EP0_Buf)
#define pUSBD_EP1_IN_DataBuf (s_EP1_Buf + 64)
#define pUSBD_EP2_IN_DataBuf (s_EP2_Buf + 64)

volatile uint8_t USBD_EP1_TxBusy = 0;

static USBD_EP2_FillCallback s_ep2FillCb = NULL;

/******************************************************************************/
void USB_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      USBD_Device_Init
 * @brief   Initializes USB device endpoints and enables the USB IRQ.
 * @return  none
 */
void USBD_Device_Init(void)
{
    R8_USB_CTRL = 0x00; /* clear RB_UC_CLR_ALL / select device mode */

    /* EP1: OUT+IN, used for button-press events (IN only used, OUT left
     * ACK/idle). EP2: OUT+IN, used for the bulk synthetic file download
     * (only IN used). EP3/EP4 unused but still need valid DMA pointers.
     * RX_EN+TX_EN (not TX-only) is required: in the 1/1/0 buffer mode the
     * IN buffer sits at UEPn_DMA+64, which is where this driver writes
     * (pUSBD_EPn_IN_DataBuf); in TX-only mode the hardware would instead
     * transmit from UEPn_DMA+0, i.e. the unused OUT buffer (all zeros). */
    R8_UEP4_1_MOD = RB_UEP1_RX_EN | RB_UEP1_TX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN;

    R16_UEP0_DMA = (uint16_t)(uint32_t)s_EP0_Buf;
    R16_UEP1_DMA = (uint16_t)(uint32_t)s_EP1_Buf;
    R16_UEP2_DMA = (uint16_t)(uint32_t)s_EP2_Buf;
    R16_UEP3_DMA = (uint16_t)(uint32_t)s_EP3_Buf;

    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    /* EP1/EP2: hardware DATA0/1 auto-toggle - the hardware advances the PID
     * only on successful (ACKed) transfers. Software toggling in the ISR
     * desynced the PID on token-poll interrupts, causing the host to drop
     * and retry most bulk packets (measured ~10 IRQs per delivered packet,
     * throughput stuck at ~244 KB/s). */
    R8_UEP1_CTRL = UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP2_CTRL = UEP_T_RES_NAK | RB_UEP_AUTO_TOG;

    R8_USB_DEV_AD = 0x00;
    R8_USB_CTRL   = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB_DP_PU;
    R8_USB_INT_FG  = 0xFF;
    R8_UDEV_CTRL   = RB_UD_PD_DIS | RB_UD_PORT_EN;
    R8_USB_INT_EN  = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;

    USBD_EP1_TxBusy = 0;

    PFIC_EnableIRQ(USB_IRQn);
}

/*********************************************************************
 * @fn      USBD_EP1_SendData
 * @brief   Queues 'len' bytes for transmission on EP1 IN.
 * @return  0 on success, 1 if a previous transfer is still pending.
 */
uint8_t USBD_EP1_SendData(const uint8_t *pbuf, uint8_t len)
{
    if (USBD_EP1_TxBusy)
    {
        return 1;
    }
    if (len > DEF_USBD_UEP1_SIZE)
    {
        len = DEF_USBD_UEP1_SIZE;
    }

    memcpy(pUSBD_EP1_IN_DataBuf, pbuf, len);
    R8_UEP1_T_LEN = len;
    USBD_EP1_TxBusy = 1;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    return 0;
}

/*********************************************************************
 * @fn      USBD_EP2_SetFillCallback
 * @brief   Registers the callback used to generate EP2 bulk IN packets.
 * @return  none
 */
void USBD_EP2_SetFillCallback(USBD_EP2_FillCallback cb)
{
    s_ep2FillCb = cb;
}

/*********************************************************************
 * @fn      USBD_EP2_StartTransfer
 * @brief   (Re)starts an EP2 bulk IN transfer from the beginning.
 * @return  none
 */
void USBD_EP2_StartTransfer(void)
{
    uint16_t len;

    if (s_ep2FillCb == NULL)
    {
        return;
    }

    /* Keep fill+arm atomic w.r.t. the USB ISR (a completion interrupt in
     * between also fills+arms, which would double-consume a ring packet).
     * The NAK check inside the critical section makes this idempotent: if
     * the endpoint is already armed/streaming there is nothing to do, so
     * callers may invoke this redundantly as a self-heal path. */
    PFIC_DisableIRQ(USB_IRQn);
    if ((R8_UEP2_CTRL & MASK_UEP_T_RES) != UEP_T_RES_NAK)
    {
        PFIC_EnableIRQ(USB_IRQn);
        return;
    }
    len = s_ep2FillCb(pUSBD_EP2_IN_DataBuf, DEF_USBD_UEP2_SIZE);
    R8_UEP2_T_LEN = (uint8_t)len;
    if (len > 0)
    {
        R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    }
    else
    {
        R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
    }
    PFIC_EnableIRQ(USB_IRQn);
}

/*********************************************************************
 * @fn      USB_IRQHandler
 * @brief   USB interrupt handler: enumeration + vendor requests.
 * @return  none
 */
void USB_IRQHandler(void)
{
    uint8_t  intflag, chtype, errflag = 0;
    uint8_t  len;
    const uint8_t *pRespDescr;
    uint16_t respLen;
    uint8_t  vendorResult;

    intflag = R8_USB_INT_FG;

    if (intflag & RB_UIF_TRANSFER)
    {
        if ((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            switch (R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
            {
                case UIS_TOKEN_IN:
                    /* EP0 IN token: continue multi-packet control IN. */
                    if ((s_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
                    {
                        switch (s_SetupReqCode)
                        {
                            case USB_GET_DESCRIPTOR:
                                len = s_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : (uint8_t)s_SetupReqLen;
                                memcpy(pUSBD_EP0_DataBuf, pUSBD_Descr, len);
                                s_SetupReqLen -= len;
                                pUSBD_Descr += len;
                                R8_UEP0_T_LEN = len;
                                R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                                break;

                            case USB_SET_ADDRESS:
                                R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | USBD_DevAddr;
                                R8_UEP0_CTRL  = UEP_R_RES_ACK | UEP_T_RES_NAK;
                                break;

                            default:
                                R8_UEP0_T_LEN = 0;
                                R8_UEP0_CTRL  = UEP_R_RES_ACK | UEP_T_RES_NAK;
                                break;
                        }
                    }
                    else if ((s_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_VENDOR)
                    {
                        /* Continue multi-packet IN transfer for a vendor
                         * request (e.g. MS OS 2.0 descriptor set). */
                        len = s_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : (uint8_t)s_SetupReqLen;
                        if (len)
                        {
                            memcpy(pUSBD_EP0_DataBuf, pUSBD_Descr, len);
                            s_SetupReqLen -= len;
                            pUSBD_Descr += len;
                        }
                        R8_UEP0_T_LEN = len;
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                    }
                    else
                    {
                        R8_UEP0_T_LEN = 0;
                        R8_UEP0_CTRL  = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                    break;

                case UIS_TOKEN_OUT:
                    /* No control OUT data stage is used by this device
                     * (SET_LED carries its value in wValue). Nothing to do. */
                    break;

                case UIS_TOKEN_IN | 1:
                    /* Previous EP1 IN packet acknowledged by the host.
                     * PID toggle is handled by hardware (RB_UEP_AUTO_TOG). */
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    USBD_EP1_TxBusy = 0;
                    break;

                case UIS_TOKEN_IN | 2:
                    /* Previous EP2 bulk IN packet ACKed - immediately
                     * generate + arm the next packet so the pipe stays
                     * full for maximum throughput. A short packet (len <
                     * max size) signals end-of-transfer. PID toggle is
                     * handled by hardware (RB_UEP_AUTO_TOG). */
                    if (s_ep2FillCb != NULL)
                    {
                        uint16_t len2 = s_ep2FillCb(pUSBD_EP2_IN_DataBuf, DEF_USBD_UEP2_SIZE);
                        R8_UEP2_T_LEN = (uint8_t)len2;
                        if (len2 > 0)
                        {
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
                        }
                        else
                        {
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                        }
                    }
                    else
                    {
                        R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    }
                    break;

                default:
                    break;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if (R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;

            s_SetupReqType = pUSBD_SetupReqPak->bRequestType;
            s_SetupReqCode = pUSBD_SetupReqPak->bRequest;
            s_SetupReqLen  = pUSBD_SetupReqPak->wLength;
            chtype = pUSBD_SetupReqPak->bRequestType;

            len = 0;
            errflag = 0;

            if ((s_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_VENDOR)
            {
                /* Vendor requests: MS OS 2.0 / WebUSB descriptor sets +
                 * application requests (LED, button, file transfer). */
                pRespDescr = NULL;
                respLen = 0;
                vendorResult = WinUSB_ProcessVendorRequest(s_SetupReqCode, pUSBD_SetupReqPak->wValue,
                                                            pUSBD_SetupReqPak->wIndex, s_SetupReqLen,
                                                            &pRespDescr, &respLen);
                if (vendorResult == WINUSB_REQ_HANDLED_DATA)
                {
                    pUSBD_Descr = pRespDescr;
                    len = (uint8_t)respLen;

                    if (s_SetupReqLen > respLen)
                    {
                        s_SetupReqLen = respLen;
                    }
                    len = (s_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : (uint8_t)s_SetupReqLen;
                    memcpy(pUSBD_EP0_DataBuf, pUSBD_Descr, len);
                    pUSBD_Descr += len;
                }
                else if (vendorResult == WINUSB_REQ_HANDLED_ACK)
                {
                    len = 0;
                    s_SetupReqLen = 0;
                }
                else
                {
                    errflag = 0xFF;
                }
            }
            else if ((s_SetupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
            {
                /* No class requests are implemented on this device. */
                errflag = 0xFF;
            }
            else
            {
                /* USB standard request processing */
                switch (s_SetupReqCode)
                {
                    case USB_GET_DESCRIPTOR:
                        switch ((uint8_t)(pUSBD_SetupReqPak->wValue >> 8))
                        {
                            case USB_DESCR_TYP_DEVICE:
                                pUSBD_Descr = MyDevDescr;
                                len = DEF_USBD_DEVICE_DESC_LEN;
                                break;

                            case USB_DESCR_TYP_CONFIG:
                                pUSBD_Descr = MyCfgDescr;
                                len = (uint8_t)DEF_USBD_CONFIG_DESC_LEN;
                                break;

                            case USB_DESCR_TYP_BOS:
                                pUSBD_Descr = MyBOSDescr;
                                len = (uint8_t)DEF_USBD_BOS_DESC_LEN;
                                break;

                            case USB_DESCR_TYP_STRING:
                                switch ((uint8_t)(pUSBD_SetupReqPak->wValue & 0xFF))
                                {
                                    case 0:
                                        pUSBD_Descr = MyLangDescr;
                                        len = (uint8_t)DEF_USBD_LANG_DESC_LEN;
                                        break;
                                    case 1:
                                        pUSBD_Descr = MyManuInfo;
                                        len = (uint8_t)DEF_USBD_MANU_DESC_LEN;
                                        break;
                                    case 2:
                                        pUSBD_Descr = MyProdInfo;
                                        len = (uint8_t)DEF_USBD_PROD_DESC_LEN;
                                        break;
                                    case 3:
                                        pUSBD_Descr = MySerNumInfo;
                                        len = (uint8_t)DEF_USBD_SN_DESC_LEN;
                                        break;
                                    default:
                                        errflag = 0xFF;
                                        break;
                                }
                                break;

                            default:
                                errflag = 0xFF;
                                break;
                        }

                        if (errflag == 0)
                        {
                            if (s_SetupReqLen > len)
                            {
                                s_SetupReqLen = len;
                            }
                            len = (s_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : (uint8_t)s_SetupReqLen;
                            memcpy(pUSBD_EP0_DataBuf, pUSBD_Descr, len);
                            pUSBD_Descr += len;
                        }
                        break;

                    case USB_SET_ADDRESS:
                        USBD_DevAddr = (uint8_t)(pUSBD_SetupReqPak->wValue & 0xFF);
                        break;

                    case USB_GET_CONFIGURATION:
                        pUSBD_EP0_DataBuf[0] = USBD_DevConfig;
                        if (s_SetupReqLen > 1)
                        {
                            s_SetupReqLen = 1;
                        }
                        len = (uint8_t)s_SetupReqLen;
                        break;

                    case USB_SET_CONFIGURATION:
                        USBD_DevConfig = (uint8_t)(pUSBD_SetupReqPak->wValue & 0xFF);
                        USBD_DevEnumStatus = 0x01;
                        /* Per USB spec (and as xHCI enforces), SET_CONFIGURATION
                         * resets every endpoint's DATA0/1 toggle on the host
                         * side. Mirror that here: with RB_UEP_AUTO_TOG the
                         * toggle bit only advances on successful transfers,
                         * so a stale toggle after a re-SET_CONFIGURATION would
                         * permanently PID-mismatch and the host would silently
                         * discard all EP1/EP2 traffic (observed as downloads
                         * hanging right after a browser/libusb re-open). */
                        R8_UEP1_CTRL &= (uint8_t)~RB_UEP_T_TOG; /* back to DATA0 */
                        R8_UEP2_CTRL &= (uint8_t)~RB_UEP_T_TOG;
                        break;

                    case USB_CLEAR_FEATURE:
                    case USB_SET_FEATURE:
                        /* No remote-wakeup / endpoint-halt features
                         * exercised by this demo; simply acknowledge. */
                        break;

                    case USB_GET_INTERFACE:
                        pUSBD_EP0_DataBuf[0] = 0x00;
                        if (s_SetupReqLen > 1)
                        {
                            s_SetupReqLen = 1;
                        }
                        len = (uint8_t)s_SetupReqLen;
                        break;

                    case USB_SET_INTERFACE:
                        /* Same toggle-reset semantics as SET_CONFIGURATION. */
                        R8_UEP1_CTRL &= (uint8_t)~RB_UEP_T_TOG;
                        R8_UEP2_CTRL &= (uint8_t)~RB_UEP_T_TOG;
                        break;

                    case USB_GET_STATUS:
                        pUSBD_EP0_DataBuf[0] = 0x00;
                        pUSBD_EP0_DataBuf[1] = 0x00;
                        if (s_SetupReqLen > 2)
                        {
                            s_SetupReqLen = 2;
                        }
                        len = (uint8_t)s_SetupReqLen;
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            }

            if (errflag == 0xFF)
            {
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
            }
            else
            {
                if (chtype & DEF_UEP_IN)
                {
                    len = (s_SetupReqLen > DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : (uint8_t)s_SetupReqLen;
                    s_SetupReqLen -= len;
                    R8_UEP0_T_LEN = len;
                    R8_UEP0_CTRL  = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
                }
                else
                {
                    if (s_SetupReqLen == 0)
                    {
                        R8_UEP0_T_LEN = 0;
                        R8_UEP0_CTRL  = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
                    }
                    else
                    {
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                }
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if (intflag & RB_UIF_BUS_RST)
    {
        /* A real USB bus reset holds RB_UMS_BUS_RESET asserted for
         * milliseconds; short SE0 glitches (observed a few times per second
         * while the host idles between transfers) clear it again almost
         * immediately. Acting on a glitch would wipe the enumeration state
         * and the endpoints' DATA toggles mid-transfer, silently corrupting
         * the stream - so only run the full reset path if the reset status
         * is still asserted after ~20us. */
        uint8_t real_reset = 0;
        if (R8_USB_MIS_ST & RB_UMS_BUS_RESET)
        {
            volatile int32_t n = 1200; /* ~20us of re-checks at 60MHz */
            while (n-- > 0)
            {
                if (!(R8_USB_MIS_ST & RB_UMS_BUS_RESET))
                {
                    break;
                }
            }
            real_reset = (n < 0);
        }

        if (real_reset)
        {
            USBD_DevConfig = 0;
            USBD_DevAddr = 0;
            USBD_DevSleepStatus = 0;
            USBD_DevEnumStatus = 0;
            USBD_EP1_TxBusy = 0;
            R8_USB_DEV_AD = 0;
            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            R8_UEP1_CTRL = UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
            R8_UEP2_CTRL = UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        }
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if (intflag & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
        if (R8_USB_MIS_ST & RB_UMS_SUSPEND)
        {
            USBD_DevSleepStatus |= 0x02;
        }
        else
        {
            USBD_DevSleepStatus &= (uint8_t)~0x02u;
        }
    }
    else
    {
        R8_USB_INT_FG = intflag;
    }
}
