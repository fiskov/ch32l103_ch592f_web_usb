# CH32V307 WinUSB / WebUSB LED demo (USBHS high-speed)

CH32V307 port of [`../ch585m`](../ch585m): the same USB descriptors
(0x1209:0001), vendor protocol and fully zero-copy EP2 bulk path
(pointer packets into four rotating gradient rows) on the USBHS
high-speed peripheral.

- QingKe V4F (`rv32imafcxw`/`ilp32f`), startup_ch32v30x_D8C.S, 144 MHz
- systick: TIM2 1 kHz (same `SysTick_Millis()` API)
- debug UART via the EVT `USART_Printf_Init(115200)`
- LED: plain GPIO heartbeat; **pin is a TODO** in `led_pwm.c`
  (`LED_PORT`/`LED_PIN`, currently PA1) - board LED not identified yet
- button on PB1 (unpopulated placeholder)
- USBD_Device_Init() calls USBHS_RCC_Init() + USBHS_Device_Init(ENABLE)
  (the EVT main called the RCC/PLL setup separately - easy to miss)

Measured: ~1000 KB/s byte-exact gradient download (512-byte HS bulk,
zero-copy) - same class as the CH585M; the shared ceiling is the
per-packet interrupt round-trip.

Build & flash:

```sh
export PATH="/opt/wch/RISC-V_Embedded_GCC15/bin:$PATH"
make -j16
minichlink -w _build/app.bin 0x08000000
```
