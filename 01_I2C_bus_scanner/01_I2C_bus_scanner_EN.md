# 1. I2C Bus Scanner — Zephyr (ESP32-S3)

An example that scans I2C0 once from a dedicated thread at boot, finds
every device that answers with an ACK, and prints the result as an
`i2cdetect`-style grid. Use it to check which address a new sensor or
display module shows up at once it's wired to the board. It's reused
throughout the `Zephyr_display` project - in Lab 2 (OLED over I2C) and
later in the SHARP/Nokia/ST7735 labs - as a quick wiring check.

## File Layout

```
Zephyr_display/
└── 01_I2C_bus_scanner/
    ├── lab/
    │   ├── src/
    │   │   └── main.c              # scanner logic (comments in English)
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay   # overlay enabling I2C0
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 01_I2C_bus_scanner_EN.md     # this document
```

## I2C Bus Concepts

| Concept | Description |
|---|---|
| Shared bus | Multiple devices hang in parallel off the same two lines, SDA/SCL |
| Address | Each device has a 7-bit address (in the 0x08-0x77 range) - this is what makes scanning possible |
| ACK/NACK | When the master sends an address, only the device using that address answers with an ACK; if nobody's there, it's NACKed |
| Scanning principle | There's no standard "who's at this address?" command, but sweeping through **a zero-length transaction that only sends the address and nothing else**, and checking for an ACK, reveals whether any device is present at that address |

## Wiring

I2C's two signal lines are named **SDA** (Serial **DA**ta) and **SCL** (Serial **CL**ock).

> ⚠️ **SCK/SCLK are SPI terms.** I2C's clock line is always called **SCL** - easy to confuse since some module silkscreens print "SCK" or "CLK" instead, but on an I2C module (usually 4 pins: VCC/GND/SDA/SCL), that pin is SCL.

