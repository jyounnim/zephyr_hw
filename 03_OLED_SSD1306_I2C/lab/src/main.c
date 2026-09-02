/*
 * Lab 03: SSD1306 0.96" OLED, I2C mode, with runtime address
 * auto-detection - raw I2C, no Zephyr Display/CFB subsystem.
 *
 * Board: ESP32-S3-DevKitC-1 (esp32s3_devkitc/esp32s3/procpu)
 * Bus:   I2C0, SDA = GPIO8, SCL = GPIO9 (series-wide GPIO8/9 pin,
 *        see the overlay and Lab 01's doc for why this overrides the
 *        board default GPIO1/GPIO2).
 *
 * THIS IS A FRESH REWRITE, not the earlier parked I2C SSD1306 attempt
 * (that one is still recorded separately in the roadmap as "parked").
 * This version is based on a confirmed-working reference implementation
 * built for a different board (Synaptics SR110) that reached a
 * successful "Hello World!" on real SSD1306 I2C hardware, then adapted
 * here for the ESP32-S3. Three things carried over unchanged because
 * they fix real, previously-reproduced bugs rather than being
 * platform-specific quirks:
 *
 *   1. The whole framebuffer is sent as ONE i2c_write() over a single
 *      contiguous buffer (control byte + 1024 pixel bytes), instead of
 *      a two-message i2c_transfer() (control byte message, then
 *      payload message). On the reference platform, two-message
 *      transfers produced a screen full of noise - the working theory
 *      is that some I2C controllers insert an unwanted STOP/restart
 *      between the two messages, so the SSD1306 sees a lone control
 *      byte, then a second transaction whose first byte gets misread as
 *      a new control byte instead of pixel data, shifting everything
 *      after it by one and effectively randomizing which bytes are
 *      commands vs. data. A single contiguous write sidesteps the
 *      question entirely for any I2C driver, so it is kept here too as
 *      the safer default even though it hasn't specifically been shown
 *      to matter on ESP32-S3's I2C driver.
 *   2. A boot settle delay (100 ms) plus a few retries per candidate
 *      address before giving up on it. SSD1306 modules need a moment
 *      after power-up before they reliably ACK on the bus; probing
 *      immediately after i2c0 becomes ready can spuriously report "not
 *      found" on a module that would otherwise scan fine a beat later.
 *   3. The overall structure: scan first (0x3C then 0x3D), remember
 *      whichever address responds, then use that address for every
 *      subsequent command/data write - since a devicetree node's `reg`
 *      is fixed at build time and can't do this, there is no child
 *      node for the OLED here; every I2C transaction is issued
 *      directly from this file.
 *
 * One thing was deliberately NOT carried over: the reference probed
 * with a 1-byte i2c_read() because write-based probing was shown to
 * miss real devices on that platform's I2C driver. This project's own
 * Lab 01 (I2C bus scanner) and Lab 02 (I2C LCD) already established
 * write-based probing (a 1-byte i2c_write() with no payload) as
 * reliable on the ESP32-S3's I2C driver, so this lab keeps that
 * convention instead of switching styles.
 *
 * HARDWARE RESET (RES) PIN: some SSD1306 breakout modules expose a
 * RES/RST pin that needs a low-then-high pulse before I2C commands are
 * sent, or the panel can show scattered dot noise indefinitely even
 * though the software init sequence completes without error. Most
 * low-cost 4-pin (VCC/GND/SDA/SCL) modules - the common case - have no
 * such pin at all and don't need any of this, so OLED_USE_HW_RESET
 * defaults to 0 here.
 *
 * If your module DOES have an RST pin, there are two ways to drive it:
 *
 *   A. (RECOMMENDED, confirmed on real hardware) Wire the module's RST
 *      straight to the dev board's own RST/EN pin instead of a GPIO.
 *      The OLED then resets physically in lockstep with the board on
 *      every power-up/reset/reflash - no code involved at all, keep
 *      OLED_USE_HW_RESET at 0 and leave oled_hw_reset() unused. This
 *      also costs zero extra GPIOs.
 *   B. (fallback, NOT yet validated on ESP32-S3) Wire RST to the GPIO
 *      below and flip OLED_USE_HW_RESET to 1, for cases where the
 *      board's RST/EN pin isn't available to wire to (e.g. already
 *      committed to something else). Double-check the chosen GPIO
 *      isn't in use elsewhere on your wiring before relying on it.
 *
 * PIN SAFETY NOTE: don't assume a GPIO is free just because nothing in
 * this lab uses it. On the ESP32-S3-DevKitC-1, avoid the strapping
 * pins (GPIO0, GPIO3, GPIO45, GPIO46) and the USB-JTAG pins (GPIO19/
 * GPIO20) for anything you toggle in software - a lesson learned the
 * hard way on a different board in this series when a pin that looked
 * unrelated by schematic net name turned out to double as a debug-
 * module signal and locked up the board on first toggle. GPIO4 is used
 * below because it matches the RST pin already used by Lab 07 in this
 * series and has no such conflict on this board.
 */

