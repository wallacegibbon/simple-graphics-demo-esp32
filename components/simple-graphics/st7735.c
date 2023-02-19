#include "st7735.h"
#include "point_iterator.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include <stddef.h>
#include <string.h>

static inline void ST7735_Screen_rst_high(struct ST7735_Screen *self) {
	ESP_ERROR_CHECK(gpio_set_level(self->rst_pin, 1));
}

static inline void ST7735_Screen_rst_low(struct ST7735_Screen *self) {
	ESP_ERROR_CHECK(gpio_set_level(self->rst_pin, 0));
}

static inline void ST7735_Screen_dc_high(struct ST7735_Screen *self) {
	ESP_ERROR_CHECK(gpio_set_level(self->dc_pin, 1));
}

static inline void ST7735_Screen_dc_low(struct ST7735_Screen *self) {
	ESP_ERROR_CHECK(gpio_set_level(self->dc_pin, 0));
}

void ST7735_Screen_write_byte(
	struct ST7735_Screen *self,
	uint8_t *data,
	int length
) {
	spi_transaction_t transac;

	memset(&transac, 0, sizeof(transac));
	transac.length = length * 8; // in bits
	transac.tx_buffer = data;
	transac.user = (void *) 1;

	ESP_ERROR_CHECK(gpio_set_level(self->cs_pin, 0));

	ESP_ERROR_CHECK(spi_device_polling_transmit(*self->dev, &transac));

	ESP_ERROR_CHECK(gpio_set_level(self->cs_pin, 1));
}

void ST7735_Screen_write_data(struct ST7735_Screen *self, uint16_t data) {
	uint8_t buffer[2];

	buffer[0] = data >> 8;
	buffer[1] = data;
	ST7735_Screen_dc_high(self);
	ST7735_Screen_write_byte(self, buffer, 2);
}

void ST7735_Screen_write_data8(struct ST7735_Screen *self, uint8_t data) {
	ST7735_Screen_dc_high(self);
	ST7735_Screen_write_byte(self, &data, 1);
}

void ST7735_Screen_write_cmd(struct ST7735_Screen *self, uint8_t data) {
	ST7735_Screen_dc_low(self);
	ST7735_Screen_write_byte(self, &data, 1);
}

void ST7735_Screen_set_address(
	struct ST7735_Screen *self, struct Point p1, struct Point p2
) {
	/// column address settings
	ST7735_Screen_write_cmd(self, 0x2A);
	ST7735_Screen_write_data(self, p1.x + 1);
	ST7735_Screen_write_data(self, p2.x + 1);

	/// row address setting
	ST7735_Screen_write_cmd(self, 0x2B);
	ST7735_Screen_write_data(self, p1.y + 26);
	ST7735_Screen_write_data(self, p2.y + 26);

	/// memory write
	ST7735_Screen_write_cmd(self, 0x2C);
}

void ST7735_Screen_draw_point(
	struct ST7735_Screen *self, struct Point p, int color
) {
	if (p.x >= self->size.x || p.y >= self->size.y)
		return;

	ST7735_Screen_set_address(self, p, p);
	ST7735_Screen_write_data(self, (uint16_t) color);
}

void ST7735_Screen_size(struct ST7735_Screen *self, struct Point *p) {
	Point_initialize(p, self->size.x, self->size.y);
}

/// The default `fill` calls `draw_point`, which will cause many
/// unnecessary `set_address` invocations.
void ST7735_Screen_fill(
	struct ST7735_Screen *self,
	struct Point p1,
	struct Point p2,
	int color
) {
	struct RectPointIterator point_iterator;
	struct Point p;

	RectPointIterator_initialize(&point_iterator, p1, p2);
	ST7735_Screen_set_address(self, p1, p2);

	while (RectPointIterator_next(&point_iterator, &p))
		ST7735_Screen_write_data(self, (uint16_t) color);
}

