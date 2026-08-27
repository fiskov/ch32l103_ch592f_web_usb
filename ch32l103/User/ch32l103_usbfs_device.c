/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32l103_usbfs_device.c
 * Description        : USBFS device stack: enumeration (EP0 control
 *                       transfers), MS OS 2.0/WebUSB descriptor support,
 *                       the application vendor requests (LED get/set,
 *                       etc.), EP1 interrupt IN (button events) and EP2
 *                       bulk IN (synthetic file download for throughput
 *                       testing, callback-driven and refilled directly
 *                       from the interrupt handler for max throughput).
 *******************************************************************************/

#include "ch32l103_usbfs_device.h"
#include "usbd_winusb.h"

/*******************************************************************************/
/* Variable Definition */

const uint8_t    *pUSBFS_Descr;

volatile uint8_t  USBFS_SetupReqCode;
volatile uint8_t  USBFS_SetupReqType;
volatile uint16_t USBFS_SetupReqValue;
volatile uint16_t USBFS_SetupReqIndex;
volatile uint16_t USBFS_SetupReqLen;

volatile uint8_t  USBFS_DevConfig;
volatile uint8_t  USBFS_DevAddr;
volatile uint8_t  USBFS_DevSleepStatus;
volatile uint8_t  USBFS_DevEnumStatus;

__attribute__ ((aligned(4))) uint8_t USBFS_EP0_Buf[DEF_USBD_UEP0_SIZE];
__attribute__ ((aligned(4))) uint8_t USBFS_EP1_Buf[DEF_USBD_UEP1_SIZE];
__attribute__ ((aligned(4))) uint8_t USBFS_EP2_Buf[DEF_USBD_UEP2_SIZE];

volatile uint8_t USBFS_EP1_TxBusy = 0;

static USBFS_EP2_FillCallback s_ep2FillCb = NULL;

/******************************************************************************/
void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      USBFS_RCC_Init
 * @brief   Set USB clock.
 * @return  none
 */
void USBFS_RCC_Init( void )
{
    if( SystemCoreClock == 96000000 )
    {
        RCC_USBCLKConfig( RCC_USBCLKSource_PLLCLK_Div2 );
    }
    else if( SystemCoreClock == 72000000 )
    {
        RCC_USBCLKConfig( RCC_USBCLKSource_PLLCLK_Div1_5 );
    }
    else if( SystemCoreClock == 48000000 )
    {
        RCC_USBCLKConfig( RCC_USBCLKSource_PLLCLK_Div1 );
    }
    RCC_HBPeriphClockCmd( RCC_HBPeriph_USBFS, ENABLE );
}

/*********************************************************************
 * @fn      USBFS_Device_Endp_Init
 * @brief   Initializes USB device endpoints (EP0 control only).
 * @return  none
 */
void USBFS_Device_Endp_Init( void )
{
    /* EP1: IN-only (TX), used for button-press events */
    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    /* EP2: IN-only (TX), used for the bulk synthetic file download */
    USBFSD->UEP2_3_MOD = USBFS_UEP2_TX_EN;

    USBFSD->UEP0_DMA = (uint32_t)USBFS_EP0_Buf;
    USBFSD->UEP1_DMA = (uint32_t)USBFS_EP1_Buf;
    USBFSD->UEP2_DMA = (uint32_t)USBFS_EP2_Buf;

    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP2_TX_CTRL = USBFS_UEP_T_RES_NAK;

    USBFS_EP1_TxBusy = 0;
}

/*********************************************************************
 * @fn      USBFS_EP1_SendData
 * @brief   Queues 'len' bytes for transmission on EP1 IN.
 * @return  0 on success, 1 if a previous transfer is still pending.
 */