#define OLED_USE_HW_RESET 0 /* set to 1 only if your module has an RST
			      * pin wired to GPIO4 below (see the file
			      * header comment) */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>
#include <string.h>

#define I2C0_NODE DT_NODELABEL(i2c0)

/* Hardware reset (RES) pin, only used when OLED_USE_HW_RESET is 1.
 * &gpio0 covers SoC pins 0-31 (see the roadmap's GPIO-controller note).
 */
#define OLED_RST_GPIO_NODE DT_NODELABEL(gpio0)
#define OLED_RST_PIN       4

#define LCD_WIDTH  128
#define LCD_HEIGHT 64
#define LCD_PAGES  (LCD_HEIGHT / 8) /* 8 pages of 8 vertical pixels each */

/* The only two addresses an SSD1306 module can be strapped to. */
static const uint8_t oled_addr_candidates[] = { 0x3C, 0x3D };

/* SSD1306 I2C control bytes (sent as the first byte of every transaction). */
#define SSD1306_CTRL_CMD  0x00
#define SSD1306_CTRL_DATA 0x40

static uint8_t framebuffer[LCD_WIDTH * LCD_PAGES];

/* Minimal 5x7 font - only the glyphs this lab actually prints
 * ("Hello World!" and "Addr 0x3C"/"Addr 0x3D"). Column-major,
 * bottom-to-top bit convention, same as the other raw-framebuffer labs
 * in this series (Lab 07/08) for consistency.
 */
struct glyph {
	char ch;
	uint8_t cols[5];
};

