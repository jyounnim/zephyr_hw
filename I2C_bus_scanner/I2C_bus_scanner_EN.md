# I2C Bus Scanner — Zephyr (SR110)

At boot, a dedicated thread scans I2C0 then I2C1 once, and prints an
`i2cdetect`-style grid for each bus showing which addresses ACKed.
It's used to quickly check what address a newly-attached sensor or
module shows up at.

## Folder layout

Assumes this sits under a `zephyr_hw` directory.

```
<west workspace root>/
└── zephyr_hw/
    └── I2C_bus_scanner/
        ├── lab/
        │   ├── src/
        │   │   └── main.c              # scanner logic
        │   ├── boards/
        │   │   └── sr100_rdk_sr100_m55.overlay   # required overlay to enable I2C0
        │   ├── CMakeLists.txt
        │   ├── prj.conf
        │   ├── README.rst              # Zephyr official sample-format doc
        │   └── sample.yaml             # twister test metadata
        ├── I2C_bus_scanner_KR.md       # Korean version
        └── I2C_bus_scanner_EN.md       # this document
```

## How it works

1. A dedicated thread (`scan_tid`), defined with `K_THREAD_DEFINE`,
   starts automatically at boot and scans I2C0 then I2C1
   (the `main()` thread does nothing and returns immediately)
2. Each address (0x08-0x77) is probed with a 1-byte **read**; success
   (ACK) means a device is present
3. After each scan, the device count and address grid are printed
4. The scan runs once and the thread exits (no repetition)

## Build

Run from the west workspace root (the parent directory of `zephyr_hw`).

The overlay file name (`boards/sr100_rdk_sr100_m55.overlay`) exactly
matches Zephyr's board-target auto-overlay naming convention
(board target with `/` replaced by `_`), so the build system finds
and applies it automatically — no `-DEXTRA_DTC_OVERLAY_FILE` needed.
I2C0's SoC-level devicetree default is `status = "disabled"`, so
without this overlay the build fails to link.
(I2C1 is already enabled in the board dts, so no overlay is needed
for it.)

```powershell
west build -p always -b sr100_rdk/sr100/m55 .\zephyr_hw\I2C_bus_scanner\lab\
```

> Do not additionally pass `-DEXTRA_DTC_OVERLAY_FILE=...` — since the
> file name is already auto-detected, passing it again duplicates the
> reference and can cause devicetree preprocessing to fail during
> CMake argument handling.

### Related devicetree facts

- In `sr100_m55.dtsi:268-277`, I2C0 is `status = "disabled"` with no
  pinctrl assignment
- In `sr100_m55.dtsi:280-...` / `sr100_rdk_m55.dts:141-144`, I2C1 is
  already `status = "okay"` with
  `pinctrl-0 = <&i2c1_ms_scl_b &i2c1_ms_sda_b>`, and already has a
  PCA6416A GPIO expander (`gpio_exp0`) on it
- I2C0's pin groups are defined in `sr100_pinctrl.dtsi:586,603` as
  `i2c0_ms_sda` / `i2c0_ms_scl`, but marked `/omit-if-no-ref/` — they
  drop out of the build entirely unless something references them,
  which is why the overlay is required

## Flash

Move to `srsdk_tools` and use `openocd_flash.py`.

```powershell
cd C:\02.work\syna_zephry\syna_zephry_sdk\srsdk_tools
python openocd_flash.py `
  --openocd C:\02.work\SRSDK_Build_tools\OpenOCD\xpack-openocd-0.12.0-4\bin\openocd.exe `
  --flash-offset 0x0 `
  --file-offset 0x0 `
  --cfg_path Input_Config\sr100_m55.cfg `
  --image Output\B0_Flash\B0_flash_full_image_GD25LE128_67Mhz_secured.bin
```

> The `--image` path should point to the secured image freshly
> generated from this `I2C_bus_scanner` build.

Once flashed, open the console (230400bps 8N1) — the scan runs
immediately with no input needed.

## Sample Output

```
=== I2C Bus Scanner (SR110) ===

Scanning I2C0...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- 3d -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
Scan complete on I2C0: 1 device(s) found
Scanning I2C1...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: 20 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- 32 -- -- -- -- -- -- 39 -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- 4c -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
Scan complete on I2C1: 4 device(s) found
```

## Confirmed items

- [x] `DT_NODELABEL(i2c0)` / `DT_NODELABEL(i2c1)` labels — confirmed against `sr100_m55.dtsi:268,280`
- [x] Default `status` for I2C0/I2C1 — I2C0 is disabled (overlay required), I2C1 is okay (no overlay needed)
- [x] I2C0's pinctrl group names — `i2c0_ms_scl` / `i2c0_ms_sda` (`sr100_pinctrl.dtsi:586,603`)
- [x] Automatic overlay application — the file name matches the board target
      (`sr100_rdk/sr100/m55` → `sr100_rdk_sr100_m55`), so it's auto-detected
      without `-DEXTRA_DTC_OVERLAY_FILE`
- [x] **1-byte `i2c_read`** probe method — confirmed against a
      hardware-verified reference sample (`i2c_test`). `CONFIG_I2C_DW=y`
      also set explicitly
- [x] A dedicated thread via `K_THREAD_DEFINE` scans once at boot and
      exits (`main()` returns immediately with nothing to do)

## TODO / VERIFY (still open)

- [ ] Confirm whether I2C0's 100 kHz clock-frequency (mirrored from
      I2C1's board setting) is appropriate for the device you attach
      (bump to 400000 for fast mode in the overlay if needed)
- [ ] Confirm which physical connector pins `i2c0_ms_scl`/`i2c0_ms_sda`
      map to, using the board schematic/pinout doc (possible pin
      conflicts with other samples)
- [ ] Confirm the exact signing/packaging procedure that produces the
      secured image for `openocd_flash.py`
      (`build\zephyr\zephyr.bin` → `Output\B0_Flash\...` conversion)
