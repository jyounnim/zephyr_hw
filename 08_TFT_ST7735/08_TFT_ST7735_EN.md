# 8. ST7735 128x160 Color TFT — raw SPI

## Overview

Board: **ESP32-S3-DevKitC-1** (`esp32s3_devkitc/esp32s3/procpu`), framework: **Zephyr RTOS**.

The first **color** display in this series - an **ST7735-controller 128x160 color TFT** (commonly sold as a generic "1.8 inch SPI TFT" module), driven over raw SPI. Same as every lab so far: no Zephyr Display/CFB subsystem, SPI/GPIO handled directly in application code.

Like the Nokia 5110 (Lab 07), this module is a **write-only display with no MISO line**, so the overlay only needs MOSI/SCLK/CS.

> ✅ **Hardware-verified** — confirmed on real ESP32-S3-DevKitC-1 + ST7735 128x160 TFT hardware. An initial blank-screen report turned out not to be a code/overlay bug but a wiring mistake (the same pattern as Lab 07) - once the wiring was corrected, both the text and the color bars displayed correctly. A good reminder that with SPI, wrong wiring produces no error at all, just a blank screen.

## Requirements

- An ST7735 128x160 color TFT module ("green tab" variant - see the "Tab Color" section below)
- ESP32-S3-DevKitC-1

## Wiring

| Signal | Role | ESP32-S3 connection |
|---|---|---|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| RST | Reset (active low) | GPIO16 |
| CS | Chip select | GPIO15 (SPI2 hardware CS0) |
| DC (some modules label it A0/RS) | Data/Command select | GPIO17 |
| SDA (MOSI) | Data in | GPIO13 |
| SCL (SCLK) | Clock | GPIO14 |
| LED (BLK) | Backlight | 3.3V (or GPIO-controlled brightness - tied high/always-on in this lab) |

The ST7735 IC itself is mostly 2.8-3.3V logic, so like the Nokia 5110 (Lab 07) it's a reasonably good voltage match for the ESP32-S3. That said, some breakout boards have an onboard regulator and accept 5V VIN while still outputting 3.3V logic - check your specific module's spec, but the logic side is typically 3.3V regardless of what VIN it's fed.

## "Tab Color" — Why It Matters

ST7735 128x160 modules ship with a colored tab sticker on the actual panel glass - red, green, black, etc. - marking a **small manufacturing-batch offset difference** in the panel itself. Same ST7735 controller, but depending on the tab color:

- The display area's starting coordinate offset (added on top of CASET/RASET) differs
- The default MADCTL (rotation/color order) value can differ

This lab is written for the **most common "green tab"** variant (column +2, row +1 offset). If the image looks cropped by a few pixels on one edge with a black margin on the opposite edge, that's almost certainly this offset not matching your actual module - adjust `ST7735_XSTART`/`ST7735_YSTART` in `main.c` (red tab is usually both 0).

## Initialization Sequence

Written and cross-checked against the "Rcmd1 + Rcmd3" (ST7735R, green tab) tables in the widely-used Adafruit_ST7735 library - effectively the industry-standard sequence for this chip - with the command/argument/delay values compared byte-for-byte against that library's source.

| Command | Args | Meaning |
|---|---|---|
| SWRESET (`0x01`) | - | Software reset, wait 150ms |
| SLPOUT (`0x11`) | - | Sleep out (exit power-save), wait 500ms |
| FRMCTR1-3 (`0xB1-0xB3`) | 3/3/6 bytes | Frame rate control |
| INVCTR (`0xB4`) | 1 byte | Display inversion control |
| PWCTR1-5 (`0xC0-0xC4`) | - | Power control (voltage regulator settings) |
| VMCTR1 (`0xC5`) | 1 byte | VCOM voltage control |
| INVOFF (`0x20`) | - | Color inversion off |
| MADCTL (`0x36`) | `0xC8` | Memory access control - rotation/BGR order (green-tab default) |
| COLMOD (`0x3A`) | `0x05` | Pixel format = 16-bit (RGB565) |
| GMCTRP1/GMCTRN1 (`0xE0`/`0xE1`) | 16 bytes each | Gamma correction tables (positive/negative) |
| NORON (`0x13`) | - | Normal display mode |
| DISPON (`0x29`) | - | Display on |

RST is hardware-reset (10ms low pulse, then a 120ms wait) before this sequence runs.

## Pixel Format and Addressing

- `COLMOD=0x05` → **RGB565** (16 bits/pixel: 5 bits R, 6 bits G, 5 bits B), high byte transmitted first
- `CASET`/`RASET` set a (x0,y0)-(x1,y1) rectangular window, then `RAMWR` streams data that fills it in row-major order
- This lab does **not** keep a full framebuffer - the Nokia 5110 (Lab 07) was only 84x6=504 bytes, small enough to hold as a static array, but this screen is 128x160x2=40,960 bytes, too much to justify as a static buffer. Instead, only the specific rectangle needed at the moment (one character, one color bar) is addressed and streamed on demand.

## Code Structure

- `st7735_cmd()` / `st7735_data()`: same pattern as Lab 07 (set the DC pin, then `spi_write_dt()`)
- `init_seq[]` + `st7735_run_init_sequence()`: the init sequence expressed as a (command, args, delay) table - structured this way specifically so it stays easy to diff against the reference library byte-for-byte
- `st7735_set_addr_window()`: sets a rectangular window via CASET/RASET/RAMWR
- `st7735_fill_rect()`: fills a window with a solid color (a 64-pixel chunk resent repeatedly - keeps even a large rectangle within a small stack buffer)
- `st7735_draw_char()`/`st7735_draw_string()`: renders the 5x7 font pixel-by-pixel, sent as one 6x8 region at a time
- `main()`: fill the whole screen black → print "Hello World!" (white) / "ST7735 TFT" (cyan) → draw **R/G/B/Y/M/C/W color bars**