static const struct glyph font5x7[] = {
	{' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
	{'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
	{'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
	{'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
	{'A', {0x7C, 0x12, 0x11, 0x12, 0x7C}},
	{'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
	{'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
	{'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
	{'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
	{'d', {0x38, 0x44, 0x44, 0x48, 0x7F}},
	{'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
	{'l', {0x00, 0x41, 0x7F, 0x40, 0x00}},
	{'o', {0x38, 0x44, 0x44, 0x44, 0x38}},
	{'r', {0x7C, 0x08, 0x04, 0x04, 0x08}},
	{'x', {0x22, 0x14, 0x08, 0x14, 0x22}},
};

static const uint8_t *glyph_lookup(char c)
{
	static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

	for (size_t i = 0; i < ARRAY_SIZE(font5x7); i++) {
		if (font5x7[i].ch == c) {
			return font5x7[i].cols;
		}
	}
	return blank;
}

#if OLED_USE_HW_RESET
/* Pulses the RES pin low then high (see the file header comment for
 * why this may be required, and the pin-safety note about picking a
 * pin carefully). Uses gpio_pin_set_raw() rather than the
 * ACTIVE_LOW-aware gpio_pin_set(), since this pin isn't described via
 * a devicetree gpio-spec here - "raw" just means the values below are
 * the literal physical level.
 */
static int oled_hw_reset(void)
{
	const struct device *gpio0 = DEVICE_DT_GET(OLED_RST_GPIO_NODE);
	int ret;

	if (!device_is_ready(gpio0)) {
		printk("RST GPIO controller (gpio0) not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure(gpio0, OLED_RST_PIN, GPIO_OUTPUT_HIGH);
	if (ret) {
		return ret;
	}

	k_sleep(K_MSEC(10));                       /* settle, not in reset */
	gpio_pin_set_raw(gpio0, OLED_RST_PIN, 0);  /* assert reset (low) */
	k_sleep(K_MSEC(10));
	gpio_pin_set_raw(gpio0, OLED_RST_PIN, 1);  /* release reset (high) */
	k_sleep(K_MSEC(10));                       /* let the panel come back up */

	return 0;
}
#endif /* OLED_USE_HW_RESET */

/* Probe a single 7-bit address with a 1-byte write and no payload -
 * same style already confirmed working on ESP32-S3's I2C driver in
 * Lab 01 (bus scanner) and Lab 02 (I2C LCD).
 */
static bool i2c_probe_addr(const struct device *bus, uint8_t addr)
{
	uint8_t dummy = 0;
	int ret = i2c_write(bus, &dummy, 1, addr);

	return (ret == 0);
}

/* Scan the two possible SSD1306 addresses and report which one (if
 * any) responds. Returns true and fills *addr_out on success.
 *
 * Each candidate gets a few retries with a short gap in between,
 * rather than a single probe - a single miss right after power-on
 * doesn't necessarily mean "not present", and this costs at most a
 * few tens of milliseconds in the failure case.
 */
#define SSD1306_PROBE_RETRIES     3
#define SSD1306_PROBE_RETRY_DELAY K_MSEC(20)

static bool ssd1306_find_address(const struct device *bus, uint8_t *addr_out)
{
	printk("Scanning for SSD1306 at 0x3C / 0x3D...\n");
	for (size_t i = 0; i < ARRAY_SIZE(oled_addr_candidates); i++) {
		uint8_t addr = oled_addr_candidates[i];

		for (int attempt = 0; attempt < SSD1306_PROBE_RETRIES; attempt++) {
			if (i2c_probe_addr(bus, addr)) {
				printk("  found device at 0x%02X\n", addr);
				*addr_out = addr;
				return true;
			}
			k_sleep(SSD1306_PROBE_RETRY_DELAY);
		}
	}
	return false;
}

static int ssd1306_cmd(const struct device *bus, uint8_t addr, uint8_t cmd)
{
	uint8_t buf[2] = { SSD1306_CTRL_CMD, cmd };

	return i2c_write(bus, buf, sizeof(buf), addr);
}

/* Sends the control byte and the framebuffer as ONE I2C transaction -
 * see the file header comment for why a two-message i2c_transfer()
 * is deliberately avoided here.
 */
static uint8_t oled_data_tx_buf[1 + LCD_WIDTH * LCD_PAGES];

static int ssd1306_data(const struct device *bus, uint8_t addr,
			 const uint8_t *data, size_t len)
{
	oled_data_tx_buf[0] = SSD1306_CTRL_DATA;
	memcpy(&oled_data_tx_buf[1], data, len);

	return i2c_write(bus, oled_data_tx_buf, len + 1, addr);
}

/* Small helper so the (fairly long) init sequence below reads as a
 * flat list of commands instead of 16 repeated "if (ret) return ret;"
 * blocks. `bus`, `addr`, and `ret` are expected to be in scope.
 *
 * On failure this prints exactly which command byte failed and with
 * what error code - needed to tell apart "the whole bus is unhappy"
 * from "this one specific command NACKs" while debugging on real
 * hardware, since a bare "init failed" doesn't distinguish those.
 *
 * The 1ms gap after every command is deliberately generous. It is not
 * required by the SSD1306 datasheet (which only specifies a bus-free
 * time in the microsecond range), but costs nothing during init and
 * rules out "commands sent faster than the controller can latch them"
 * as a variable while debugging a real I2C write failure.
 */
#define SSD1306_CMD(c) do { \
		ret = ssd1306_cmd(bus, addr, (c)); \
		if (ret) { \
			printk("  ssd1306 cmd 0x%02X failed, ret=%d\n", (c), ret); \
			return ret; \
		} \
		k_sleep(K_MSEC(1)); \
	} while (0)

static int ssd1306_init(const struct device *bus, uint8_t addr)
{
	int ret;

	SSD1306_CMD(0xAE);       /* Display off */
	SSD1306_CMD(0xD5);       /* Set display clock divide ratio/osc freq */
	SSD1306_CMD(0x80);
	SSD1306_CMD(0xA8);       /* Set multiplex ratio */
	SSD1306_CMD(LCD_HEIGHT - 1);
	SSD1306_CMD(0xD3);       /* Set display offset */
	SSD1306_CMD(0x00);
	SSD1306_CMD(0x40);       /* Set display start line = 0 */
	SSD1306_CMD(0x8D);       /* Charge pump: enable (no external Vcc) */
	SSD1306_CMD(0x14);
	SSD1306_CMD(0x20);       /* Memory addressing mode: horizontal */
	SSD1306_CMD(0x00);
	SSD1306_CMD(0xA1);       /* Segment remap (column 127 = SEG0) */
	SSD1306_CMD(0xC8);       /* COM output scan direction: remapped */
	SSD1306_CMD(0xDA);       /* COM pins hardware configuration */
	SSD1306_CMD(0x12);
	SSD1306_CMD(0x81);       /* Contrast control */
	SSD1306_CMD(0xCF);
	SSD1306_CMD(0xD9);       /* Pre-charge period */
	SSD1306_CMD(0xF1);
	SSD1306_CMD(0xDB);       /* VCOMH deselect level */
	SSD1306_CMD(0x40);
	SSD1306_CMD(0xA4);       /* Resume to RAM content display */
	SSD1306_CMD(0xA6);       /* Normal (not inverted) display */
	SSD1306_CMD(0xAF);       /* Display on */

	return 0;
}

static void fb_draw_char(int col, int page, char c)
{
	if (page < 0 || page >= LCD_PAGES) {
		return;
	}
	const uint8_t *glyph = glyph_lookup(c);

	for (int i = 0; i < 5; i++) {
		int x = col + i;

		if (x >= 0 && x < LCD_WIDTH) {
			framebuffer[page * LCD_WIDTH + x] = glyph[i];
		}
	}
	/* 6th column is left as-is (0-initialized) for the letter-spacing gap. */
}

static void fb_draw_string(int col, int page, const char *str)
{
	while (*str) {
		fb_draw_char(col, page, *str++);
		col += 6; /* 5 glyph columns + 1 blank column */
	}
}

/* Pushes the whole framebuffer out in one shot. Re-sending the
 * column/page address window every time (rather than relying on the
 * controller's auto-increment/wrap alone) makes this self-contained -
 * safe to call repeatedly without tracking cursor state elsewhere.
 */
static int ssd1306_update(const struct device *bus, uint8_t addr)
{
	int ret;

	SSD1306_CMD(0x21);            /* Set column address range */
	SSD1306_CMD(0);
	SSD1306_CMD(LCD_WIDTH - 1);
	SSD1306_CMD(0x22);            /* Set page address range */
	SSD1306_CMD(0);
	SSD1306_CMD(LCD_PAGES - 1);

	return ssd1306_data(bus, addr, framebuffer, sizeof(framebuffer));
}

int main(void)
{
	const struct device *i2c0 = DEVICE_DT_GET(I2C0_NODE);
	uint8_t oled_addr;

	printk("\n=== OLED SSD1306 (I2C, ESP32-S3) ===\n");

	if (!device_is_ready(i2c0)) {
		printk("I2C0 device not ready - check devicetree status/overlay\n");
		return 0;
	}

	/* Give the OLED's own power supply/POR circuit time to settle
	 * before probing it. Lab 01's full 0x08-0x77 scan reaches 0x3C/
	 * 0x3D fairly late (after already probing many other addresses),
	 * so it incidentally gets this delay for free. This lab probes
	 * only two addresses right at boot, so without an explicit delay
	 * it could reach the module before it's ready to ACK on the bus.
	 */
	k_sleep(K_MSEC(100));

	if (!ssd1306_find_address(i2c0, &oled_addr)) {
		printk("No SSD1306 found at 0x3C or 0x3D - check wiring/power\n");
		printk("(this series has seen weak/missing pull-ups on this bus\n");
		printk(" cause exactly this - try adding external 4.7k pull-ups\n");
		printk(" on SDA/SCL to 3.3V, and/or an external 3.3V supply for\n");
		printk(" the module, before assuming the module itself is bad)\n");
		return 0;
	}

	/* Brief settle gap between the probe above and the write-heavy
	 * init sequence below - cheap insurance in case the bus/controller
	 * needs a moment between transactions. */
	k_sleep(K_MSEC(10));

#if OLED_USE_HW_RESET
	if (oled_hw_reset()) {
		printk("SSD1306 hardware reset failed - check RST wiring (gpio0 %d)\n",
		       OLED_RST_PIN);
		return 0;
	}
#endif

	if (ssd1306_init(i2c0, oled_addr)) {
		printk("SSD1306 init failed (I2C write error) - check wiring\n");
		return 0;
	}

	memset(framebuffer, 0, sizeof(framebuffer));
	fb_draw_string(0, 0, "Hello World!");
	fb_draw_string(0, 2, oled_addr == 0x3C ? "Addr 0x3C" : "Addr 0x3D");

	if (ssd1306_update(i2c0, oled_addr)) {
		printk("SSD1306 framebuffer update failed (I2C write error)\n");
		return 0;
	}

	printk("SSD1306 initialized at 0x%02X, \"Hello World!\" written\n", oled_addr);

	while (1) {
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
