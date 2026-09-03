# Lab 06 Troubleshooting: TFT ST7789V3

This lab has been confirmed working on real hardware. Depending on the specific module or wiring in hand, though, you might still run into one of the symptoms below.

## Screen stays completely dark (nothing but black)

| Check | Details |
|---|---|
| Wiring | Re-check all 7 pins (VCC/GND/SCLK/MOSI/CS/RST/DC) — CS (GPIO15) and DC (GPIO17) are especially easy to swap by mistake |
| RST polarity | The overlay's `reset-gpios` is set as `GPIO_ACTIVE_LOW` (low = reset asserted). It's rare for a module to be wired the opposite way, but if reset never seems to release, probe the actual logic level in `st7789_reset()` |
| Power | Confirm the 3.3V rail is actually stable and there's no bad breadboard contact |
| Reusing wiring from the `SSD1306`/`ST7735` labs | If you reused a breadboard, double-check that the previous lab's wiring actually matches this lab's pin assignment (see "Common SPI Lab Lessons" in the roadmap doc) |

## Init looks fine (serial output is normal) but the screen is dark or blank (backlight issue)

If the panel init sequence completes without errors but the screen stays dark regardless, the cause is more likely to be the **backlight (BLK) power** than SPI/GPIO wiring.

- If your module has a separate BLK pin, check that it's tied to 3.3V. BLK is a plain power pin, not a signal driven by a GPIO — so this is a wiring issue, not something to fix in code or devicetree.
- Some modules already tie the backlight to VCC internally, so they either have no BLK pin or light up without it wired — this item doesn't apply to those.
- Backlight LEDs can be polarity-sensitive (some modules expose LED+/LED- instead of a single BLK pin) — check the datasheet and wire the polarity correctly.

## Screen turns on but the image looks shifted or only partially visible (offset issue)

The ST7789 controller's native GRAM is 240x320, but this module only uses 240x280 of it. If `main.c`/the overlay's `x-offset` (default 0) and `y-offset` (default 20) don't match your actual module, the image can appear shifted up/down or clipped at the top or bottom.

- If the module vendor's example code or datasheet specifies an offset, use that value.
- If you don't know the value, try `y-offset` values around 0, 20, and 40 — layouts that use only 240x280 of a 240x320 GRAM typically either split the margin evenly (20 each) or push it to one side.

## Colors look inverted (e.g. a black background renders as white)

`st7789_init()` sends `INVON` (Display Inversion On, `0x21`). Most ST7789 glass needs this for correct colors, but some modules are the opposite — if so, try changing the `ST7789_INVON` call in `main.c` to `0x20` (`INVOFF`, Display Inversion Off).

## Red and blue appear swapped (RGB vs BGR)

Bit 3 (`0x08`) of the `madctl` value (`0x00`) in `st7789_init()` controls color order (RGB/BGR). If colors look swapped, try `madctl` = `0x08`.

## Screen appears flipped or rotated

Also a `madctl` value issue. MADCTL bit layout (typical for ST7789):

| Bit | Name | Meaning |
|---|---|---|
| 7 (`0x80`) | MY | Vertical flip |
| 6 (`0x40`) | MX | Horizontal flip |
| 5 (`0x20`) | MV | Row/column swap (rotation family) |
| 3 (`0x08`) | RGB/BGR | Color order |

Try combinations like `0x00`, `0x60`, `0xA0`, `0xC0` depending on how your module is physically mounted.

## Init hangs partway through, or an SPI write fails

`st7789_init()` / `st7789_fill_rect()` / `st7789_draw_char()` all report which step failed via printk if an SPI write fails (e.g. `"Fill (background) failed"`). If you see this:

1. Try lowering `spi-max-frequency` (currently 20MHz) — as learned in the other SPI labs in this series, SPI has no ACK, so wiring/signal integrity issues usually show up as visual glitches rather than write failures, but a long breadboard run is safer at a lower clock.
2. Double-check the overlay to confirm CS (GPIO15) is correctly pinmuxed to hardware CS0.

## Screen fills correctly but text doesn't show or looks garbled

- The font table (`font5x7`) used by `st7789_draw_char()` only contains the glyphs needed for `"Hello World!"`. If you modify the code to print other characters, you'll need to add those glyphs to the font table.
- If the text color (`fg`) and background color (`bg`) are the same, nothing will appear to render even though it is.
