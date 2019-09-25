raspbootin
==========

**Modified for use with the Raspberry Pi 2 only for CPEN 432**

See https://github.com/mrvn/raspbootin for the original
and more information on how this works.

Usage (CPEN 432):
------

#### Building
Ensure that the toolchain (`arm-none-eabi-gcc`) is runnable:
```sh
$ arm-none-eabi-gcc --version
arm-none-eabi-gcc (GNU Tools for ARM Embedded Processors) 4.9.3 20150529 (release) [ARM/embedded-4_9-branch revision 224288]
Copyright (C) 2014 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

To build run `make`. This will build `raspbootin/kernel.img` and `raspbootcom/raspbootcom`

```sh
$ make
```

#### SD Card Setup
To set up the SD Card ensure its formatted correctly as with FAT.

Get the latest firmware for the Raspberry Pi from https://github.com/raspberrypi/firmware.

Copy the `start.elf` and `bootcode.bin` from it.
```sh
$ git clone https://github.com/raspberrypi/firmware rpi-firmware
$ cd rpi-firmware
$ cp boot/start.elf boot/bootcode.bin /mnt/myraspberrypi-sdcard
```

Then copy the `raspbootin/kernel.img` and `raspbootin/config.txt`
built in the previous step to the SD Card for the Raspberry Pi 2.
```sh
$ make
$ cp raspbootin/config.txt /mnt/myraspberrypi-sdcard/
$ cp raspbootin/kernel.img /mnt/myraspberrypi-sdcard/kernel7.img
```

Overall only these files should exist on the SD Card:
```
bootcode.bin
config.txt
kernel7.img
start.elf
```

#### Loading new Kernels
1. Ensure the Raspberry Pi is off and the serial cable is correctly plugged in.
2. Run the `raspbootcom` executable that was built.
```sh
$ ./raspbootcom/raspbootcom /dev/ttyUSB0 /path/to/my/kernel.img
```
3. Turn on the Raspberry Pi.

4. To upload a new kernel (Assuming the new one is at the same file location) simply turn the Raspberry Pi off and back on
   **there is no need to close raspbootcom since it always re-reads the kernel file**

