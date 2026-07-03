# custom-LCD-driver

Linux kernel I2C client driver for Raspberry Pi 3B (BCM2837, ARM) that controls an HD44780 LCD through a PCF8574 backpack and exposes a character device at `/dev/lcd-screen`.

## Features

- I2C client driver using `struct i2c_driver`
- Device match by ID table name: `lcd-screen`
- Character device interface: `/dev/lcd-screen`
- Write path uses `copy_from_user`
- Mutex-protected I2C transfer sequence for concurrency safety
- HD44780 4-bit initialization and nibble transfer over PCF8574
- Buffered text state machine with multi-page rendering (16x2 pages)
- Background page switching implemented with `struct delayed_work`

## Project Layout

- `src/main.c`: minimal module init/exit, registers i2c driver
- `src/lcd_i2c.c`: probe/remove, device lifecycle
- `src/lcd_hw.c`: PCF8574 transport and HD44780 protocol
- `src/lcd_chardev.c`: character device, buffered write path, page scheduler
- `src/lcd_screen.h`: shared types, constants, prototypes

## Pin Mapping (PCF8574 -> HD44780)

- P0 -> RS
- P1 -> RW (kept low for write)
- P2 -> E
- P3 -> Backlight
- P4 -> D4
- P5 -> D5
- P6 -> D6
- P7 -> D7

## Build

Update `KERNELDIR` and `CROSS` in `Makefile` if your paths differ.

```bash
make
```

Clean:

```bash
make clean
```

## Load and Unload

```bash
sudo insmod lcd_screen.ko
sudo rmmod lcd_screen
```

Check kernel messages:

```bash
dmesg | tail -n 50
```

## Create the I2C Device (if not from device tree)

Find I2C bus and address first (common PCF8574 addresses are 0x27 or 0x3f):

```bash
i2cdetect -y 1
```

Instantiate the client with the exact driver name:

```bash
echo lcd-screen 0x3f > /sys/bus/i2c/devices/i2c-1/new_device
```

Remove it later:

```bash
echo 0x3f > /sys/bus/i2c/devices/i2c-1/delete_device
```

## Use the Character Device

Write text directly to the LCD:

```bash
echo "Hello LCD" > /dev/lcd-screen
```

or

```bash
sudo sh -c 'echo "Raspberry Pi" > /dev/lcd-screen'
```

Notes:

- The driver removes `\n` and `\r` from input text before storing the message.
- The display is rendered as fixed 16x2 pages (32 characters per page).
- If the text is longer than one page, pages rotate every 3000 ms.
- On each write, pending page transitions are canceled, page 0 is drawn immediately, then scheduling restarts.

## Driver Flow

- In probe:
  - Allocates and initializes device state
  - Registers `/dev/lcd-screen`
  - Runs HD44780 4-bit initialization sequence
  - Starts delayed-work pagination loop
- In write:
  - Copies userspace bytes with `copy_from_user`
  - Stops pending page work
  - Updates internal message buffer and resets page index to 0
  - Renders page 0 immediately
  - Re-arms delayed work for next page switch
- In remove:
  - Stops delayed work
  - Unregisters character device
  - Turns off LCD backlight
  - Destroys mutex and exits cleanly

## Troubleshooting

- `No such file or directory: /dev/lcd-screen`
  - Driver may not be probed yet. Ensure the I2C device exists as `lcd-screen`.
- I2C transfer failures
  - Confirm wiring, bus number, and address with `i2cdetect`.
- Wrong characters or no output
  - Recheck backpack pin mapping and LCD power/contrast.
- Text does not rotate across pages
  - Ensure your message is longer than 32 characters and module is loaded without runtime errors.

## License

GPL-2.0-or-later
