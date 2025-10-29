/*
 * Copyright (c) 2019 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

enum corner {
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_RIGHT,
	BOTTOM_LEFT
};

typedef void (*fill_buffer)(enum corner corner, uint8_t grey, uint8_t *buf,
			    size_t buf_size);

static void fill_buffer_mono(enum corner corner, uint8_t grey,
			     uint8_t black, uint8_t white,
			     uint8_t *buf, size_t buf_size)
{
	uint16_t color;

	switch (corner) {
	case BOTTOM_LEFT:
		color = (grey & 0x01u) ? white : black;
		break;
	default:
		color = black;
		break;
	}

	memset(buf, color, buf_size);
}

static inline void fill_buffer_mono01(enum corner corner, uint8_t grey,
				      uint8_t *buf, size_t buf_size)
{
	fill_buffer_mono(corner, grey, 0x00u, 0xFFu, buf, buf_size);
}

static inline void fill_buffer_mono10(enum corner corner, uint8_t grey,
				      uint8_t *buf, size_t buf_size)
{
	fill_buffer_mono(corner, grey, 0xFFu, 0x00u, buf, buf_size);
}

int main(void)
{
	uint8_t bg_color;
	uint8_t *buf;
	const struct device *display_dev;
	struct display_capabilities capabilities;
	struct display_buffer_descriptor buf_desc;
	size_t buf_size = 0;
	fill_buffer fill_buffer_fnc = NULL;
	int ret;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device %s not found. Aborting sample.",
			display_dev->name);
		return 0;
	}

	LOG_INF("Display sample for %s", display_dev->name);
	display_get_capabilities(display_dev, &capabilities);

	buf_size = (capabilities.x_resolution * capabilities.y_resolution) / 8;

	switch (capabilities.current_pixel_format) {
	case PIXEL_FORMAT_MONO01:
		bg_color = 0xFFu;
		fill_buffer_fnc = fill_buffer_mono01;
		buf_size = DIV_ROUND_UP(DIV_ROUND_UP(
			buf_size, NUM_BITS(uint8_t)), sizeof(uint8_t));
		break;
	case PIXEL_FORMAT_MONO10:
		bg_color = 0x00u;
		fill_buffer_fnc = fill_buffer_mono10;
		buf_size = DIV_ROUND_UP(DIV_ROUND_UP(
			buf_size, NUM_BITS(uint8_t)), sizeof(uint8_t));
		break;
	default:
		LOG_ERR("Unsupported pixel format. Aborting sample.");
		return 0;
	}

	buf = k_aligned_alloc(CONFIG_SAMPLE_BUFFER_ADDR_ALIGN, buf_size);

	if (buf == NULL) {
		LOG_ERR("Could not allocate memory. Aborting sample.");
		return 0;
	}

	(void)memset(buf, bg_color, buf_size);

	buf_desc.buf_size = buf_size;
	buf_desc.pitch = ROUND_UP(capabilities.x_resolution, CONFIG_SAMPLE_PITCH_ALIGN);
	buf_desc.width = capabilities.x_resolution;
	buf_desc.height = capabilities.y_resolution;

	display_write(display_dev, 0, 0, &buf_desc, buf);

	return 0;
}
