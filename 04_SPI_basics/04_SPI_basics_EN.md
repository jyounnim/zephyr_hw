# 3. SPI Basics — Why There's No "SPI Scanner"

> **Review note**: the original file was uploaded as `04_SPI_basics_KR.md`, but the body of the document (the "3." in the title, the `03_SPI_basics/` folder tree, section 8's "I2C scanner (Lab 1) vs. this loopback test (Lab 3)", and the closing section's "Lab 4") all consistently point to this being **Lab 3**. Renamed to `03_SPI_basics_*.md` to match, which also lines up with the Lab 01 (I2C scanner) → Lab 02 (I2C LCD) → this lab (Lab 03) sequence.
>
> Also filled in `CMakeLists.txt` and `sample.yaml`, which were listed in the folder tree but had no content shown in the original document. The code, overlay, and custom binding were cross-checked against official Zephyr sources and left unchanged. See "Review summary" at the bottom for details.

## What this lab covers

The original plan included an "SPI scanner," but on review, **SPI is structurally incapable of I2C-style scanning**. This lab explains why, and builds a **loopback self-test** instead - one that verifies the SPI bus itself is working correctly.

## I2C vs. SPI: a fundamental difference

| | I2C | SPI |
|---|---|---|
| Signal lines | 2 shared lines (SDA, SCL) | At least 3-4 (SCK, MOSI, MISO, CS) |
| How devices are told apart | **Address** (7-bit) - can be swept in software | **CS (Chip Select) pin** - determined by which pin is asserted, only known from the schematic |
| "Who's out there?" | Possible (ACK/NACK on an address) | **Not possible** - the protocol itself has no concept of addressing |
| Connecting multiple devices | Just wire them to the same two lines | SCK/MOSI/MISO can be shared, but each device needs **its own CS line** |

**The key point**: I2C scanning works because the protocol has a built-in mechanism - "ask at an address, and only the device using that address answers." SPI has no such mechanism to begin with: the instant CS is asserted, whatever's wired to that line simply knows "I've been selected" - there's no step in the protocol where a device first announces who it is. So there is no general-purpose way to ask, in code, "what's connected to this bus?"

## So how do you verify SPI wiring is correct?

Split it into two separate questions.

1. **Is the bus itself (SCK/MOSI/MISO wiring, SPI peripheral configuration) working?** This can be verified generically. The **loopback test** covered in this lab answers this question.
2. **Is there actual communication with a specific chip?** This can only be verified by knowing that chip's own commands (e.g. reading a SPI flash chip's JEDEC ID, or a specific display's init sequence). Labs 5-7, where we talk to a real display, are this step.

## What a loopback test is

If you physically jumper MOSI (what the master sends) to MISO (what the master receives), or - even simpler - share the same GPIO pin between them at the pinmux level, whatever you send comes right back. If the data matches exactly, that confirms "at minimum, the SPI peripheral, clock, and pin routing are all working."

This lab uses the **no-jumper-wire** version: the overlay maps MISO and MOSI onto the same GPIO (GPIO8). The ESP32-S3's GPIO matrix lets input routing and output routing be configured independently per pin, so the same physical pad can act as "SPI2 output (MOSI)" and "SPI2 input (MISO)" at the same time - whatever the master drives out on MOSI is read straight back on MISO on that same pin.

## Requirements

- No extra hardware needed (a pure software/pinmux loopback)

## File Layout

```
Zephyr_display/
└── 03_SPI_basics/
    ├── lab/
    │   ├── src/
    │   │   └── main.c
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    │   ├── dts/
    │   │   └── bindings/
    │   │       └── spi/
    │   │           └── zds,spi-loopback.yaml
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 03_SPI_basics_EN.md
```

## Devicetree Overlay

```dts
&pinctrl {
    spim2_loopback: spim2_loopback {
        group1 {
            pinmux = <SPIM2_MISO_GPIO8>;
            output-enable;
        };
        group2 {
            pinmux = <SPIM2_MOSI_GPIO8>;   /* same GPIO8 as MISO - this is the trick */
            input-enable;
        };
        group3 {
            pinmux = <SPIM2_SCLK_GPIO12>, <SPIM2_CSEL_GPIO10>;
        };
    };
};

&spi2 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
    pinctrl-0 = <&spim2_loopback>;
    pinctrl-names = "default";

    loopback_dev: loopback@0 {
        compatible = "zds,spi-loopback";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

Notice that MOSI and MISO are mapped to the **same physical pin (GPIO8)**, one set `output-enable` and the other `input-enable` - that's the whole trick behind a jumper-free loopback. SCK/CS use GPIO12/GPIO10 respectively (these are arbitrary free pins, not pins reserved for anything else on this board - worth double-checking against your board revision's silkscreen if you repurpose them).

## Custom Devicetree Binding

Since we need a placeholder "bus test slot" rather than a real chip, a minimal binding is enough.

```yaml
description: Generic placeholder SPI device node, used for a bus-level loopback self-test