uint8_t USBFS_EP1_SendData(const uint8_t *pbuf, uint8_t len)
{
    if (USBFS_EP1_TxBusy)
    {
        return 1;
    }
    if (len > DEF_USBD_UEP1_SIZE)
    {
        len = DEF_USBD_UEP1_SIZE;
    }

    memcpy(USBFS_EP1_Buf, pbuf, len);
    USBFSD->UEP1_TX_LEN  = len;
    USBFS_EP1_TxBusy = 1;
    USBFSD->UEP1_TX_CTRL = (USBFSD->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
    return 0;
}

/*********************************************************************
 * @fn      USBFS_EP2_SetFillCallback
 * @brief   Registers the callback used to generate EP2 bulk IN packets.
 * @return  none
 */
void USBFS_EP2_SetFillCallback(USBFS_EP2_FillCallback cb)
{
    s_ep2FillCb = cb;
}

/*********************************************************************
 * @fn      USBFS_EP2_StartTransfer
 * @brief   (Re)starts an EP2 bulk IN transfer from the beginning.
 * @return  none
 */
void USBFS_EP2_StartTransfer(void)
{
    uint16_t len;

    if (s_ep2FillCb == NULL)
    {
        return;
    }

    len = s_ep2FillCb(USBFS_EP2_Buf, DEF_USBD_UEP2_SIZE);
    USBFSD->UEP2_TX_LEN = len;
    if (len > 0)
    {
        USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
    }
    else
    {
        USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
    }
}

/*********************************************************************
 * @fn      USBFS_Device_Init
 * @brief   Initializes USB device.
 * @return  none
 */
void USBFS_Device_Init( FunctionalState sta )
{
    if( sta )
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFS_Device_Endp_Init( );
        USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
        USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
        USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
        NVIC_EnableIRQ( USBFS_IRQn );
    }
    else
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFSD->BASE_CTRL = 0x00;
        NVIC_DisableIRQ( USBFS_IRQn );
    }
}

/*********************************************************************
 * @fn      USBFS_IRQHandler
 * @brief   USBFS interrupt handler: enumeration + vendor requests.
 * @return  none
 */
