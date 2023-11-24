## Build & Install

- Install the toolchain:
```shell
pacman -Syu aarch64-linux-gnu-gcc
pamac install aarch64-none-elf-gcc-bin
```
- `cd circle` and create a `Config.mk` file:
```makefile
PREFIX64 = aarch64-none-elf-
AARCH = 64
RASPPI = 3
SDCARD = /run/media/tar/RPI
```
- run `./makeall --nosample`
  - this builds circle's static libraries
- `cd boot` and, for the 64-bit toolchain (`aarch64-none-elf`), `make install64`
  - this builds the firmware and bootloader and installs them on the SD card
- `cd ../sample/[sample]`, `make` and `make install`
  - this builds the kernel image and installs it on the SD card
