# Lab 06: TFT ST7789V3 (1.69" 240x280, raw SPI)

## 1. Overview

Board: **ESP32-S3-DevKitC-1** (`esp32s3_devkitc/esp32s3/procpu`). Framework: **Zephyr RTOS**.

This lab drives a 1.69" 240x280 color TFT panel built around the Sitronix **ST7789V3** controller, over SPI. Instead of Zephyr's Display/CFB subsystem or the in-tree `sitronix,st7789v` driver, it talks to the controller directly with **raw SPI**, the same approach used by the other SPI display labs in this series.

## 2. Wiring

This lab reuses the **exact same SPI2 pin assignment** as this series' ST7735 lab (Lab 08) — no need to rewire the breadboard, just swap the module in on the same wiring.

| Signal | Role | ESP32-S3-DevKitC-1 |
|---|---|---|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCL/SCLK | SPI clock | GPIO14 (SPI2 SCLK) |
| SDA/MOSI | SPI data | GPIO13 (SPI2 MOSI) |
| CS | Chip select | GPIO15 (SPI2 hardware CS0) |
| RES/RST | Reset | GPIO16 |
| DC | Data/Command select | GPIO17 |
| BLK | Backlight | 3.3V (see below) |

MISO is intentionally not wired — this panel is write-only from the MCU's side.

**BLK (backlight) pin**: if your module exposes a separate BLK pin, it needs to be tied to 3.3V for the backlight to turn on — this is a plain power connection, not something driven by a GPIO. Some modules instead tie the backlight to VCC internally, so they either have no BLK pin at all or light up without any extra wiring. If your module has a BLK pin and the screen stays dark or off, check this pin first.

## 3. Panel GRAM Offset

The ST7789 controller's native GRAM is 240x320. The 1.69" module used in this lab has a physical glass panel of only 240x280, so it uses only the middle slice of the controller's GRAM — meaning a fixed offset has to be added every time an addressing window is set.

This lab uses X offset 0, Y offset 20 as defaults (a commonly documented value for 1.69" 240x280 ST7789V3 modules). The offsets live in the overlay's `x-offset`/`y-offset` properties and `main.c`'s `X_OFFSET`/`Y_OFFSET` (both read from devicetree) — adjust them here if the image appears shifted or clipped (see the troubleshooting doc for details).

## 4. Devicetree

Since this lab doesn't use a Zephyr in-tree display driver, it defines a custom binding, `zds,st7789v`, to hold the wiring info (SPI bus/CS, RST/DC pins) and panel spec (resolution, offsets) in devicetree — the same pattern as Lab 07 (`zds,pcd8544`) and Lab 08 (`zds,st7735`) in this series.

```dts
&pinctrl {
    spim2_default: spim2_default {
        group1 {
            pinmux = <SPIM2_SCLK_GPIO14>, <SPIM2_MOSI_GPIO13>, <SPIM2_CSEL_GPIO15>;
        };
    };
};

&spi2 {
    status = "okay";
    pinctrl-0 = <&spim2_default>;
    pinctrl-names = "default";

    st7789v_disp: st7789v@0 {
        compatible = "zds,st7789v";
        reg = <0>;
        spi-max-frequency = <20000000>;
        reset-gpios = <&gpio0 16 GPIO_ACTIVE_LOW>;
        dc-gpios = <&gpio0 17 GPIO_ACTIVE_HIGH>;
        width = <240>;
        height = <280>;
        x-offset = <0>;
        y-offset = <20>;
    };
};
```

`main.c` pulls the SPI spec and RST/DC GPIO specs straight from this node with `SPI_DT_SPEC_GET()` / `GPIO_DT_SPEC_GET()` — pin numbers and the SPI clock are never hardcoded in the source.

## 5. Code Structure

- `st7789_write_cmd()` / `st7789_write_data()`: toggle the DC pin to command/data mode, then send 1 byte (command) or N bytes (data) with `spi_write_dt()`. CS is toggled automatically per transaction by SPI2 hardware CS0.
- `st7789_reset()`: pulses the RST pin (assert low, then deassert high) and waits for the controller to come out of reset.
- `st7789_init()`: the standard ST7789 init sequence — Software Reset → Sleep Out → COLMOD (16bpp RGB565) → MADCTL (orientation/color order) → Display Inversion On → Normal Display Mode On → Display On.
- `st7789_set_addr_window()`: sets the rectangular pixel-write window via CASET/RASET/RAMWR. X_OFFSET/Y_OFFSET are added here in a single place, so every other function that uses it doesn't need to worry about the offset.
- `st7789_fill_rect()`: fills a rectangle with a solid color. Instead of keeping a full-screen framebuffer in RAM, it builds a single-row buffer and re-sends it for as many rows as needed — the same approach used by the other raw-SPI display labs in this series.
- `st7789_draw_char()` / `st7789_draw_string()`: draws a 5x7 bitmap font scaled by `scale`. Each character streams row by row, with no per-character buffer.
- `main()`: reset → init → fill black background → fill rainbow color bars → draw `"Hello World!"` text.

## 6. Build & Run

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 06_TFT_ST7789V3/lab
west flash
west espressif monitor
```

### Expected Serial Output

```
=== TFT ST7789V3 (SPI, 240x280) ===
ST7789V3 initialized, color bars + "Hello World!" drawn
```

The panel should show `Hello World!` near the top, with red/green/blue/yellow/cyan/magenta/white color bars filling the rest of the screen below it.

## 7. Notes

This lab has been confirmed working on real hardware (ESP32-S3-DevKitC-1 + a 1.69" 240x280 ST7789V3 module). Symptoms that can vary by the specific module in hand — such as a shifted image or unexpected colors — are covered in the separate troubleshooting doc (`06_TFT_ST7789V3_TROUBLESHOOTING_EN.md`).

## 8. File Layout

```
06_TFT_ST7789V3/
├── 06_TFT_ST7789V3_KR.md
├── 06_TFT_ST7789V3_EN.md
├── 06_TFT_ST7789V3_TROUBLESHOOTING_KR.md
├── 06_TFT_ST7789V3_TROUBLESHOOTING_EN.md
└── lab/
    ├── CMakeLists.txt
    ├── README.rst
    ├── prj.conf
    ├── sample.yaml
    ├── boards/
    │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    ├── dts/
    │   └── bindings/
    │       └── display/
    │           └── zds,st7789v.yaml
    └── src/
        └── main.c
```