compatible: "zds,spi-loopback"

include: spi-device.yaml
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20.0)

# The custom "zds,spi-loopback" binding lives under this app's own
# dts/bindings/, so DTS_ROOT must be extended BEFORE find_package(Zephyr...)
# runs. Get the order wrong and the devicetree compiler can't find the
# binding, failing the build with "'zds,spi-loopback' compatible not found".
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spi_basics_lab)

target_sources(app PRIVATE src/main.c)
```

## prj.conf

```
CONFIG_SPI=y
CONFIG_GPIO=y
```

No need to turn on the ESP32 SPI driver (`CONFIG_ESP32_SPIM`) separately - Kconfig defines it to auto-enable whenever the devicetree has an `espressif,esp32-spi` node with `status = "okay"`. (This differs from the I2C lab, where `CONFIG_I2C_ESP32=y` had to be set explicitly - worth keeping in mind.)

## sample.yaml

```yaml
sample:
  name: SPI basics - bus loopback self-test
  description: >
    Verify the SPI2 (GPSPI2) peripheral, clock, and pin routing on the
    ESP32-S3-DevKitC-1 using a devicetree-level loopback (MISO and MOSI
    mapped to the same GPIO pad) - no jumper wire and no real SPI device
    required.
common:
  tags:
    - spi
  platform_allow:
    - esp32s3_devkitc/esp32s3/procpu
  harness: console
  harness_config:
    type: one_line
    regex:
      - "PASS: received bytes match sent bytes.*"
tests:
  sample.spi.esp32s3_loopback:
    build_only: true
```

## Code

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <string.h>

#define LOOPBACK_NODE DT_NODELABEL(loopback_dev)

static const struct spi_dt_spec loopback_spi = SPI_DT_SPEC_GET(
    LOOPBACK_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER, 0);

static bool spi_loopback_test(void) {
    uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0xAA, 0x55, 0xFF, 0x00};
    uint8_t rx_data[8] = {0};

    struct spi_buf tx_buf = { .buf = tx_data, .len = sizeof(tx_data) };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

    struct spi_buf rx_buf = { .buf = rx_data, .len = sizeof(rx_data) };
    struct spi_buf_set rx_bufs = { .buffers = &rx_buf, .count = 1 };

    int ret = spi_transceive_dt(&loopback_spi, &tx_bufs, &rx_bufs);
    if (ret != 0) {
        printk("spi_transceive_dt failed: %d\n", ret);
        return false;
    }

    printk("Sent:     ");
    for (int i = 0; i < (int)sizeof(tx_data); i++) printk("%02X ", tx_data[i]);
    printk("\n");

    printk("Received: ");
    for (int i = 0; i < (int)sizeof(rx_data); i++) printk("%02X ", rx_data[i]);
    printk("\n");

    return memcmp(tx_data, rx_data, sizeof(tx_data)) == 0;
}

#define TEST_STACK_SIZE 2048
#define TEST_PRIORITY   5

static void test_thread_entry(void *p1, void *p2, void *p3) {
    printk("\n=== SPI Basics: bus loopback self-test ===\n");

    if (!spi_is_ready_dt(&loopback_spi)) {
        printk("SPI device not ready - check devicetree status/overlay\n");
        return;
    }

    bool ok = spi_loopback_test();

    if (ok) {
        printk("PASS: received bytes match sent bytes - SPI peripheral, "
               "clock, and pin routing are all working.\n");
    } else {
        printk("FAIL: received bytes do NOT match sent bytes - check "
               "the MISO/MOSI pinmux, or SCLK wiring.\n");
    }
}

K_THREAD_DEFINE(test_tid, TEST_STACK_SIZE, test_thread_entry,
                NULL, NULL, NULL, TEST_PRIORITY, 0, 0);

int main(void) {
    return 0;
}
```

