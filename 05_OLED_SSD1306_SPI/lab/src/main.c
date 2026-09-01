/*
 * OLED (SSD1306, 0.96") over SPI - Zephyr, ESP32-S3
 *
 * This file is intentionally almost identical to Lab 2's main.c
 * (I2C mode). Only the overlay changed (bus type, compatible node
 * properties) - the application code barely differs, which is the
 * point of pairing these two labs.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>

#define DISPLAY_STACK_SIZE 2048
#define DISPLAY_PRIORITY   5

static void display_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(dev)) {
		printk("DisplayThread: display device not ready\n");
		return;
	}

	if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
		display_set_pixel_format(dev, PIXEL_FORMAT_MONO01);
	}

	if (cfb_framebuffer_init(dev)) {
		printk("DisplayThread: framebuffer init failed\n");
		return;
	}

	cfb_framebuffer_clear(dev, true);
	display_blanking_off(dev);

	printk("DisplayThread: ready (SPI mode)\n");

	int counter = 0;

	while (1) {
		char buf[32];

		snprintf(buf, sizeof(buf), "Count: %d", counter++);

		cfb_framebuffer_clear(dev, false);
		cfb_print(dev, "SSD1306 (SPI)", 0, 0);
		cfb_print(dev, buf, 0, 16);
		cfb_framebuffer_finalize(dev);

		k_sleep(K_SECONDS(1));
	}
}

K_THREAD_DEFINE(display_id, DISPLAY_STACK_SIZE, display_thread_entry,
		 NULL, NULL, NULL, DISPLAY_PRIORITY, 0, 0);

int main(void)
{
	printk("main: started, DisplayThread is running independently\n");
	return 0;
}
