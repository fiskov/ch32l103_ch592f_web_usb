# CH32L103 WinUSB / WebUSB LED demo

Firmware for the WCH CH32L103 (RISC-V, QingKe V4C core) demonstrating a USB
full-speed device that:

- Advertises a single BOS descriptor carrying **two Platform Capability
  descriptors side by side**:
  - **Microsoft OS 2.0** — so **Windows automatically binds `winusb.sys`**
    to the device with **no `.inf` file** needed (the classic "WCID"/WinUSB
    trick).
  - **[WebUSB](https://wicg.github.io/webusb/#webusb-platform-capability-descriptor)**
    — lets WebUSB-capable browsers fetch a landing-page URL via the
    `GET_URL` vendor request. (A device does **not** need this capability
    for `navigator.usb.requestDevice()` to find it — Chrome can already open
    any device by VID/PID — but it's what enables the "open landing page"
    prompt some browsers show when the device is plugged in.) The landing
    page URL is currently set to
    [`https://fiskov.github.io/webusb-led/index.html`](https://fiskov.github.io/webusb-led/index.html) -
    update `MyWebUSBURLDescr` in `User/usb_desc.c` if you move/rename that
    page. The page itself is built/hosted from the separate
    [`fiskov.github.io`](https://github.com/fiskov/fiskov.github.io)
    repository, not from here.

  Both capabilities coexist without conflict: Windows only looks at the
  MS OS 2.0 one, WebUSB-aware browsers only look at the WebUSB one, and any
  OS/browser that doesn't recognize a given capability UUID simply skips it.
- Because no signed kernel driver is required on any OS, the very same
  device can be opened directly from a browser via the **WebUSB API**.
- Controls the brightness of an LED on **PB8** using **TIM4 Channel 3 PWM**,
  driven purely by two vendor control requests (SET_LED / GET_LED) sent over
  EP0.
- Debounces a push-button on **PA1** (internal pull-up, shorts to GND when
  pressed) in software and reports every press to the host as an event over
  a dedicated interrupt IN endpoint (EP1) - no polling required.

This firmware shares its USB descriptors, vendor-request protocol, and
overall architecture (scheduler, systick, button debounce, file-transfer
ring buffer) with the sibling [`../ch592f`](../ch592f) port for the WCH
CH592F - only the low-level peripheral drivers differ between the two.

## Hardware

- Board: any CH32L103 board (e.g. WCH CH32L103-EVT), USB connector wired to
  the internal USB-FS PHY pins (PA11/PA12 typically, see your board's
  schematic).
- LED: connect an LED (with a current-limiting resistor, e.g. 330 Ω) between
  **PB8** and GND (LED cathode to GND) — PB8 is the default (non-remapped)
  location of **TIM4_CH3**. The LED is driven **active-low** (see
  `led_pwm.c`); adjust `TIM_OCPolarity` there if your wiring is active-high.
- Button: connect a momentary push-button between **PA1** and GND. PA1 uses
  the MCU's internal pull-up, so no external resistor is needed.

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
| `GET_BUTTON_RAW`       | IN        | Vendor request, `bRequest=0x07`, returns 1 byte = instantaneous non-debounced PA1 state (1=released, 0=pressed) - diagnostic |
| `GET_MILLIS`           | IN        | Vendor request, `bRequest=0x08`, returns 4 bytes little-endian `uint32` = current tick counter value - diagnostic |
| `GET_FILE_SIZE`        | IN        | Vendor request, `bRequest=0x09`, returns 4 bytes little-endian `uint32` = synthetic file size in bytes |
| `START_FILE_XFER`      | OUT (no data stage) | Vendor request, `bRequest=0x0A`, resets the EP2 bulk read offset to 0 and starts the download |
| Button-press event     | IN, **EP1 interrupt** | 4 bytes little-endian `uint32` = milliseconds since boot at the moment of the (debounced) press |
| Synthetic file data     | IN, **EP2 bulk** | ~3 MB uncompressed BMP image, generated on the fly (see below) |

### Throughput / correctness test (EP2 bulk)

The device also exposes a synthetic ~3 MB, 1024x1024 24-bit BMP image
(`filexfer.[ch]`) purely for testing USB bulk-transfer throughput and
end-to-end data correctness - nothing is stored on the device, every byte
is computed on the fly from its absolute offset by a small deterministic
formula. To download it: send `GET_FILE_SIZE`, then `START_FILE_XFER`, then
read repeatedly from EP2 (bulk IN, 64-byte max packet) until the reported
size has been received.

Packet generation runs ahead of time in a background ring buffer
(`FileXfer_Pump()`, called from the idle main loop) so the USB interrupt
handler only has to `memcpy()` a pre-built packet per transaction -
measured throughput on real hardware is **~730 KB/s (5.7 Mbit/s)** over
libusb, about 1.66x the theoretical per-packet minimum for 64-byte
full-speed bulk transfers.

The button-press event is the only transfer that does **not** go over EP0:
the device exposes one interrupt IN endpoint (EP1, 8-byte max packet, 10ms
polling interval) purely so the firmware can proactively notify the host
the instant the button is pressed, without the host having to poll a
control request. A cooperative scheduler (`shed.[ch]`) drives the
debounce-poll task (and any other periodic firmware tasks) from a 1ms
SysTick tick (`systick.[ch]`).

PA1 is sampled every `SET_DEBOUNCE_MS` milliseconds (5ms by default); a
press is only reported once the pin has read LOW for two consecutive
samples in a row after being stably released, which reliably filters out
mechanical contact bounce without adding perceptible latency.

VID/PID used in this demo: **0x1209 / 0x0001** (the open, non-commercial
["pid.codes" test allocation](https://pid.codes/1209/0001/)). Replace with
your own registered VID/PID before shipping a real product.

## Building

Requires a RISC-V GCC toolchain targeting `rv32imac`/`ilp32` (the one bundled
with WCH's **MounRiver Studio** works out of the box; a plain
`riscv-none-embed-gcc` / `riscv64-unknown-elf-gcc` also works).

```sh
# Point Make at your toolchain if it's not already in PATH. Example for a
# Linux MounRiver Studio 2 install (adjust the version folder as needed):
export PATH="/usr/share/MRS2/MRS-linux-x64/resources/app/resources/linux/components/WCH/Toolchain/RISC-V Embedded GCC15/bin:$PATH"

cd ch32l103
make                       # builds _build/winusb_webusb_led.{elf,hex,bin}
```

The MounRiver toolchain's binaries are prefixed `riscv32-wch-elf-`, which is
already the default in the `Makefile`. If your toolchain uses a different
prefix (e.g. `riscv-none-embed-` for the "GNU MCU Eclipse" toolchain),
override it:

```sh
make TOOLCHAIN_PREFIX=riscv-none-embed-
```

## Flashing

Any of the following works:

- **MounRiver Studio / WCH-LinkUtility**: flash `_build/winusb_webusb_led.hex`
  at address `0x08000000`.
- **[wlink](https://github.com/ch32-rs/wlink)** (WCH-LinkE, open source CLI):
  ```sh
  make flash
  # or directly:
  wlink flash _build/winusb_webusb_led.bin
  ```
- **OpenOCD** with a WCH-Link/compatible adapter and the appropriate RISC-V
  OpenOCD build.

## Verifying WinUSB binding on Windows

1. Flash the firmware and plug the board in.
2. Open **Device Manager** — the device should appear automatically under
   "Universal Serial Bus devices" bound to **WinUSB**, with no driver
   installation prompt and no `.inf` file involved.
3. Tools such as [Zadig](https://zadig.akeo.ie/) are **not** needed; the
   MS OS 2.0 descriptors handle the binding automatically.

## Verifying on Linux / macOS

No special driver is required on Linux or macOS; the vendor-specific
interface (class `0xFF`) is accessible directly through `libusb` (and thus
also through Chrome's WebUSB implementation) as soon as the appropriate
udev rule / permissions allow user-space access to the device node.

## Source layout

```
ch32l103/
├── Core/            WCH RISC-V core support (unmodified from the CH32L103 EVT SDK)
├── Debug/           printf-over-UART + delay helpers (unmodified)
├── Ld/              Linker script (unmodified)
├── Peripheral/      CH32L103 Standard Peripheral Library (unmodified)
├── Startup/         Startup assembly / vector table (unmodified)
├── User/
│   ├── usb_desc.[ch]          Device / Config / String / BOS / MS OS 2.0 / WebUSB descriptors
│   ├── usbd_winusb.[ch]       Vendor control-request dispatcher
│   ├── ch32l103_usbfs_device.[ch]  USBFS device stack (EP0 control + EP1 IN)
│   ├── led_pwm.[ch]           TIM4_CH3 / PB8 PWM LED brightness driver
│   ├── systick.[ch]           Free-running 1ms tick counter (TIM2-backed, not the
│   │                          SysTick peripheral - see comment in systick.c)
│   ├── shed.[ch]              Minimal cooperative scheduler (named one-shot/periodic tasks)
│   ├── button.[ch]            Debounced PA1 push-button, sends press events over EP1
│   ├── filexfer.[ch]          Synthetic ~3MB BMP generator (ring-buffered), streamed over EP2 bulk
│   ├── version.h              Firmware version constants
│   ├── main.c                 Application entry point
│   ├── ch32l103_it.[ch]       NMI / HardFault handlers (unmodified)
│   └── system_ch32l103.[ch]   Clock setup (unmodified)
└── Makefile
```

## Web page

The browser-side WebUSB/WinUSB control page for this firmware is built and
hosted from the separate
[`fiskov.github.io`](https://github.com/fiskov/fiskov.github.io) repository
(not from this repo) at
`https://fiskov.github.io/webusb-led/index.html`. The sibling
[`../ch592f`](../ch592f) firmware speaks the identical protocol, so the same
page works for both boards.
