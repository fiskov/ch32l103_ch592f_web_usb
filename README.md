# ch32l103_ch592f_web_usb

Two independent, self-contained WinUSB / WebUSB LED+button demo firmwares,
one per MCU, speaking the **identical USB protocol** so a single web page
(hosted separately, see below) can drive either board:

- [`ch32l103/`](ch32l103) — WCH CH32L103 (RISC-V, QingKe V4C core).
  LED on **PB8** (hardware TIM4_CH3 PWM), button on **PA1**.
- [`ch592f/`](ch592f) — WCH CH592F (RISC-V). LED on **PB23** (TMR0
  hardware PWM, remapped), button on **PB22** (the chip's BOOT pin).
- [`ch585m/`](ch585m) — WCH CH585M (RISC-V): the same WinUSB/WebUSB
  LED + file-transfer demo with a zero-copy high-speed USBHS bulk path
  (~1 MB/s, gradient image, byte-verified).
- [`ch32v307/`](ch32v307) — WCH CH32V307 (RISC-V, QingKe V4F): the same
  WinUSB/WebUSB demo on the USBHS high-speed peripheral (~1 MB/s,
  byte-verified; LED pin still TODO).

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
