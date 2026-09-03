/*
 * Lab 06: ST7789V3 1.69" 240x280 color TFT, raw SPI, no Zephyr
 * Display/CFB subsystem.
 *
 * Board: ESP32-S3-DevKitC-1 (esp32s3_devkitc/esp32s3/procpu)
 * Bus:   SPI2 (GPSPI2), hardware CS0. SCLK=GPIO14, MOSI=GPIO13,
 *        CS0=GPIO15, RST=GPIO16, DC=GPIO17 - the same SPI2 pin
 *        assignment as this series' ST7735 lab, reused as-is since
 *        both are write-only color SPI TFTs with the same RST/DC
 *        wiring needs.
 *
 * This lab talks to the ST7789V3 controller directly with raw SPI
 * writes (command/data selected via the DC pin), the same style used
 * throughout this series for SPI displays, rather than going through
 * Zephyr's built-in "sitronix,st7789v" display driver.
 *
 * PANEL RAM OFFSET: the ST7789 controller's native GRAM is 240x320.
 * This particular module's visible glass is only 240x280, centered
 * in a sub-window of that GRAM - so every addressing window sent to
 * the controller needs a fixed offset added (X_OFFSET/Y_OFFSET below,
 * also mirrored in the devicetree overlay). 20 is the commonly
 * documented Y offset for 1.69" 240x280 ST7789V3 modules; if the
 * image on your specific module is shifted or clipped, check your
 * module's own datasheet/example code for its offset and update the
 * overlay's x-offset/y-offset properties.
 *
 * COLOR ORDER / ORIENTATION: MADCTL below is set to a common default.
 * If colors come out with red/blue swapped, or the image is mirrored/
 * rotated relative to how your module is mounted, see the
 * troubleshooting doc for the MADCTL bits to flip.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>

#define DISP_NODE DT_NODELABEL(st7789v_disp)

#define PANEL_WIDTH   DT_PROP(DISP_NODE, width)
#define PANEL_HEIGHT  DT_PROP(DISP_NODE, height)
#define X_OFFSET      DT_PROP(DISP_NODE, x_offset)
#define Y_OFFSET      DT_PROP(DISP_NODE, y_offset)

/* ST7789 command set (only what this lab needs). */
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_COLMOD  0x3A
#define ST7789_MADCTL  0x36
#define ST7789_INVON   0x21
#define ST7789_NORON   0x13
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C

/* RGB565 colors used by the demo pattern. */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

static const struct spi_dt_spec spi_spec =
	SPI_DT_SPEC_GET(DISP_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB, 0);
static const struct gpio_dt_spec reset_spec = GPIO_DT_SPEC_GET(DISP_NODE, reset_gpios);
static const struct gpio_dt_spec dc_spec = GPIO_DT_SPEC_GET(DISP_NODE, dc_gpios);

/* Minimal 5x7 font - only the glyphs this lab prints ("Hello World!").
 * Same column-major, bottom-to-top bit convention used by the other
 * raw-framebuffer labs in this series, so the table is copied
 * verbatim for consistency.
 */
struct glyph {
	char ch;
	uint8_t cols[5];
};