void USBFS_IRQHandler( void )
{
    uint8_t  intflag, intst, errflag;
    uint16_t len;
    const uint8_t *pRespDescr;
    uint16_t respLen;
    uint8_t  vendorResult;

    intflag = USBFSD->INT_FG;
    intst   = USBFSD->INT_ST;

    if( intflag & USBFS_UIF_TRANSFER )
    {
        switch( intst & USBFS_UIS_TOKEN_MASK )
        {
            /* data-in stage processing */
            case USBFS_UIS_TOKEN_IN:
                switch( intst & ( USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK ) )
                {
                    case USBFS_UIS_TOKEN_IN | DEF_UEP0:
                        if( USBFS_SetupReqLen == 0 )
                        {
                            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                        }

                        if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_STANDARD )
                        {
                            switch( USBFS_SetupReqCode )
                            {
                                case USB_GET_DESCRIPTOR:
                                    len = USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                                    memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                                    USBFS_SetupReqLen -= len;
                                    pUSBFS_Descr += len;
                                    USBFSD->UEP0_TX_LEN = len;
                                    USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                                    break;

                                case USB_SET_ADDRESS:
                                    USBFSD->DEV_ADDR = (USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) | USBFS_DevAddr;
                                    break;

                                default:
                                    break;
                            }
                        }
                        else if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_VENDOR )
                        {
                            /* Continue multi-packet IN transfer for a vendor request
                             * (e.g. the MS OS 2.0 descriptor set, 30 bytes > 1 packet
                             * only if EP0 size < 30; kept generic just in case). */
                            len = USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                            if ( len )
                            {
                                memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                                USBFS_SetupReqLen -= len;
                                pUSBFS_Descr += len;
                            }
                            USBFSD->UEP0_TX_LEN = len;
                            USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                        }
                        break;

                    case USBFS_UIS_TOKEN_IN | DEF_UEP1:
                        /* Previous EP1 IN packet acknowledged by the host. */
                        USBFSD->UEP1_TX_CTRL = (USBFSD->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFSD->UEP1_TX_CTRL ^= USBFS_UEP_T_TOG;
                        USBFS_EP1_TxBusy = 0;
                        break;

                    case USBFS_UIS_TOKEN_IN | DEF_UEP2:
                        /* Previous EP2 bulk IN packet ACKed - toggle DATA0/1
                         * and immediately generate + arm the next packet so
                         * the pipe stays full for maximum throughput. A
                         * short packet (len < max size) signals the end of
                         * the bulk transfer to the host, matching the
                         * fill callback's own end-of-data (len==0) case. */
                        USBFSD->UEP2_TX_CTRL ^= USBFS_UEP_T_TOG;
                        if (s_ep2FillCb != NULL)
                        {
                            len = s_ep2FillCb(USBFS_EP2_Buf, DEF_USBD_UEP2_SIZE);
                            USBFSD->UEP2_TX_LEN = len;
                            if (len > 0)
                            {
                                USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                            }
                            else
                            {
                                USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                            }
                        }
                        else
                        {
                            USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        }
                        break;

                    default :
                        break;
                }
                break;

            /* data-out stage processing */
            case USBFS_UIS_TOKEN_OUT:
                switch( intst & ( USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK ) )
                {
                    case USBFS_UIS_TOKEN_OUT | DEF_UEP0:
                        /* No control OUT data stage is used by this device
                         * (SET_LED carries its value in wValue). Nothing to do. */
                        break;

                    default:
                        break;
                }
                break;

            /* Setup stage processing */
            case USBFS_UIS_TOKEN_SETUP:
                USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
                USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_NAK;

                USBFS_SetupReqType  = pUSBFS_SetupReqPak->bRequestType;
                USBFS_SetupReqCode  = pUSBFS_SetupReqPak->bRequest;
                USBFS_SetupReqLen   = pUSBFS_SetupReqPak->wLength;
                USBFS_SetupReqValue = pUSBFS_SetupReqPak->wValue;
                USBFS_SetupReqIndex = pUSBFS_SetupReqPak->wIndex;
                len = 0;
                errflag = 0;

                if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_VENDOR )
                {
                    /* Vendor requests: MS OS 2.0 descriptor set + LED control */
                    pRespDescr = NULL;
                    respLen = 0;
                    vendorResult = WinUSB_ProcessVendorRequest( USBFS_SetupReqCode, USBFS_SetupReqValue,
                                                                 USBFS_SetupReqIndex, USBFS_SetupReqLen,
                                                                 &pRespDescr, &respLen );
                    if ( vendorResult == WINUSB_REQ_HANDLED_DATA )
                    {
                        pUSBFS_Descr = pRespDescr;
                        len = respLen;

                        /* Copy the first packet into the EP0 buffer now, same
                         * as the standard GET_DESCRIPTOR path below - without
                         * this the host would read stale/garbage EP0 data. */
                        if ( USBFS_SetupReqLen > len )
                        {
                            USBFS_SetupReqLen = len;
                        }
                        len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                        memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                        pUSBFS_Descr += len;
                    }
                    else if ( vendorResult == WINUSB_REQ_HANDLED_ACK )
                    {
                        len = 0;
                        USBFS_SetupReqLen = 0;
                    }
                    else
                    {
                        errflag = 0xFF;
                    }
                }
                else if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                {
                    /* No class requests are implemented on this device */
                    errflag = 0xFF;
                }
                else
                {
                    /* usb standard request processing */
                    switch( USBFS_SetupReqCode )
                    {
                        case USB_GET_DESCRIPTOR:
                            switch( (uint8_t)(USBFS_SetupReqValue>>8) )
                            {
                                case USB_DESCR_TYP_DEVICE:
                                    pUSBFS_Descr = MyDevDescr;
                                    len = DEF_USBD_DEVICE_DESC_LEN;
                                    break;

                                case USB_DESCR_TYP_CONFIG:
                                    pUSBFS_Descr = MyCfgDescr;
                                    len = DEF_USBD_CONFIG_DESC_LEN;
                                    break;

                                case USB_DESCR_TYP_BOS:
                                    pUSBFS_Descr = MyBOSDescr;
                                    len = DEF_USBD_BOS_DESC_LEN;
                                    break;

                                case USB_DESCR_TYP_STRING:
                                    switch( (uint8_t)(USBFS_SetupReqValue&0xFF) )
                                    {
                                        case DEF_STRING_DESC_LANG:
                                            pUSBFS_Descr = MyLangDescr;
                                            len = DEF_USBD_LANG_DESC_LEN;
                                            break;

                                        case DEF_STRING_DESC_MANU:
                                            pUSBFS_Descr = MyManuInfo;
                                            len = DEF_USBD_MANU_DESC_LEN;
                                            break;

                                        case DEF_STRING_DESC_PROD:
                                            pUSBFS_Descr = MyProdInfo;
                                            len = DEF_USBD_PROD_DESC_LEN;
                                            break;

                                        case 0x03: /* serial number index */
                                            pUSBFS_Descr = MySerNumInfo;
                                            len = DEF_USBD_SN_DESC_LEN;
                                            break;

                                        default:
                                            errflag = 0xFF;
                                            break;
                                    }
                                    break;

                                default :
                                    errflag = 0xFF;
                                    break;
                            }

                            if( USBFS_SetupReqLen > len )
                            {
                                USBFS_SetupReqLen = len;
                            }
                            len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                            memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                            pUSBFS_Descr += len;
                            break;

                        case USB_SET_ADDRESS:
                            USBFS_DevAddr = (uint8_t)( USBFS_SetupReqValue & 0xFF );
                            break;

                        case USB_GET_CONFIGURATION:
                            USBFS_EP0_Buf[ 0 ] = USBFS_DevConfig;
                            if( USBFS_SetupReqLen > 1 )
                            {
                                USBFS_SetupReqLen = 1;
                            }
                            len = USBFS_SetupReqLen;
                            break;

                        case USB_SET_CONFIGURATION:
                            USBFS_DevConfig = (uint8_t)( USBFS_SetupReqValue & 0xFF );
                            USBFS_DevEnumStatus = 0x01;
                            break;

                        case USB_CLEAR_FEATURE:
                        case USB_SET_FEATURE:
                            /* No remote-wakeup / endpoint-halt features on this
                             * endpoint-less interface; simply acknowledge. */
                            break;

                        case USB_GET_INTERFACE:
                            USBFS_EP0_Buf[0] = 0x00;
                            if ( USBFS_SetupReqLen > 1 )
                            {
                                USBFS_SetupReqLen = 1;
                            }
                            len = USBFS_SetupReqLen;
                            break;

                        case USB_SET_INTERFACE:
                            break;

                        case USB_GET_STATUS:
                            USBFS_EP0_Buf[ 0 ] = 0x00;
                            USBFS_EP0_Buf[ 1 ] = 0x00;
                            if ( USBFS_SetupReqLen > 2 )
                            {
                                USBFS_SetupReqLen = 2;
                            }
                            len = USBFS_SetupReqLen;
                            break;

                        default:
                            errflag = 0xFF;
                            break;
                    }
                }

                if( errflag == 0xFF)
                {
                    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_STALL;
                    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_STALL;
                }
                else
                {
                    if( USBFS_SetupReqType & DEF_UEP_IN )
                    {
                        len = ( USBFS_SetupReqLen > DEF_USBD_UEP0_SIZE )? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                        USBFS_SetupReqLen -= len;
                        USBFSD->UEP0_TX_LEN  = len;
                        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_ACK;
                    }
                    else
                    {
                        if( USBFS_SetupReqLen == 0 )
                        {
                            USBFSD->UEP0_TX_LEN  = 0;
                            USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_ACK;
                        }
                        else
                        {
                            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_ACK;
                        }
                    }
                }
                break;

            default :
                break;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }
    else if( intflag & USBFS_UIF_BUS_RST )
    {
        USBFS_DevConfig = 0;
        USBFS_DevAddr = 0;
        USBFS_DevSleepStatus = 0;
        USBFS_DevEnumStatus = 0;
        USBFSD->DEV_ADDR = 0;
        USBFS_Device_Endp_Init( );
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    }
    else if( intflag & USBFS_UIF_SUSPEND )
    {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
        if( USBFSD->MIS_ST & USBFS_UMS_SUSPEND )
        {
            USBFS_DevSleepStatus |= 0x02;
        }
        else
        {
            USBFS_DevSleepStatus &= ~0x02;
        }
    }
    else
    {
        USBFSD->INT_FG = intflag;
    }
}

/*********************************************************************
 * @fn      USBFS_Send_Resume
 * @brief   USBFS device sends wake-up signal to host
 * @return  none
 */
void USBFS_Send_Resume( void )
{
    USBFSD->UDEV_CTRL ^= USBFS_UD_LOW_SPEED;
    Delay_Ms( 8 );
    USBFSD->UDEV_CTRL ^= USBFS_UD_LOW_SPEED;
    Delay_Ms( 1 );
}
