# CH592F WinUSB / WebUSB LED demo

Firmware for the WCH CH592F (RISC-V) demonstrating a USB full-speed device
that:

- Advertises a single BOS descriptor carrying **two Platform Capability
  descriptors side by side**:
  - **Microsoft OS 2.0** — so **Windows automatically binds `winusb.sys`**
    to the device with **no `.inf` file** needed (the classic "WCID"/WinUSB
    trick).
  - **[WebUSB](https://wicg.github.io/webusb/#webusb-platform-capability-descriptor)**
    — lets WebUSB-capable browsers fetch a landing-page URL via the
    `GET_URL` vendor request. The landing page URL is currently set to
    `https://fiskov.github.io/webusb-ch592f-led/index.html` - update
    `MyWebUSBURLDescr` in `app/usb_desc.c` if you move/rename that page.
    (Build/host the actual web page from
    [`fiskov.github.io`](https://github.com/fiskov/fiskov.github.io), not
    from this repository - see the sibling [`../ch32l103`](../ch32l103)
    firmware, which uses the identical protocol and can share the same
    web page.)
- Controls the brightness of an LED on **PA4** using a **software PWM**
  driven from the TMR1 interrupt (see `app/led_pwm.c`), exposed via two
  vendor control requests (`SET_LED` / `GET_LED`) sent over EP0.
- Debounces a push-button on **PB22** — the CH592F's **BOOT pin** (internal
  pull-up, shorts to GND when pressed) — in software and reports every
  press to the host as an event over a dedicated interrupt IN endpoint
  (EP1) - no polling required.
- Streams a synthetic ~3 MB BMP image over EP2 bulk IN for USB
  bulk-transfer throughput/correctness testing.

This is a CH592F port of the sibling [`../ch32l103`](../ch32l103) firmware:
same USB descriptors/protocol/vendor-request numbering, same overall
architecture (scheduler, systick, button debounce, file-transfer ring
buffer), only the low-level peripheral drivers differ.

## Hardware

| Item           | Value                                              |
| -------------- | --------------------------------------------------- |
| MCU            | WCH CH592F (RISC-V, `rv32imacxw`)                    |
| USB D+ pull-up | internal (`RB_PIN_USB_DP_PU`)                        |
| Debug UART     | UART1 — TX: PA9, RX: PA8                             |
| LED            | **PA4** (active-low), software PWM via TMR1          |
| Button         | **PB22** (the chip's BOOT pin), internal pull-up     |
| System clock   | 60 MHz (PLL)                                         |

LED and button pins were **deliberately kept the same as the original**
`ch592_usbhid` firmware this project replaces (PA4 LED) and chosen to reuse
an already-broken-out pin with no extra wiring (PB22 BOOT button), so no
rewiring is needed on existing boards.

None of CH592's fixed-function PWM4-PWM11 hardware channels are routed to
PA4 (they live on PA6/PA7/PA12/PA13/PB0/PB1/PB3/PB4 - see `CH592SFR.h`), so
unlike the CH32L103 sibling firmware (PB8 = TIM4_CH3 hardware PWM),
brightness here is produced by toggling PA4 in software from a ~128kHz
TMR1 interrupt (~500Hz 8-bit PWM carrier - flicker-free, low overhead).

## USB protocol summary

| Transfer              | Direction | Details                                                     |
|------------------------|-----------|---------------------------------------------------------------|
| Standard descriptors   | IN        | Device/Configuration/String/**BOS** descriptors               |
| MS OS 2.0 descriptor   | IN        | Vendor request, `bRequest=0x01`, `wIndex=0x0007` (fixed by spec) |
| WebUSB `GET_URL`       | IN        | Vendor request, `bRequest=0x04`, `wIndex=0x0002`, `wValue`=1 (fixed by spec) |
| `SET_LED`              | OUT (no data stage) | Vendor request, `bRequest=0x02`, `wValue`=brightness 0..255 |
| `GET_LED`              | IN        | Vendor request, `bRequest=0x03`, returns 1 byte = current brightness |
| `GET_VERSION`          | IN        | Vendor request, `bRequest=0x05`, returns 3 bytes = major,minor,patch |
| `SET_DEBOUNCE_MS`      | OUT (no data stage) | Vendor request, `bRequest=0x06`, `wValue`=debounce interval in ms (default 5) |
| `GET_BUTTON_RAW`       | IN        | Vendor request, `bRequest=0x07`, returns 1 byte = instantaneous non-debounced PB22 state (1=released, 0=pressed) - diagnostic |
| `GET_MILLIS`           | IN        | Vendor request, `bRequest=0x08`, returns 4 bytes little-endian `uint32` = current tick counter value - diagnostic |
| `GET_FILE_SIZE`        | IN        | Vendor request, `bRequest=0x09`, returns 4 bytes little-endian `uint32` = synthetic file size in bytes |
| `START_FILE_XFER`      | OUT (no data stage) | Vendor request, `bRequest=0x0A`, resets the EP2 bulk read offset to 0 and starts the download |
| Button-press event     | IN, **EP1 interrupt** | 4 bytes little-endian `uint32` = milliseconds since boot at the moment of the (debounced) press |
| Synthetic file data     | IN, **EP2 bulk** | ~3 MB uncompressed BMP image, generated on the fly |

VID/PID used in this demo: **0x1209 / 0x0001** (the open, non-commercial
["pid.codes" test allocation](https://pid.codes/1209/0001/)). Replace with
your own registered VID/PID before shipping a real product.

## Project Structure

```
ch592f/
├── app/
│   ├── usb_desc.[ch]          Device / Config / String / BOS / MS OS 2.0 / WebUSB descriptors
│   ├── usbd_winusb.[ch]       Vendor control-request dispatcher
│   ├── ch592_usbd_device.[ch] USB device stack (EP0 control + EP1 IN + EP2 bulk IN)
│   ├── led_pwm.[ch]           PA4 software-PWM LED brightness driver (TMR1)
│   ├── systick.[ch]           Free-running 1ms tick counter (TMR2-backed)
│   ├── shed.[ch]              Minimal cooperative scheduler (named one-shot/periodic tasks)
│   ├── button.[ch]            Debounced PB22 (BOOT) push-button, sends press events over EP1
│   ├── filexfer.[ch]          Synthetic ~3MB BMP generator (ring-buffered), streamed over EP2 bulk
│   ├── version.h              Firmware version constants
│   └── main.c                 Application entry point
├── vendor/
│   ├── Ld/Link.ld              Linker script
│   ├── RVMSIS/                 RISC-V CMSIS headers
│   ├── StdPeriphDriver/        CH592 peripheral driver library + libISP592
│   └── Startup/
│       └── startup_CH592.S     Reset/startup assembly
├── Makefile                    Top-level build configuration
├── rules.mk                    Generic GCC build rules
└── version                     Firmware version number
```

## Requirements

- **Toolchain**: `riscv32-wch-elf-gcc` (GCC 15) — or adjust `GCC_PREFIX` in
  [`Makefile`](Makefile) for GCC 12 (`riscv-none-elf-`) / GCC 11
  (`riscv-none-embed-`)
- **Flasher**: [`wchisp`](https://github.com/ch32-rs/wchisp) — USB ISP mode

## Build

```bash
# Clean build artifacts
make clean
# Default build (produces .elf, .bin, .hex in _build/)
make -j16
# Via USB ISP (wchisp)
make flash
```

## Version

The version is embedded into the firmware both via `bcdDevice` (see
[`app/version.h`](app/version.h)) and via the `GET_VERSION` vendor request,
and separately via the build-time `SW_VERSION_` define sourced from
[`version`](version) (legacy from the pre-refactor firmware, kept for
build-system compatibility).
