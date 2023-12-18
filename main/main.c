#include "driver/gpio.h"
#include "esp_log.h"
#include "sc_color.h"
#include "sc_common.h"
#include "sc_painter.h"
#include "sc_ssd1306.h"
#include "sc_ssd1306_esp32_i2c.h"
#include "sc_st7735.h"
#include "sc_st7735_esp32_softspi.h"

void fancy_display_1(struct painter *painter) {
	static int current_cnt = 0, step = 1;
	struct point p;
	int color, i;

	point_initialize(&p, 64, 32);
	for (i = 0; i < 31; i++) {
		color = current_cnt == i ? BLACK_16bit : GREEN_16bit;
		painter_draw_circle(painter, p, i, color);
	}
	painter_flush(painter);

	if (current_cnt == 31)
		step = -1;
	else if (current_cnt == 0)
		step = 1;

	current_cnt += step;
}

void initialize_screen_1(struct ssd1306_screen *screen, struct ssd1306_adaptor_esp32_i2c *adaptor) {
	printf("initializing SSD1306...\r\n");

	ssd1306_adaptor_esp32_i2c_initialize(adaptor, 0x3C, I2C_NUM_0);

	ssd1306_initialize(
		screen,
		(const struct ssd1306_adaptor_i **)adaptor);

	printf("SSD1306 screen on...\r\n");
	ssd1306_display_on(screen);

	// printf("setting SSD1306 screen to 32-row mode...\r\n");
	// SSD1306_Screen_fix_32row(&screen);
}

void initialize_screen_2(
	struct st7735_screen *screen2,
	struct st7735_adaptor_esp32_soft_spi *adaptor2) {

	/// start the BG LED of T-Dongle-S3
	ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_38, GPIO_MODE_OUTPUT));
	ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_38, 0));

	printf("initializing ST7735...\r\n");

	st7735_adaptor_esp32_soft_spi_initialize(
		adaptor2, GPIO_NUM_3, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_1, GPIO_NUM_2);

	st7735_initialize(
		screen2,
		(const struct st7735_adaptor_i **)adaptor2);
}

void app_main() {
	struct ssd1306_adaptor_esp32_i2c adaptor1;
	struct ssd1306_screen screen1;
	struct st7735_adaptor_esp32_soft_spi adaptor2;
	struct st7735_screen screen2;
	struct painter painter;
	struct point p1;
	struct point p2;

	// initialize_screen_1(&screen1, &adaptor1);
	initialize_screen_2(&screen2, &adaptor2);

	// ssd1306_screen_set_up_down_invert(&screen1);

	// painter.drawing_board = (struct drawing_i **) &screen1;
	painter.drawing_board = (const struct drawing_i **)&screen2;

	printf("clearing screen...\r\n");
	painter_clear(&painter, BLACK_16bit);

	printf("drawing a rectangle...\r\n");
	point_initialize(&p1, 64 - 50, 32 - 20);
	point_initialize(&p2, 64 + 50, 32 + 20);
	painter_draw_rectangle(&painter, p1, p2, BLUE_16bit);

	printf("drawing a circle on top left...\r\n");
	point_initialize(&p1, 64 - 50, 32 - 20);
	painter_draw_circle(&painter, p1, 5, RED_16bit);

	/*
	printf("drawing a line...\r\n");
	point_initialize(&p1, 30, 10);
	point_initialize(&p2, 20, 50);
	painter_draw_line(&painter, p1, p2, WHITE_1bit);
	*/

	printf("flushing the screen...\r\n");
	painter_flush(&painter);

	while (1) {
		fancy_display_1(&painter);
	}
}
