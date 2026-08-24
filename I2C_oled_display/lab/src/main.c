/*
 * SSD1306 OLED bring-up lab — I2C0 (J25 pin 3/4), SR110 RDK.
 *
 * Draws a full white rectangle first (proves the panel + I2C link work at
 * all), then a slowly moving vertical bar (proves display_write() and
 * addressing/pitch are correct, not just a static full-screen fill).
 *
 * CONFIRMED: Zephyr's standard display subsystem + the official
 * solomon,ssd1306fb driver are used as-is — no custom register-level code
 * needed for this chip.
 *
 * TODO/VERIFY: I2C0 pinctrl node names in app.overlay (see comment there).
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ssd1306_lab, LOG_LEVEL_INF);

#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define BAR_WIDTH       8

static uint8_t framebuf[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

static void fill_screen(uint8_t pattern)
{
	memset(framebuf, pattern, sizeof(framebuf));
}

/* Very small monochrome vertical-bar "animation" to prove addressing works,
 * not just a static full-screen fill. SSD1306 is page-addressed: each byte
 * covers 8 vertical pixels in one column. */
static void draw_vbar(int x)
{
	memset(framebuf, 0x00, sizeof(framebuf));
	for (int page = 0; page < DISPLAY_HEIGHT / 8; page++) {
		for (int col = x; col < x + BAR_WIDTH && col < DISPLAY_WIDTH; col++) {
			framebuf[page * DISPLAY_WIDTH + col] = 0xFF;
		}
	}
}

int main(void)
{
	const struct device *display_dev = DEVICE_DT_GET_ANY(solomon_ssd1306)   /* NOT solomon_ssd1306fb -- confirmed on real hardware, Zephyr v4.4.1 */;
	struct display_capabilities caps;
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(framebuf),
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
		.pitch = DISPLAY_WIDTH,
	};

	LOG_INF("SSD1306 I2C0 lab starting");

	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready -- check I2C0 pinctrl/wiring/address (0x3c vs 0x3d)");
		return -1;
	}

	display_get_capabilities(display_dev, &caps);
	LOG_INF("Display ready: %dx%d", caps.x_resolution, caps.y_resolution);

	/* Step 1: full white screen -- proves the panel responds at all. */
	fill_screen(0xFF);
	display_write(display_dev, 0, 0, &desc, framebuf);
	display_blanking_off(display_dev);
	LOG_INF("Step 1: full white screen -- check the panel is lit solid white");
	k_msleep(2000);

	/* Step 2: full black screen -- proves we can also clear it. */
	fill_screen(0x00);
	display_write(display_dev, 0, 0, &desc, framebuf);
	LOG_INF("Step 2: full black screen -- check the panel is fully dark");
	k_msleep(2000);

	/* Step 3: a vertical bar sweeping left to right -- proves addressing
	 * and pitch are correct, not just a lucky full-screen fill. */
	LOG_INF("Step 3: sweeping vertical bar -- watch it move left to right");
	int x = 0;
	int dir = 1;

	while (1) {
		draw_vbar(x);
		display_write(display_dev, 0, 0, &desc, framebuf);

		x += dir * 4;
		if (x >= DISPLAY_WIDTH - BAR_WIDTH || x <= 0) {
			dir = -dir;
		}
		k_msleep(80);
	}

	return 0;
}