void ST7735_Screen_initialize(
	struct ST7735_Screen *self,
	spi_device_handle_t *dev,
	uint8_t cs_pin,
	uint8_t rst_pin,
	uint8_t dc_pin
) {
	self->painter_interface.draw_point =
		(PainterDrawPoint) ST7735_Screen_draw_point;

	self->painter_interface.size = (PainterSize) ST7735_Screen_size;

	self->painter_interface.clear = NULL;
	self->painter_interface.fill = (PainterFill) ST7735_Screen_fill;

	self->painter_interface.flush = NULL;

	self->size.x = 160;
	self->size.y = 80;

	gpio_set_direction(cs_pin, GPIO_MODE_OUTPUT);
	gpio_set_direction(rst_pin, GPIO_MODE_OUTPUT);
	gpio_set_direction(dc_pin, GPIO_MODE_OUTPUT);

	self->cs_pin = cs_pin;
	self->rst_pin = rst_pin;
	self->dc_pin = dc_pin;

	/// For simplicity, assume that screen is the only SPI device
	spi_device_acquire_bus(*dev, portMAX_DELAY);
	self->dev = dev;

	ST7735_Screen_rst_low(self);
	delay(200);

	ST7735_Screen_rst_high(self);
	delay(20);

	ST7735_Screen_write_cmd(self, 0x11);
	delay(100);

	/// display inversion mode (0 is black, -1 is white)
	ST7735_Screen_write_cmd(self, 0x21);

	/// Set the frame frequency of the full colors normal mode
	ST7735_Screen_write_cmd(self, 0xB1);
	/// Frame rate=fosc/((RTNA x 2 + 40) x (LINE + FPA + BPA +2))
	/// fosc = 850kHz
	ST7735_Screen_write_data8(self, 0x05); // RTNA
	ST7735_Screen_write_data8(self, 0x3A); // FPA
	ST7735_Screen_write_data8(self, 0x3A); // BPA

	/// Set the frame frequency of the Idle mode
	ST7735_Screen_write_cmd(self, 0xB2);
	/// Frame rate=fosc/((RTNB x 2 + 40) x (LINE + FPB + BPB +2))
	/// fosc = 850kHz
	ST7735_Screen_write_data8(self, 0x05); // RTNB
	ST7735_Screen_write_data8(self, 0x3A); // FPB
	ST7735_Screen_write_data8(self, 0x3A); // BPB

	/// Set the frame frequency of the Partial mode/ full colors
	ST7735_Screen_write_cmd(self, 0xB3);
	ST7735_Screen_write_data8(self, 0x05);
	ST7735_Screen_write_data8(self, 0x3A);
	ST7735_Screen_write_data8(self, 0x3A);
	ST7735_Screen_write_data8(self, 0x05);
	ST7735_Screen_write_data8(self, 0x3A);
	ST7735_Screen_write_data8(self, 0x3A);

	ST7735_Screen_write_cmd(self, 0xB4);
	ST7735_Screen_write_data8(self, 0x03);

	ST7735_Screen_write_cmd(self, 0xC0);
	ST7735_Screen_write_data8(self, 0x62);
	ST7735_Screen_write_data8(self, 0x02);
	ST7735_Screen_write_data8(self, 0x04);

	ST7735_Screen_write_cmd(self, 0xC1);
	ST7735_Screen_write_data8(self, 0xC0);

	ST7735_Screen_write_cmd(self, 0xC2);
	ST7735_Screen_write_data8(self, 0x0D);
	ST7735_Screen_write_data8(self, 0x00);

	ST7735_Screen_write_cmd(self, 0xC3);
	ST7735_Screen_write_data8(self, 0x8D);
	ST7735_Screen_write_data8(self, 0x6A);

	ST7735_Screen_write_cmd(self, 0xC4);
	ST7735_Screen_write_data8(self, 0x8D);
	ST7735_Screen_write_data8(self, 0xEE);

	ST7735_Screen_write_cmd(self, 0xC5);
	ST7735_Screen_write_data8(self, 0x0E); /// VCOM

	ST7735_Screen_write_cmd(self, 0xE0);
	ST7735_Screen_write_data8(self, 0x10);
	ST7735_Screen_write_data8(self, 0x0E);
	ST7735_Screen_write_data8(self, 0x02);
	ST7735_Screen_write_data8(self, 0x03);
	ST7735_Screen_write_data8(self, 0x0E);
	ST7735_Screen_write_data8(self, 0x07);
	ST7735_Screen_write_data8(self, 0x02);
	ST7735_Screen_write_data8(self, 0x07);
	ST7735_Screen_write_data8(self, 0x0A);
	ST7735_Screen_write_data8(self, 0x12);
	ST7735_Screen_write_data8(self, 0x27);
	ST7735_Screen_write_data8(self, 0x37);
	ST7735_Screen_write_data8(self, 0x00);
	ST7735_Screen_write_data8(self, 0x0D);
	ST7735_Screen_write_data8(self, 0x0E);
	ST7735_Screen_write_data8(self, 0x10);

	ST7735_Screen_write_cmd(self, 0xE1);
	ST7735_Screen_write_data8(self, 0x10);
	ST7735_Screen_write_data8(self, 0x0E);
	ST7735_Screen_write_data8(self, 0x03);
	ST7735_Screen_write_data8(self, 0x03);
	ST7735_Screen_write_data8(self, 0x0F);
	ST7735_Screen_write_data8(self, 0x06);
	ST7735_Screen_write_data8(self, 0x02);
	ST7735_Screen_write_data8(self, 0x08);
	ST7735_Screen_write_data8(self, 0x0A);
	ST7735_Screen_write_data8(self, 0x13);
	ST7735_Screen_write_data8(self, 0x26);
	ST7735_Screen_write_data8(self, 0x36);
	ST7735_Screen_write_data8(self, 0x00);
	ST7735_Screen_write_data8(self, 0x0D);
	ST7735_Screen_write_data8(self, 0x0E);
	ST7735_Screen_write_data8(self, 0x10);

	/// 16 bit color
	ST7735_Screen_write_cmd(self, 0x3A);
	ST7735_Screen_write_data8(self, 0x05);

	ST7735_Screen_write_cmd(self, 0x36);
	ST7735_Screen_write_data8(self, 0x78);

	/// display on
	ST7735_Screen_write_cmd(self, 0x29);
}

