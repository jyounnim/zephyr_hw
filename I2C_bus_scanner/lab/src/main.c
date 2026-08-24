/*
 * I2C Bus Scanner (Zephyr / syna_zephyr_sdk, SR110)
 *
 * Runs a one-shot scan of I2C0 then I2C1 in a dedicated thread at
 * boot and prints an `i2cdetect`-style grid for each. Useful for
 * checking what address a newly-attached sensor or module shows up
 * at.
 *
 * Key devicetree facts (sr100_m55.dtsi / sr100_rdk_m55.dts / sr100_pinctrl.dtsi):
 *   - Node labels &i2c0 / &i2c1 are the SoC's I2C controllers.
 *   - I2C1 is already enabled and pinctrl'd in the board .dts, with a
 *     PCA6416A GPIO expander on it — no overlay needed for I2C1.
 *   - I2C0 ships "status = disabled" with no pinctrl at the SoC
 *     level. The overlay in boards/sr100_rdk_sr100_m55.overlay
 *     enables it via pin groups i2c0_ms_scl / i2c0_ms_sda. The build
 *     must include that overlay or DEVICE_DT_GET(I2C0_NODE) fails to
 *     link.
 *
 * Probe method: a 1-byte i2c_read() per address is what reliably
 * detects devices on this board's DesignWare I2C driver port
 * (confirmed against a known-good reference sample).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdint.h>
#include <stdbool.h>

#define I2C0_NODE DT_NODELABEL(i2c0)
#define I2C1_NODE DT_NODELABEL(i2c1)

#define I2C_SCAN_ADDR_MIN 0x08U   /* addresses below this are reserved */
#define I2C_SCAN_ADDR_MAX 0x77U   /* addresses above this are reserved */

#define SCAN_THREAD_STACK_SIZE 2048
#define SCAN_THREAD_PRIORITY   5

/* Probe a single 7-bit address with a 1-byte read. */
static bool i2c_probe_addr(const struct device *bus, uint8_t addr)
{
	uint8_t byte;
	int ret = i2c_read(bus, &byte, 1, addr);

	return (ret == 0);
}

static void scan_bus(const struct device *bus, const char *bus_name)
{
	if (bus == NULL || !device_is_ready(bus)) {
		printk("[%s] device not ready - check devicetree status/overlay\n",
		       bus_name);
		return;
	}

	printk("\nScanning %s...\n", bus_name);
	printk("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

	int found = 0;

	for (uint8_t row = 0; row <= 0x70; row += 0x10) {
		printk("%02x: ", row);

		for (uint8_t col = 0; col < 16; col++) {
			uint8_t addr = row + col;

			if (addr < I2C_SCAN_ADDR_MIN || addr > I2C_SCAN_ADDR_MAX) {
				printk("   ");
				continue;
			}

			if (i2c_probe_addr(bus, addr)) {
				printk("%02x ", addr);
				found++;
			} else {
				printk("-- ");
			}
		}
		printk("\n");
	}

	printk("Scan complete on %s: %d device(s) found\n", bus_name, found);
}

/* One-shot scan thread: runs once at boot, then exits. */
static void scan_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct device *i2c0 = DEVICE_DT_GET(I2C0_NODE);
	const struct device *i2c1 = DEVICE_DT_GET(I2C1_NODE);

	printk("\n=== I2C Bus Scanner (SR110) ===\n");
	scan_bus(i2c0, "I2C0");
	scan_bus(i2c1, "I2C1");
}

K_THREAD_DEFINE(scan_tid, SCAN_THREAD_STACK_SIZE, scan_thread_entry,
		 NULL, NULL, NULL, SCAN_THREAD_PRIORITY, 0, 0);

int main(void)
{
	/* All work happens in scan_tid, defined above via K_THREAD_DEFINE.
	 * Nothing left for the main thread to do.
	 */
	return 0;
}
