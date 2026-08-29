# CH32V307 USBHS MSC (U-Disk) demo - scaffold

Standalone make project ported from the CH32V307 EVT example
`EXAM/USB/USBHS/DEVICE/MSC_U-Disk` (same layout as the sibling
`../ch585m` project: app/ + vendor/{Ld,Core,Debug,Peripheral,Startup}).

- QingKe V4F core: `-march=rv32imafcxw -mabi=ilp32f`
- sources converted to UTF-8; EVT vendor warnings not fatal
- startup: `startup_ch32v30x_D8C.S` (V307 = D8C variant)
- storage medium selectable in `app/SW_UDISK.h` (SPI flash / internal)

Builds clean (11.9 KB flash). Board not connected yet - flash/verify
with minichlink/wch-link when the demoboard arrives:

```sh
export PATH="/opt/wch/RISC-V_Embedded_GCC15/bin:$PATH"
make -j16
minichlink -w _build/app.bin 0x08000000
```
