#ifndef DISPLAY_H
#define DISPLAY_H

#include "ssd1306/ssd1306_fonts.h"
#include "chronograph.h"

typedef enum {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
} DISPLAY_ALIGNMENT;

void display_init();

void display_write_aligned(uint8_t y, const char *str, FontDef font,
	DISPLAY_ALIGNMENT alignment = ALIGN_LEFT);
void display_draw_stat(chrono_stat_t stats);

#endif
