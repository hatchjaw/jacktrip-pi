# A basic JackTrip client for bare-metal Raspberry Pi

Be sure to `--recurse-submodules` when cloning, as this project uses the very,
_very_ helpful [Circle](https://github.com/rsta2/circle) bare metal environment for Raspberry Pi.
A tip of the hat to Rene Stange.

Manually configure IPv4 for your ethernet interface:

- Address: 192.168.10.10
- Submask: 255.255.255.0
- Gateway: 192.168.10.1

Run JACK and start a JackTrip hub server:

```shell
jacktrip -S -q2 -p5
```

Connect a Raspberry Pi to your computer with an ethernet cable and after a few
seconds it should connect to the JackTrip server.

## Build & Install

- Install the arm toolchain, which might be in your package manager, e.g. for
  Arch/Manjaro:

```shell
# pacman -Syu aarch64-linux-gnu-gcc
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
  - this builds Circle's static libraries
- `cd boot` and, for the 64-bit toolchain (`aarch64-none-elf`), `make install64`
  - this builds the firmware and bootloader and installs them on the SD card
- `cd ../../src`, `make` and `make install`
  - this builds the kernel image and installs it on the SD card
- `cp cmdline.txt /run/media/tar/RPI` to use I2S instead of PWM sound

The script [buildall.sh](src/buildall.sh) encapsulate the last three points
above; useful if modifying Circle itself.
[build.sh](src/build.sh) handles the final bullet point, and copies cmdline.txt
to the SD card; useful if switching sound devices.

## Outlook

## Issues

- Too much logging obstructs other tasks, e.g. audio.
- Sometimes fifo reads get out of sync somehow, and periodic ring-mod-like
  distortion results.
- Occasional synchronous exceptions, again likely caused by logging too much,
  and/or from sensitive parts of the code.

## Nota bene

Don't be fooled by the CMake files, which are only present to make my IDE
behave nicely.
