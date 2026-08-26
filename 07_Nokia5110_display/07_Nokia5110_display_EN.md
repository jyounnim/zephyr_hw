# 7. Nokia 5110 (PCD8544) Monochrome LCD — 84x48, raw SPI

## Overview

Board: **ESP32-S3-DevKitC-1** (`esp32s3_devkitc/esp32s3/procpu`), framework: **Zephyr RTOS**.

This lab drives the classic **PCD8544-controller 84x48 monochrome LCD** ("Nokia 5110" module, named after the old Nokia phone screen it came from) over raw SPI. Same house style as the rest of this series (SSD1306, the I2C LCD, the SPI loopback lab) - no Zephyr Display/CFB subsystem, just SPI/GPIO handled directly in application code.

This module **has no MISO line at all** (it's a write-only display) - so unlike Lab 3's loopback test, this overlay only needs MOSI/SCLK/CS.

> ✅ **Hardware-verified**: confirmed working on real ESP32-S3-DevKitC-1 + Nokia 5110 hardware, with the default Vop (`0xB0`) displaying correctly as-is - no contrast tuning needed. The one real issue hit during bring-up wasn't the code or the overlay - it was **wiring**: the board was still wired for an earlier, unrelated test on different GPIOs, and that old wiring got reused without checking it against this lab's wiring table (RST=GPIO4, DC=GPIO5, CE=GPIO10, DIN=GPIO11, CLK=GPIO12). Worth remembering for the next SPI display lab: unlike I2C, SPI has no ACK/NACK (see Lab 3), so wrong wiring never shows up as a serial error - it just quietly shows nothing on screen.

## Requirements

- A Nokia 5110 (PCD8544) LCD module (typically 8 pins: RST, CE, DC, DIN, CLK, VCC, LIGHT, GND)
- ESP32-S3-DevKitC-1

## Wiring

| Signal | Role | ESP32-S3 connection |
|---|---|---|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| RST | Reset (active low) | GPIO4 |
| CE (CS) | Chip select | GPIO10 (SPI2 hardware CS0) |
| DC | Data/Command select | GPIO5 |
| DIN (MOSI) | Data in | GPIO11 |
| CLK (SCLK) | Clock | GPIO12 |
| LIGHT (BL) | Backlight | 3.3V or switched to GND (optional) |

> **Good news on power, for once**: unlike the PCF8574 LCD backpack (Lab 02) or the SSD1306, the Nokia 5110/PCD8544 module came out of an actual Nokia phone, so its **native logic level is 2.7-3.3V**. That's a natural match for the ESP32-S3's 3.3V-only GPIOs - no level shifter or 5V concerns needed, just wire it straight up. That said, some low-cost breakout boards' onboard regulator/resistor network assumes a 5V input, so check your specific module's silkscreen/listing before feeding VCC 5V if you're tempted to. This lab is written **assuming a direct 3.3V connection**.

## PCD8544 Command Set

The PCD8544 toggles between a "basic instruction set" and an "extended instruction set" while configuring.

| Command | Value | Meaning |
|---|---|---|
| Function Set (extended) | `0x21` | enter extended mode |
| Set Vop (contrast) | `0x80 \| Vop` | set contrast - this lab defaults to `0xB0` |
| Temperature Control | `0x04` | temperature coefficient 0 |
| Bias System | `0x14` | bias 1:48 |
| Function Set (basic) | `0x20` | back to basic mode |
| Display Control | `0x0C` | normal (non-inverted) display mode |

**The contrast (Vop) value varies a lot between modules.** The HD44780 LCD in Lab 02 had a physical trimmer you could turn by hand; the PCD8544 **has no trimmer at all - this Vop command value is the entire software contrast control**. If the screen shows nothing (too light) or comes up entirely black (too dark), adjust `PCD8544_SET_VOP_DEFAULT` in `main.c` (default `0xB0`) somewhere in the `0x80`-`0xFF` range.

## Addressing and the Framebuffer

- The screen is 84 x 48 pixels = 84 columns x 6 pages (8 pixels tall each)
- `0x80|x` sets the X (column) address, `0x40|y` sets the Y (page) address
- After setting the address, streaming data auto-increments X, **wrapping to the next page once X reaches 84** - so the entire framebuffer (84x6 = 504 bytes) can be written starting at (0,0) in one single transfer to refresh the whole screen

## Code Structure

- `pcd8544_cmd()` / `pcd8544_data()`: set the DC pin to 0 (command) or 1 (data), then send via `spi_write_dt()` (CE/CS is toggled automatically by the SPI driver around each transfer)
- `pcd8544_init()`: pulse RST, send the extended-mode commands (Vop/temperature/bias), switch back to basic mode, enable normal display
- `framebuffer[84*6]`: holds the whole screen; `fb_draw_char`/`fb_draw_string` render a 5x7 font into it, and `pcd8544_update()` sends it all at once
- `main()`: check SPI/GPIO readiness, initialize, print "Hello World!" / "Nokia 5110" on two lines

## Devicetree

```dts
&pinctrl {
    spim2_pcd8544: spim2_pcd8544 {
        group1 {
            pinmux = <SPIM2_MOSI_GPIO11>, <SPIM2_SCLK_GPIO12>, <SPIM2_CSEL_GPIO10>;
        };
    };
};

&spi2 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
    pinctrl-0 = <&spim2_pcd8544>;
    pinctrl-names = "default";

    pcd8544: pcd8544@0 {
        compatible = "zds,pcd8544";
        reg = <0>;
        spi-max-frequency = <1000000>;
        reset-gpios = <&gpio0 4 GPIO_ACTIVE_LOW>;
        dc-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>;
    };
};
```

The `reset-gpios`/`dc-gpios` property names deliberately match the ones Zephyr's own `mipi-dbi-spi` display binding uses - this lab doesn't use that framework (it's a custom `zds,pcd8544` binding instead), but keeping the naming convention aligned with the official binding avoids confusion if this ever gets ported to a real Zephyr display driver later.

The GPIO controller label used is `&gpio0` - ESP32-S3's devicetree splits GPIO into two controllers, `gpio0` (pins 0-31) and `gpio1` (pins 32-53); GPIO4 and GPIO5, used here, both fall under `gpio0`.

## Custom Devicetree Binding

```yaml
description: |
  PCD8544-based Nokia 5110 monochrome LCD (84x48), driven over raw SPI
  from application code - no Zephyr Display/CFB subsystem involved.

compatible: "zds,pcd8544"

include: spi-device.yaml

properties:
  reset-gpios:
    type: phandle-array
    required: true
    description: >
      Reset pin. Active low - pulse low to reset the PCD8544 controller.

  dc-gpios:
    type: phandle-array
    required: true
    description: >
      Data/Command select pin. Driven low before a command byte is
      written, high before a data byte is written.
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20.0)

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nokia5110_lab)

target_sources(app PRIVATE src/main.c)
```

(Same as Lab 3: since a custom binding is used, `DTS_ROOT` must be extended before `find_package(Zephyr...)`.)

## prj.conf

```
CONFIG_SPI=y
CONFIG_GPIO=y
CONFIG_PRINTK=y
```

(As confirmed in Lab 3, `CONFIG_ESP32_SPIM` auto-enables once the devicetree's SPI node is turned on, so it doesn't need to be added explicitly.)

## File Layout

```
Zephyr_display/
└── 07_Nokia5110_display/
    ├── lab/
    │   ├── src/
    │   │   └── main.c
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    │   ├── dts/
    │   │   └── bindings/
    │   │       └── display/
    │   │           └── zds,pcd8544.yaml
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 07_Nokia5110_display_EN.md
```

## Build & Run

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 07_Nokia5110_display/lab
west flash
west espressif monitor
```

### Expected serial output

```
Nokia 5110 (PCD8544) lab starting
Nokia 5110 initialized and "Hello World!" / "Nokia 5110" written
```

The screen's first page (top) should show `Hello World!`, and the second page `Nokia 5110`.

## Things to Notice

- **The Vop (contrast) value is the part of this lab most likely to need tuning** - since there's no physical trimmer, before suspecting the wiring if the screen looks blank, try a few different `PCD8544_SET_VOP_DEFAULT` values first.
- Being a write-only display with no MISO line makes a nice contrast with Lab 3 (SPI loopback) - that lab verified "the bus itself is sound," and this one builds a real device's command protocol on top of that assumption.
- Matching property names to Zephyr's own official bindings (`reset-gpios`/`dc-gpios`) means this devicetree can largely be reused as-is if this custom driver is ever swapped out for a real Zephyr Display driver later.

## Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| `SPI device not ready` / `RST/DC GPIO not ready` | The overlay isn't applied - check that the filename matches the board target |
| Screen stays completely blank/white | Vop too low (not enough contrast) - try raising `PCD8544_SET_VOP_DEFAULT` from `0xB0` (e.g. `0xB8`, `0xC0`) |
| Screen goes fully black / a checkerboard pattern | Vop too high (too much contrast) - try lowering the value, or check that the RST sequence completed correctly |
| Serial shows an init failure / SPI write error | Re-check the wiring (especially CE/CS, DIN/MOSI, CLK). Run `west build -t devicetree` to confirm the `&spi2` node merged correctly |
| Characters are garbled or in the wrong place | Check the 6-pixel spacing logic in `fb_draw_string`, and that the page argument is within 0-5 |
| Build error (can't find `SPIM2_MOSI_GPIO11` etc.) | Same issue as Labs 1 and 3 - use `west build -t devicetree` to see which pinmux macros are actually available |
| `'zds,pcd8544' compatible not found` | Check that `CMakeLists.txt`'s `list(APPEND DTS_ROOT ...)` comes before `find_package(Zephyr...)` |

## Next

Later labs can extend this same raw-SPI-plus-custom-binding pattern to other display controllers - a SHARP memory LCD, an ST7735 color TFT, and so on.
