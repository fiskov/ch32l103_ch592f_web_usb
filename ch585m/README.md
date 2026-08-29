# CH585M WinUSB / WebUSB LED demo (throughput-comparison port)

CH585M port of [`../ch592f`](../ch592f) (and the CH32L103 sibling): the
**same USB descriptors (VID/PID 0x1209:0x0001), the same vendor protocol
(SET_LED / GET_LED / GET_VERSION / GET_MILLIS / EP1 button events / EP2
synthetic 3 MB BMP bulk download) and the same ring-buffered bulk path**,
running on the CH585's USB2 full-speed device peripheral - so host-side
throughput numbers are directly comparable across the three boards.

Differences from the CH592F original:

- USB device peripheral: CH585 USB2 (register-compatible with the CH59x
  USBD; endpoint DMA registers are 32-bit here, and the USB pin enable
  lives in `R16_PIN_CONFIG` (`RB_PIN_USB_EN | RB_UDP_PU_EN`) instead of
  `R16_PIN_ANALOG_IE`).
- LED on **PB23** via the TMR0 hardware PWM channel (`RB_PIN_TMR0`
  alternate mapping, same as on the CH592F): real 8-bit brightness,
  ~58.6 kHz carrier. Heartbeat blink until the first host `SET_LED`.
- Button on **PB22** (input with pull-up, unpopulated on this board so
  far - the EP1 interrupt-in path stays idle).
- Debug UART: **UART0, TX on PB7, RX on PB4**, 115200.
- Scheduler (`shed.[ch]`) and millisecond time base (`systick.[ch]`,
  TMR2-backed) ported unchanged from the CH592F project.

## Measured throughput (same host, same harness)

| Configuration (CH585M unless noted)          | Sustained EP2 bulk-IN | Data check |
|-----------------------------------------------|-----------------------|------------|
| CH592F full-speed, memcpy ring (reference)    | ~230 KB/s             | byte-exact |
| USBHS high-speed, memcpy ring + per-byte BMP generator | ~256 KB/s    | byte-exact |
| USBHS + 64 KB pre-generated repeating block (generator bypass experiment, pre in-flight guard) | ~534 KB/s | block check FAILED (arm race) |
| USBHS + zero-copy DMA, 62.4 MHz system clock | ~327 KB/s | byte-exact |
| USBHS + zero-copy DMA, 78 MHz system clock | ~330 KB/s | byte-exact (gradient) |
| USBHS + zero-copy DMA + checkerboard generator (two precomputed rows) | ~935 KB/s | pattern-verified |

The 62.4 -> 78 MHz jump (+25% CPU) moved throughput by ~1%: the cap is
the per-packet interrupt round-trip, not clock or generator speed.

Notes: the ~256-327 KB/s figures are generator-limited, not bus-limited -
the 534 KB/s experiment (since reverted) proved the per-byte BMP walk was
the cap. Next step toward real USBHS numbers: combine zero-copy DMA with
the pre-generated block (serving straight from the static 64 KB buffer)
and chain packets inside the IN-complete handler with no NAK gap.

## Build & flash

```sh
export PATH="/opt/wch/RISC-V_Embedded_GCC15/bin:$PATH"
make -j16                                        # -> _build/app.bin
minichlink -w _build/app.bin 0x0 && minichlink -a && minichlink -b
```

The previous life of this directory (the EVT USBHS mass-storage demo it
was scaffolded from) is preserved in git history.