| Signal | Role | ESP32-S3 connection (per this lab's overlay) |
|---|---|---|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| **SDA** | Data | **GPIO8** |
| **SCL** | Clock (SPI's SCK equivalent) | **GPIO9** |

These two pins (GPIO8/9) match the ESP32-S3 Arduino framework's default I2C pins (`Wire.begin()`'s defaults) - the same pins used in the non-OS curriculum, so if you've already wired that up, it can be reused as-is. Changing the overlay's `pinmux` value moves it to different pins.

```dts
pinmux = <I2C0_SDA_GPIO8>, <I2C0_SCL_GPIO9>;
```

## How It Works

1. A dedicated thread (`scan_tid`) defined with `K_THREAD_DEFINE` starts automatically at boot and runs the I2C0 scan (`main()` does nothing and returns immediately)
2. For each address (0x08-0x77), attempts **a zero-length write** and treats the ACK as evidence the device is present
3. After the scan finishes, prints how many devices were found and the address map
4. Scans once and the thread exits (no repeat)

## Probing Method — Why a Zero-Length Write Instead of a Read

The first version tried a **1-byte read** at each address, but that occasionally **missed devices that were actually present** on real hardware. Tracking it down turned up two causes.

1. **Some I2C devices don't respond meaningfully to a read unless a register address has been written first** - for a device whose "current pointer" depends on prior state, the read result can vary from run to run.
2. **Zephyr GitHub issue #45008** ("esp32: i2c_read() error was returned successfully at the bus nack") reports exactly this problem - ESP32-family I2C drivers can't fully be trusted to detect a NACK correctly in `i2c_read()`.

**Fix**: switched to probing with **a zero-length write**, matching Zephyr's own official sample (`samples/drivers/i2c/i2c_scanner`). This only checks "was the address byte ACKed" and requires nothing further, making it far more reliable regardless of a device's internal state.

```c
static bool i2c_probe_addr(const struct device *bus, uint8_t addr)
{
    int ret = i2c_write(bus, NULL, 0, addr);
    return (ret == 0);
}
```

## Devicetree — Enabling I2C0

ESP32-S3 boards often ship I2C0 with `status = "disabled"` by default, so it needs to be turned on via an overlay.

```dts
&pinctrl {
    i2c0_default: i2c0_default {
        group1 {
            pinmux = <I2C0_SDA_GPIO8>, <I2C0_SCL_GPIO9>;
            bias-pull-up;
            drive-open-drain;
            output-high;
        };
    };
};

&i2c0 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_STANDARD>;
    pinctrl-0 = <&i2c0_default>;
    pinctrl-names = "default";
};
```

> ⚠️ **Worth checking — correction**: `esp32s3_devkitc` **already has** its own `i2c0_default` pinctrl node (the board default is SDA=GPIO1, SCL=GPIO2, with only `status` set to disabled). This document originally said "if that's the case, drop the `&pinctrl` block above and keep only the `&i2c0` block" - that guidance was inaccurate.
>
> The correct move is actually **to keep the `&pinctrl` block above as-is**. This overlay redeclares a node with the **exact same name** (`i2c0_default`) that the board already defines, and under Zephyr's devicetree merge rules, a property declared later overrides an earlier value for the same node - so keeping this block is what actually rewires I2C0 to this lab's intended GPIO8/GPIO9, instead of leaving it on the board's default GPIO1/GPIO2. Conversely, dropping this block and keeping only `&i2c0` silently leaves I2C0 on the board's default GPIO1/GPIO2 - which then mismatches the wiring table above (GPIO8/9) and can make the scan find nothing (this exact GPIO8/9-vs-GPIO1/2 mix-up has caused real trouble in another lab in this series).
>
> After `west build -t devicetree`, it's worth checking the generated `build/zephyr/zephyr.dts` to confirm the final `i2c0_default` node's `pinmux` value actually resolved to GPIO8/GPIO9.

## Build

```powershell
west build -p always -b esp32s3_devkitc/esp32s3/procpu .\01_I2C_bus_scanner\lab\
west flash
west espressif monitor
```

### Expected Output

```
=== I2C Bus Scanner (ESP32-S3) ===

Scanning I2C0...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- 3c -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
Scan complete on I2C0: 1 device(s) found
```

(The example above is with a single SSD1306 OLED wired at 0x3C - Lab 2 is where you'll actually get into this state.)

## Things to Notice

- This scanner is a **diagnostic tool meant to be reused across the whole `Zephyr_display` project** - whenever a later lab hits a "device not responding" issue, make it a habit to run this scanner first and confirm the address actually shows up.
- Switching from read-probing to write-probing is a good example of "**if an implementation differs from the official sample, there's usually a reason**" - when something behaves oddly, comparing your implementation against the official sample/docs to see exactly where they diverge is a solid debugging habit.
- An unexpected address in the scan result is itself useful information - it could mean a wiring mistake (a different device got connected), or a module using an address different from what you expected (some modules change address via a pin jumper).

## Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| `[I2C0] device not ready` | The overlay isn't actually being applied - check that the overlay filename matches the board target, or that it's specified via `-DEXTRA_DTC_OVERLAY_FILE` |
| No address shows up at all | Check the SDA/SCL wiring, check the pull-up resistors, confirm the `pinmux` value matches the pins actually wired. Also see the "Worth checking — correction" box above (make sure it hasn't silently fallen back to the board's default GPIO1/2) |
| One specific device intermittently drops out | If this is still happening, double-check `i2c_probe_addr` is really using a zero-length write - if it's reverted to the read-based approach, this problem can resurface |
| Build error (can't find `I2C0_SDA_GPIO8` etc.) | The ESP32 pinctrl header isn't included - the board's default overlay/dtsi usually handles this, so the symbol name might differ. Use `west build -t devicetree` to see which pinmux macros are actually available |

## Next

Lab 2 (`02_OLED_SSD1306_I2C`) puts a real OLED display on top of the wiring this scanner confirmed.
