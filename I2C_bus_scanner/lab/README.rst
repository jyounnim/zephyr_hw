.. zephyr:code-sample:: i2c_bus_scanner
   :name: I2C Bus Scanner
   :relevant-api: i2c_interface

   Scan I2C0 and I2C1 for connected devices once at boot, from a dedicated thread.

Overview
********

This sample scans I2C0 then I2C1 once, from a dedicated thread
(``K_THREAD_DEFINE``) that starts automatically at boot. It walks the
standard 7-bit address range (0x08-0x77) issuing a 1-byte read to each
address, and prints an ``i2cdetect``-style grid showing which
addresses ACKed. It is meant as a quick way to check what address a
newly-attached sensor or module shows up at.

Devicetree notes
*****************

* ``&i2c0`` / ``&i2c1`` are the SoC's I2C controllers
  (``sr100_m55.dtsi:268,280``).
* I2C1 is already enabled with pinctrl in the board ``.dts``
  (``sr100_rdk_m55.dts:141-144``) and already has a PCA6416A GPIO
  expander on it — no overlay needed for I2C1.
* I2C0 ships ``status = "disabled"`` with no pinctrl in the SoC dtsi
  (``sr100_m55.dtsi:277``). Its pin groups (``i2c0_ms_scl`` /
  ``i2c0_ms_sda``) exist in ``sr100_pinctrl.dtsi`` (lines 603, 586)
  but are marked ``/omit-if-no-ref/``, so they drop out of the build
  unless referenced. The overlay in ``boards/`` enables I2C0 by
  referencing them.
* A 1-byte ``i2c_read()`` probe is used per address — confirmed
  reliable on this board's DesignWare I2C driver port.
* ``CONFIG_I2C_DW=y`` is set explicitly in ``prj.conf``.

TODO/VERIFY
***********

* I2C0's 100 kHz clock-frequency (mirrored from I2C1's board setting)
  may need to change depending on the device attached.
* Confirm which physical connector pins ``i2c0_ms_scl``/``i2c0_ms_sda``
  map to, to avoid conflicts with other samples.

Building and Running
*********************

Build for the SR110 RDK (M55 core). The overlay file name
(``boards/sr100_rdk_sr100_m55.overlay``) matches Zephyr's board-target
auto-overlay convention (board target with ``/`` replaced by ``_``), so
it is picked up automatically — no ``-DEXTRA_DTC_OVERLAY_FILE`` needed.

.. code-block:: console

   west build -b sr100_rdk/sr100/m55 samples/i2c_bus_scanner

Flash the board:

.. code-block:: console

   west flash

Sample Output
*************

Open the console at 230400bps 8N1 — the scan runs once immediately,
no input needed:

.. code-block:: console

   === I2C Bus Scanner (SR110) ===

   Scanning I2C0...
        0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
   00:             --  --  --  --  --  --  --  --  --
   10: --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
   20: --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
   30: --  --  --  --  --  --  --  --  --  --  --  --  --  3d  --  --
   ...
   Scan complete on I2C0: 1 device(s) found

   Scanning I2C1...
        0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
   00:             --  --  --  --  --  --  --  --  --
   10: --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
   20: 20  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --
   30: --  --  32  --  --  --  --  --  --  39  --  --  --  --  --  --
   40: --  --  --  --  --  --  --  --  --  --  --  --  4c  --  --  --
   ...
   Scan complete on I2C1: 4 device(s) found