## Why Color Bars — a Built-in Diagnostic

This lab bakes in a lesson from earlier labs right from the start, instead of adding it after the fact. Like the SSD1306 lab's ALL-ON/ALL-OFF test or the Nokia 5110's Vop sweep, **the most common bug on a color display (R/G/B channel order swapped, a pixel byte-order mistake) is completely invisible on a monochrome display.** So rather than stopping at text, this lab draws R/G/B/Y/M/C/W color bars alongside it from the very first boot - if the "R" bar comes out blue, MADCTL's BGR/RGB bit is flipped; if every color looks subtly off, suspect the pixel data is going out little-endian instead of big-endian.

## Devicetree

```dts
&pinctrl {
    spim2_st7735: spim2_st7735 {
        group1 {
            pinmux = <SPIM2_MOSI_GPIO13>, <SPIM2_SCLK_GPIO14>, <SPIM2_CSEL_GPIO15>;
        };
    };
};

&spi2 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
    pinctrl-0 = <&spim2_st7735>;
    pinctrl-names = "default";

    st7735: st7735@0 {
        compatible = "zds,st7735";
        reg = <0>;
        spi-max-frequency = <4000000>;
        reset-gpios = <&gpio0 16 GPIO_ACTIVE_LOW>;
        dc-gpios = <&gpio0 17 GPIO_ACTIVE_HIGH>;
    };
};
```

Same as Lab 07, `reset-gpios`/`dc-gpios` match Zephyr's own `mipi-dbi-spi` binding naming, and the GPIO controller used is `&gpio0` (pins 0-31) - GPIO13-17, used here, all fall in that range.

## Custom Devicetree Binding

```yaml
description: |
  ST7735-based 128x160 color TFT ("green tab" variant), driven over raw
  SPI from application code - no Zephyr Display/CFB subsystem involved.

compatible: "zds,st7735"

include: spi-device.yaml

properties:
  reset-gpios:
    type: phandle-array
    required: true
    description: >
      Reset pin. Active low - pulse low to reset the ST7735 controller.

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
project(st7735_tft_lab)

target_sources(app PRIVATE src/main.c)
```

(Same as Labs 3 and 7: since a custom binding is used, `DTS_ROOT` must be extended before `find_package(Zephyr...)`.)

## prj.conf

```
CONFIG_SPI=y
CONFIG_GPIO=y
CONFIG_PRINTK=y
```

## File Layout

```
Zephyr_display/
└── 08_TFT_ST7735/
    ├── lab/
    │   ├── src/
    │   │   └── main.c
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    │   ├── dts/
    │   │   └── bindings/
    │   │       └── display/
    │   │           └── zds,st7735.yaml
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 08_TFT_ST7735_EN.md
```

## Build & Run

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 08_TFT_ST7735/lab
west flash
west espressif monitor
```

### Expected serial output

```
ST7735 TFT lab starting
ST7735 initialized and demo screen (text + color bars) drawn
```

The screen should show white `Hello World!` and cyan `ST7735 TFT` on a black background, followed by seven color bars below in order: red, green, blue, yellow, magenta, cyan, white.

## Things to Notice

- **The color bars are this lab's real first troubleshooting tool** - before worrying about whether the text looks right, check whether the bar colors and order actually match R/G/B/Y/M/C/W.
- If the edges of the screen look cropped or shifted by a few pixels, that's most likely a tab-color offset issue (see the "Tab Color" section above) - like the Nokia 5110's Vop, this has no physical fix; it's a pure software constant to adjust.
- Drawing only what's needed on demand (no framebuffer) contrasts with the Nokia 5110's "always send the whole framebuffer" approach - a good illustration of the point where holding a full framebuffer in memory stops being cheap as the screen gets bigger.

## Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| `SPI device not ready` / `RST/DC GPIO not ready` | The overlay isn't applied - check that the filename matches the board target |
| Screen stays completely white/black/unresponsive | Re-check RST/DC wiring. As with Lab 3/7, SPI has no ACK, so wrong wiring shows up exactly like this - silently, with no error |
| Color bars come out with swapped colors (e.g. "R" looks blue) | MADCTL's BGR/RGB bit doesn't match this module - try toggling the BGR bit in `init_seq[]`'s MADCTL (`0x36`) argument (`0xC8`) |
| Image is cropped on one edge with a margin on the other | Tab-color offset mismatch - try setting `ST7735_XSTART`/`ST7735_YSTART` to 0 (typical for red/black tab modules) |
| Image appears mirrored or upside down | MADCTL's MX/MY bits - adjust the top two bits of `0xC8` |
| Characters are garbled or misplaced | Check `st7735_draw_char`'s 6x8 pixel layout, or the font table |
| Build error (can't find `SPIM2_MOSI_GPIO13` etc.) | Same issue as the earlier SPI labs - use `west build -t devicetree` to see which pinmux macros are actually available |
| `'zds,st7735' compatible not found` | Check that `CMakeLists.txt`'s `list(APPEND DTS_ROOT ...)` comes before `find_package(Zephyr...)` |

## Next

By this lab, the raw-SPI-plus-custom-binding pattern has scaled from a monochrome character LCD (Lab 02, I2C) to a monochrome graphic LCD (Lab 07, SPI) to a color TFT (Lab 08, SPI). Later labs can carry this pattern over to sensors or motor control, or keep extending it to displays like the ST7789 (a higher-resolution cousin controller).
