.. zephyr:code-sample:: oled-ssd1306-i2c-autoaddr
   :name: OLED SSD1306 (I2C mode, address auto-detect)
   :relevant-api: i2c_interface

   Scan I2C0 for an SSD1306 OLED at 0x3C or 0x3D, auto-select whichever
   responds, then draw "Hello World!" on a 128x64 panel using raw I2C
   writes instead of a Zephyr display driver.

Overview
********

This sample drives a 0.96" SSD1306 128x64 monochrome OLED in I2C mode.
SSD1306 I2C modules ship strapped to one of two 7-bit addresses (0x3C
or 0x3D) depending on how the module's SA0 pin is wired, and a
devicetree node's ``reg`` property is fixed at build time - so a
standard driver can only target one address per build. This sample's
whole point is to *scan* for whichever address is actually present at
boot (the same pattern as the Lab 01 bus scanner) and use that, so it
talks to the controller directly with :c:func:`i2c_write` rather than
going through Zephyr's Display/CFB subsystem.

Requirements
************

* An :zephyr:board:`esp32s3_devkitc` board
* An SSD1306-based 128x64 I2C OLED module

Wiring
******

* Module VCC -> board 3.3V
* Module GND -> board GND
* Module SDA -> board GPIO8 (I2C0 SDA, series-wide pin)
* Module SCL -> board GPIO9 (I2C0 SCL, series-wide pin)
* Module RST (only on modules that expose this pin) -> board GPIO4,
  and set ``OLED_USE_HW_RESET`` to 1 in ``main.c`` - most low-cost
  4-pin modules have no RST pin and don't need this

Building and Running
*********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/esp32s3_devkitc/oled_ssd1306_i2c
   :board: esp32s3_devkitc/esp32s3/procpu
   :goals: build flash

Sample Output
=============

.. code-block:: console

   === OLED SSD1306 (I2C, ESP32-S3) ===
   Scanning for SSD1306 at 0x3C / 0x3D...
     found device at 0x3c
   SSD1306 initialized at 0x3C, "Hello World!" written

The OLED itself should show ``Hello World!`` on the first line and
``Addr 0x3C`` (or ``0x3D``) on the third line.
