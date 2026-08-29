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
- LED on **PB23** as a plain GPIO (no hardware PWM channel needed for
  this comparison port; brightness API is quantized: 0 = off, any other
  value = fully on). Heartbeat blink until the first host `SET_LED`.
- Button on **PB22** (input with pull-up, unpopulated on this board so
  far - the EP1 interrupt-in path stays idle).
- Debug UART: **UART0, TX on PB7, RX on PB4**, 115200.
- Scheduler (`shed.[ch]`) and millisecond time base (`systick.[ch]`,
  TMR2-backed) ported unchanged from the CH592F project.

## Measured throughput (same host, same harness)

| Board    | Sustained EP2 bulk-IN, byte-exact |
|----------|-----------------------------------|
| CH585M   | ~256 KB/s                         |
| CH592F   | ~230 KB/s                         |

## Build & flash

```sh
export PATH="/opt/wch/RISC-V_Embedded_GCC15/bin:$PATH"
make -j16                                        # -> _build/app.bin
minichlink -w _build/app.bin 0x0 && minichlink -a && minichlink -b
```

The previous life of this directory (the EVT USBHS mass-storage demo it
was scaffolded from) is preserved in git history.