## Build & Run

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 03_SPI_basics/lab
west flash
west espressif monitor
```

(On PowerShell, just swap the path: `west build -p always -b esp32s3_devkitc/esp32s3/procpu .\03_SPI_basics\lab\`.)

### Expected output

```
=== SPI Basics: bus loopback self-test ===
Sent:     01 02 03 04 AA 55 FF 00
Received: 01 02 03 04 AA 55 FF 00
PASS: received bytes match sent bytes - SPI peripheral, clock, and pin routing are all working.
```

Confirm that `Sent` and `Received` match exactly.

## Things to notice

- Passing this test does not mean "any SPI device will now just work" - it only confirms **the bus itself (electrical signals, peripheral configuration) is sound**. Talking to a real device still requires implementing that device's own protocol correctly (which is exactly what labs 5-7 do).
- Placing the I2C scanner (Lab 1) side by side with this loopback test (Lab 3) makes it concrete that **"these two protocols look superficially similar (clock + data), but their design philosophies are completely different"** - I2C is optimized for "discovering multiple devices on a bus," while SPI is optimized for "talking fast to a device you already know is there."
- Adding multiple CS lines (an array of GPIOs in the overlay's `cs-gpios`) lets you connect several SPI devices while sharing the same SCK/MOSI/MISO - use this approach in a later lab if you want to drive multiple displays at once.

## Troubleshooting

| Symptom | Cause / Fix |
|---|---|
| `SPI device not ready` | The overlay isn't actually being applied - check that the filename matches the board target |
| `spi_transceive_dt failed` | A problem with the SPI bus configuration itself - run `west build -t devicetree` to confirm the `&spi2` node merged correctly |
| Sent and Received don't match | Double-check the overlay actually maps MISO/MOSI to the same GPIO - if they're on different pins, there's no loopback |
| Devicetree binding not found (`'zds,spi-loopback' compatible not found`) | Check that `CMakeLists.txt`'s `list(APPEND DTS_ROOT ...)` comes before `find_package(Zephyr...)` |
| Build error about an undeclared macro like `SPIM2_MISO_GPIO8` | See "Review summary" below - the macro naming follows the expected convention, but this hasn't been confirmed on an actual build |

## Next

Lab 4 (`04_OLED_SSD1306_SPI`) connects a real OLED (SSD1306 in SPI mode) on top of the SPI bus verified here.

---

## Review summary

Before delivering this document, the following was cross-checked against official Zephyr sources:

- **`&spi2` label and `compatible`**: confirmed the ESP32-S3 SoC devicetree defines both `spi2` (base address 0x60024000, GPSPI2) and `spi3` (0x60025000, GPSPI3) nodes with `compatible = "espressif,esp32-spi"` and `status = "disabled"` by default. Turning it on via `status = "okay"` in the overlay is the same pattern used in the I2C0 lab.
- **`CONFIG_ESP32_SPIM` auto-enable**: confirmed in Zephyr's `drivers/spi/Kconfig.esp32` that `ESP32_SPIM` is defined as `depends on DT_HAS_ESPRESSIF_ESP32_SPI_ENABLED` with `default y`. In other words, enabling the SPI node in the devicetree is enough to auto-enable the driver, so `CONFIG_SPI=y` alone in prj.conf is sufficient (unlike the I2C lab, which needed an explicit `CONFIG_xxx_ESP32=y`).
- **Pinmux macro naming convention**: confirmed that `esp32s3-pinctrl.h` mechanically generates I2S-related signal macros in the form `<PERIPHERAL>_<SIGNAL>_GPIO<N>` (e.g. `I2S1_MCLK_GPIO8`) for every GPIO number. `SPIM2_MISO_GPIO8`-style macros are very likely generated the same way, but the file was too large to directly confirm the SPI section verbatim. **Worth a quick check on your first build** - if a macro name is wrong, the devicetree compilation stage fails immediately with an "undeclared" error, which is much faster to diagnose than a hardware issue.
- **The "map MOSI/MISO to the same GPIO" technique itself**: structurally sound, since the ESP32/ESP32-S3 GPIO matrix does let input and output signal routing be configured independently per pin. However, whether Zephyr's official `tests/drivers/spi/spi_loopback` repository actually uses this exact technique for ESP32-family boards could not be directly confirmed due to repository browsing limits - treat the original document's claim that "this is what Zephyr's own official test does" as unverified, and judge this lab's soundness on the GPIO-matrix architecture reasoning above instead.
