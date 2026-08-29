# CH585M USBHS MSC (U-Disk) demo

Port of the WCH CH585 EVT example `EXAM/USB/USBHS/DEVICE/MSC_U-Disk` to a
standalone GCC make project (same layout as the sibling
[`../ch592f`](../ch592f) and [`../ch32l103`](../ch32l103) projects).

The firmware enumerates the CH585M as a **USB mass-storage device (U-Disk)**
over the USBHS (high-speed capable) peripheral, exposing either the internal
flash or an external SPI flash as the disk, selected by `STORAGE_MEDIUM` in
`app/SW_UDISK.h` (`MEDIUM_SPI_FLASH` / `MEDIUM_INTERNAL_FLASH`).

## Hardware notes

| Item        | Value                                              |
|-------------|----------------------------------------------------|
| MCU         | WCH CH585M (RISC-V QingKe V4, `rv32imc_zba_zbb_zbc_zbs_xw`) |
| Debug UART  | UART0 — TX: PB7, RX: PB4 (see `app/main.c` DebugInit) |
| System clock| 62.4 MHz HSE+PLL by default (`SYSCLK_FREQ`)          |
| USB         | USBHS device; **high-speed mode needs an external
crystal** (per the EVT note) — with the internal oscillator only
full-speed is reliable |

## Project structure

```
ch585m_usbhs_msc/
├── app/                        MSC example user code (from EVT User/)
│   ├── main.c                  Entry point, storage-medium init
│   ├── ch585_usbhs_device.[ch] USBHS device stack (endpoints, IRQ)
│   ├── usb_desc.[ch]           Device/config/string descriptors
│   ├── SW_UDISK.[ch]           SCSI/MSC command layer
│   ├── SPI_FLASH.[ch]          External SPI-flash storage backend
│   └── Internal_Flash.[ch]     Internal-flash storage backend
├── vendor/
│   ├── Ld/Link.ld              Linker script (from EVT EXAM/SRC)
│   ├── RVMSIS/                 RISC-V CMSIS core files
│   ├── StdPeriphDriver/        CH58x peripheral library + libISP585.a
│   └── Startup/startup_CH585.S Reset/startup assembly
├── Makefile                    Build configuration (PROJECT=app)
├── rules.mk                    Generic GCC build rules (CH585 arch flags)
└── version                     Firmware version number
```

## Build

```sh
export PATH="/opt/wch/RISC-V_Embedded_GCC15/bin:$PATH"
make -j16        # -> _build/app.{elf,bin,hex}
```

## Flash

Via WCH-LinkE + minichlink (same as the CH592F board):

```sh
minichlink -w _build/app.bin 0x0
minichlink -a && minichlink -b   # reboot and run
```

The demoboard is not connected yet - build-verify only for now.

## Next steps when the board is connected

1. Check the UART0 banner (`USBHS UDisk Demo`, storage medium) at 115200.
2. Decide the storage medium (`STORAGE_MEDIUM` in `app/SW_UDISK.h`) based on
   what the demoboard carries (most CH585 boards have an external SPI flash).
3. For USB high-speed: fit/verify the external crystal and keep
   `SYSCLK_FREQ` HSE-based.