static const struct glyph font5x7[] = {
	{' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
	{'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
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

static int st7789_write_cmd(uint8_t cmd)
{
	struct spi_buf buf = { .buf = &cmd, .len = 1 };
	struct spi_buf_set set = { .buffers = &buf, .count = 1 };

	gpio_pin_set_dt(&dc_spec, 0); /* command mode */
	return spi_write_dt(&spi_spec, &set);
}

static int st7789_write_data(const uint8_t *data, size_t len)
{
	struct spi_buf buf = { .buf = (void *)data, .len = len };
	struct spi_buf_set set = { .buffers = &buf, .count = 1 };

	gpio_pin_set_dt(&dc_spec, 1); /* data mode */
	return spi_write_dt(&spi_spec, &set);
}

static int st7789_reset(void)
{
	int ret = gpio_pin_configure_dt(&reset_spec, GPIO_OUTPUT_INACTIVE);

	if (ret) {
		return ret;
	}
	k_sleep(K_MSEC(10));
	gpio_pin_set_dt(&reset_spec, 1); /* assert reset */
	k_sleep(K_MSEC(10));
	gpio_pin_set_dt(&reset_spec, 0); /* release reset */
	k_sleep(K_MSEC(150));            /* datasheet: allow up to ~120ms to recover */

	return 0;
}

/* Standard ST7789 init sequence (16bpp RGB565, display inversion on -
 * required by most ST7789 glass to show correct, non-inverted colors).
 */
static int st7789_init(void)
{
	int ret;
	uint8_t colmod = 0x55;   /* 16 bits/pixel */
	uint8_t madctl = 0x00;   /* orientation/RGB order - see troubleshooting doc */

	ret = st7789_write_cmd(ST7789_SWRESET);
	if (ret) return ret;
	k_sleep(K_MSEC(150));

	ret = st7789_write_cmd(ST7789_SLPOUT);
	if (ret) return ret;
	k_sleep(K_MSEC(255));

	ret = st7789_write_cmd(ST7789_COLMOD);
	if (ret) return ret;
	ret = st7789_write_data(&colmod, 1);
	if (ret) return ret;
	k_sleep(K_MSEC(10));

	ret = st7789_write_cmd(ST7789_MADCTL);
	if (ret) return ret;
	ret = st7789_write_data(&madctl, 1);
	if (ret) return ret;

	ret = st7789_write_cmd(ST7789_INVON);
	if (ret) return ret;
	k_sleep(K_MSEC(10));

	ret = st7789_write_cmd(ST7789_NORON);
	if (ret) return ret;
	k_sleep(K_MSEC(10));

	ret = st7789_write_cmd(ST7789_DISPON);
	if (ret) return ret;
	k_sleep(K_MSEC(100));

	return 0;
}

static int st7789_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint16_t xs = x0 + X_OFFSET, xe = x1 + X_OFFSET;
	uint16_t ys = y0 + Y_OFFSET, ye = y1 + Y_OFFSET;
	uint8_t caset[4] = { xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF };
	uint8_t raset[4] = { ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF };
	int ret;

	ret = st7789_write_cmd(ST7789_CASET);
	if (ret) return ret;
	ret = st7789_write_data(caset, sizeof(caset));
	if (ret) return ret;

	ret = st7789_write_cmd(ST7789_RASET);
	if (ret) return ret;
	ret = st7789_write_data(raset, sizeof(raset));
	if (ret) return ret;

	return st7789_write_cmd(ST7789_RAMWR);
}

/* Fills a rectangle with a solid RGB565 color, one row at a time from
 * a small stack buffer (no full-screen framebuffer is kept in RAM -
 * consistent with this series' raw-SPI, direct-write approach).
 */
static int st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
	uint8_t row[PANEL_WIDTH * 2];
	int ret;

	ret = st7789_set_addr_window(x, y, x + w - 1, y + h - 1);
	if (ret) return ret;

	for (uint16_t i = 0; i < w; i++) {
		row[i * 2] = color >> 8;
		row[i * 2 + 1] = color & 0xFF;
	}

	for (uint16_t line = 0; line < h; line++) {
		ret = st7789_write_data(row, (size_t)w * 2);
		if (ret) return ret;
	}

	return 0;
}

/* Draws one character scaled up by `scale`, one source pixel row at a
 * time (again no per-character framebuffer needed).
 */
static int st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
	const uint8_t *glyph = glyph_lookup(c);
	uint8_t row[5 * 8 * 2]; /* generous fixed max: 5 cols * up to 8x scale * 2 bytes */
	int ret;

	ret = st7789_set_addr_window(x, y, x + 5 * scale - 1, y + 7 * scale - 1);
	if (ret) return ret;

	for (uint8_t srow = 0; srow < 7; srow++) {
		for (uint8_t scol = 0; scol < 5; scol++) {
			bool on = (glyph[scol] >> srow) & 0x1;
			uint16_t color = on ? fg : bg;

			for (uint8_t sx = 0; sx < scale; sx++) {
				size_t idx = (scol * scale + sx) * 2;

				row[idx] = color >> 8;
				row[idx + 1] = color & 0xFF;
			}
		}
		for (uint8_t sy = 0; sy < scale; sy++) {
			ret = st7789_write_data(row, (size_t)5 * scale * 2);
			if (ret) return ret;
		}
	}

	return 0;
}

static int st7789_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale)
{
	int ret;

	while (*str) {
		ret = st7789_draw_char(x, y, *str++, fg, bg, scale);
		if (ret) return ret;
		x += 6 * scale; /* 5 glyph columns + 1 blank column */
	}
	return 0;
}

int main(void)
{
	printk("\n=== TFT ST7789V3 (SPI, 240x%d) ===\n", PANEL_HEIGHT);

	if (!spi_is_ready_dt(&spi_spec)) {
		printk("SPI2 device not ready - check devicetree status/overlay\n");
		return 0;
	}
	if (!gpio_is_ready_dt(&reset_spec) || !gpio_is_ready_dt(&dc_spec)) {
		printk("RST/DC GPIO controller not ready\n");
		return 0;
	}

	if (gpio_pin_configure_dt(&dc_spec, GPIO_OUTPUT_INACTIVE)) {
		printk("DC GPIO configure failed\n");
		return 0;
	}

	if (st7789_reset()) {
		printk("Panel reset failed - check RST wiring (gpio0 16)\n");
		return 0;
	}

	if (st7789_init()) {
		printk("ST7789 init failed (SPI write error) - check wiring\n");
		return 0;
	}

	/* Black background, then a stack of color bars, then a text
	 * banner drawn on top - same "text + color bars" demo pattern
	 * used by this series' other raw-SPI color TFT lab. */
	if (st7789_fill_rect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, COLOR_BLACK)) {
		printk("Fill (background) failed\n");
		return 0;
	}

	static const uint16_t bar_colors[] = {
		COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE,
	};
	uint16_t bars_top = 40;
	uint16_t bar_height = (PANEL_HEIGHT - bars_top) / ARRAY_SIZE(bar_colors);

	for (size_t i = 0; i < ARRAY_SIZE(bar_colors); i++) {
		if (st7789_fill_rect(0, bars_top + i * bar_height, PANEL_WIDTH, bar_height, bar_colors[i])) {
			printk("Fill (color bar %zu) failed\n", i);
			return 0;
		}
	}

	if (st7789_draw_string(10, 10, "Hello World!", COLOR_WHITE, COLOR_BLACK, 2)) {
		printk("Text draw failed\n");
		return 0;
	}

	printk("ST7789V3 initialized, color bars + \"Hello World!\" drawn\n");

	while (1) {
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
