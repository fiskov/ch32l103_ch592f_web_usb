/********************************** (C) COPYRIGHT *******************************
 * File Name          : version.h
 * Description        : Firmware version, exposed to the host both via
 *                       bcdDevice (device descriptor) and via the
 *                       GET_VERSION vendor control request.
 *******************************************************************************/
#ifndef APP_VERSION_H_
#define APP_VERSION_H_

#define FW_VERSION_MAJOR   0
#define FW_VERSION_MINOR   5
#define FW_VERSION_PATCH   2

/* bcdDevice field of the USB device descriptor: BCD-encoded MAJOR.MINOR
 * (e.g. major=0, minor=2 -> 0x0002 -> "00.02"). The patch level is not
 * representable in bcdDevice and is only exposed via GET_VERSION.        */
#define FW_BCD_DEVICE_HI   FW_VERSION_MAJOR
#define FW_BCD_DEVICE_LO   FW_VERSION_MINOR

#endif /* APP_VERSION_H_ */
