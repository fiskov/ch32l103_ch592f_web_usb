# wch-usb-demos

WinUSB / WebUSB LED + file-transfer demo firmwares for several WCH RISC-V
MCUs, all speaking the **identical USB protocol** (0x1209:0x0001) so a
single web page can drive any board:

- [`ch32l103/`](ch32l103) — WCH CH32L103 (QingKe V4C). LED on PB8
  (hardware TIM4_CH3 PWM), button on PA1.
- [`ch592f/`](ch592f) — WCH CH592F. LED on PB23 (TMR0 hardware PWM,
  remapped), button on PB22 (BOOT pin). Zero-copy DMA, interrupt-driven
  UART TX, CPU profiling (vendor 0x0C).
- [`ch585m/`](ch585m) — WCH CH585M. LED on PA9 (TMR0 hardware PWM),
  UART on PA14. USBHS high-speed, zero-copy rows.
- [`ch32v307/`](ch32v307) — WCH CH32V307 (QingKe V4F). USBHS high-speed,
  zero-copy rows, DMA UART TX, CPU profiling (vendor 0x0C). LED pin TBD.

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

## Measured throughput (all with the same test harness)

All numbers measured on the same Linux/xHCI host, downloading the same
synthetic 3 MB gradient BMP over EP2 bulk IN, byte-verified:

| Board    | USB         | Architecture                  | Throughput | ISR/packet | CPU in ISR |
|----------|-------------|-------------------------------|-----------:|-----------:|-----------:|
| CH32L103 | full-speed  | ring buffer + memcpy           | ~230 KB/s | —          | —          |
| CH592F   | full-speed  | ring buffer + memcpy (stock)   | ~228 KB/s | 60.9 µs    | 22.3%      |
| CH592F   | full-speed  | **zero-copy DMA from ring**    | **287 KB/s** | **7.8 µs** | **3.6%** |
| CH585M   | high-speed  | zero-copy rotating rows        | ~1000 KB/s | ~1 µs      | ~2%        |
| CH32V307 | high-speed  | zero-copy rotating rows        | ~1000 KB/s | 1.2 µs     | 1.9%      |

Notes:

- Full-speed theoretical max: ~1.2 MB/s (19 × 64-byte transactions/frame).
  The CH592F at 287 KB/s reaches ~24% of this (limited by xHCI NAK-backoff
  with INT_BUSY; removing INT_BUSY doubles speed but corrupts data).
- High-speed theoretical max: ~53 MB/s. The ~1 MB/s ceiling on CH585M/
  CH32V307 is the per-packet interrupt round-trip (single-buffered endpoint,
  ~2000 packets/s × 512 bytes). Double-buffer mode would break this but
  requires a different arming protocol than the reference manual documents.
- CPU in ISR measured via TMR3/TIM3 at 1 µs resolution (vendor request 0x0C
  on CH592F and CH32V307).
- The zero-copy DMA on CH592F uses the same ring buffer as the stock code
  but the DMA reads directly from the ring slot (no memcpy). One slot of
  headroom prevents the pump from overwriting a slot the DMA is reading.

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
