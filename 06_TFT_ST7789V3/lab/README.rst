.. zephyr:code-sample:: tft-st7789v3
   :name: TFT ST7789V3 (1.69" 240x280, raw SPI)
   :relevant-api: spi_interface

   Draw color bars and "Hello World!" on a 1.69" 240x280 ST7789V3 SPI
   TFT panel using raw SPI writes instead of a Zephyr display driver.

Overview
********

This sample drives a 1.69" 240x280 color TFT panel built around the
Sitronix ST7789V3 controller, over SPI. It talks to the controller
directly with :c:func:`spi_write_dt` (command/data selected via a
D/C GPIO), the same raw-SPI approach used by the other SPI display
labs in this series, rather than Zephyr's in-tree
``sitronix,st7789v`` display driver.

Requirements
************

* An :zephyr:board:`esp32s3_devkitc` board
* A 1.69" 240x280 ST7789V3 SPI TFT panel

Wiring
******

* Panel VCC -> board 3.3V
* Panel GND -> board GND
* Panel SCL/SCLK -> board GPIO14 (SPI2 SCLK)
* Panel SDA/MOSI -> board GPIO13 (SPI2 MOSI)
* Panel CS -> board GPIO15 (SPI2 hardware CS0)
* Panel RES/RST -> board GPIO16
* Panel DC -> board GPIO17
* Panel BLK (backlight) -> board 3.3V, if the module exposes a
  separate BLK pin

This is the same SPI2 pin assignment as this series' ST7735 lab -
reuse that lab's wiring as-is.

BLK is a plain power pin, not a controlled GPIO signal - just tie it
to 3.3V. Some modules tie the backlight to VCC internally and have no
separate BLK pin at all; on those, no extra wiring is needed.

Building and Running
*********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/esp32s3_devkitc/tft_st7789v3
   :board: esp32s3_devkitc/esp32s3/procpu
   :goals: build flash

Sample Output
=============

.. code-block:: console

   === TFT ST7789V3 (SPI, 240x280) ===
   ST7789V3 initialized, color bars + "Hello World!" drawn

The panel itself should show ``Hello World!`` near the top and a
stack of color bars filling the rest of the screen.
