.. _tc32_general:

General information about the TC32 port
=======================================

- [Linux and Windows binaries of the compiler
  toolchain](https://github.com/Ai-Thinker-Open/Telink_825X_SDK/blob/master/README.md)
- [TLSR8232 documentation and
  SDK](http://wiki.telink-semi.cn/wiki/chip-series/TLSR823x-Series)
- [archive.org mirror of both of the above](https://archive.org/details/tc32_compiler)

To build, install both the toolchain and the SDK somewhere, and do this:

```
$ cd ports/tc32
$ make TC32_HOME=$COMPILER_PATH TC32SDK=$SDK_PATH/ble_sdk_hawk -j $(nproc)
```

You will be left with a `ports/tc32/build/firmware.bin` and corresponding ELF
file. You can flash the device with
[telinkdebugger](https://github.com/davidgiven/telinkdebugger):

```
$ ./client.py --serial_port=/dev/ttyACM1 write_flash firmware.bin
$ ./client.py --serial_port=/dev/ttyACM1 run
```

Currently this only works on the LT716 smartwatch platform, which in addition
seems not to be very standardised, so there might be rough edges. The port will
communicate via 3.3V TTL UART at 115200 baud 8n1 on the Tx/Rx pins.

The following hardware is supported:

- flash; the leftover flash is formatted as a LFS1 filesystem automatically (you
  get about 400kB free)
- GPIO pins, via the `machine.Pin` class
- the screen, via the `tc32.Screen` class
- battery voltage measurement via `tc32.battery_mv()`
- the button (via `Pin("BUTTON", Pin.IN)`).

Notable missing features:

- the screen is hardcoded for my LT716 smartwatch with a GC9106 80x160 screen.
  Other models seem to have different screens.
- no power management (it will run for about 40 minutes off the battery).
- no pin features except GPIO.
- floating point.
