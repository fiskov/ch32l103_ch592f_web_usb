# ch32l103_ch592f_web_usb

Two independent, self-contained WinUSB / WebUSB LED+button demo firmwares,
one per MCU, speaking the **identical USB protocol** so a single web page
(hosted separately, see below) can drive either board:

- [`ch32l103/`](ch32l103) — WCH CH32L103 (RISC-V, QingKe V4C core).
  LED on **PB8** (hardware TIM4_CH3 PWM), button on **PA1**.
- [`ch592f/`](ch592f) — WCH CH592F (RISC-V). LED on **PB23** (TMR0
  hardware PWM, remapped), button on **PB22** (the chip's BOOT pin).
- [`ch585m/`](ch585m) — WCH CH585M (RISC-V) **USBHS mass-storage (U-Disk)
  demo**, ported from the CH585 EVT; internal or SPI-flash backing store.

Both devices:

- Advertise a BOS descriptor with **Microsoft OS 2.0** (Windows
  auto-binds `winusb.sys`, no `.inf` file) and **WebUSB** (browser
  `GET_URL` landing-page) Platform Capabilities side by side.
- Expose LED brightness control (`SET_LED`/`GET_LED`), a debounced
  push-button reported over an EP1 interrupt IN endpoint, and a synthetic
  ~3 MB BMP file streamed over EP2 bulk IN for throughput testing.
- Use VID/PID **0x1209 / 0x0001** (the open ["pid.codes" test
  allocation](https://pid.codes/1209/0001/)).

See each project's own README for hardware wiring, the full vendor-request
table, and build/flash instructions:

- [`ch32l103/README.md`](ch32l103/README.md)
- [`ch592f/README.md`](ch592f/README.md)

## Measured throughput (demo conditions)

Sustained EP2 bulk-IN download of the synthetic 3 MB BMP file, verified
byte-exact, USB full-speed device on a Linux/xHCI host:

| Board    | Conditions                          | Sustained speed |
|----------|-------------------------------------|-----------------|
| CH592F   | libusb, 64 B - 64 KB read sizes     | ~230 KB/s       |
| CH592F   | browser (WebUSB, 16 KB transferIn)  | ~240 KB/s       |
| CH32L103 | to be measured (board not connected) | -             |

Notes:

- Both boards are full-speed USB devices, so the bus ceiling is the same;
  the numbers reflect each chip's USB device peripheral and this demo's
  driver path, not an MCU maximum.
- For the CH592F, ~230 KB/s is what the single-buffered bulk endpoint plus
  the per-packet interrupt round-trip sustains in this demo configuration.
- Right after the host closes and re-opens the device, the first download
  can run slower for one transfer (host-controller NAK backoff); later
  downloads run at full speed again.

## Web page

The browser-side control page is **not** part of this repository. It is
built and hosted from the separate
[`fiskov.github.io`](https://github.com/fiskov/fiskov.github.io) repository
and works with either board, since both speak the same protocol:

- `https://fiskov.github.io/webusb-led/index.html` (one page for both
  boards, since the protocol is identical)

## Building

Each firmware is a fully independent Make project; build them separately
from their own directory:

```sh
export PATH="/path/to/RISC-V Embedded GCC15/bin:$PATH"

cd ch32l103 && make -j16   # -> ch32l103/_build/winusb_webusb_led.{elf,hex,bin}
cd ../ch592f && make -j16  # -> ch592f/_build/app.{elf,hex,bin}
cd ../ch585m && make -j16  # -> ch585m/_build/app.{elf,hex,bin}
```

See [`.vscode/tasks.json`](.vscode/tasks.json) for ready-made VS Code build
tasks covering both projects.
