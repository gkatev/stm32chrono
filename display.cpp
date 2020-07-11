#include <string.h>
#include <math.h>

#include <libopencm3/stm32/i2c.h>
#include <core/lib/mini_printf.h>

#include "ssd1306/ssd1306.h"
#include "chronograph.h"
#include "display.h"

#include <core/usb_vcp.h>
#include <core/UART.h>

// -------------------------------------------------

static float stdev(int count, float sum, float sqsum) {
	return count ? sqrt((float) (count*sqsum - sum*sum) / (count * count)) : 0;
}

static int fract_part(float num, int points) {
	int div = pow(10, points);
	return (int) (num * div) % div;
}

// -------------------------------------------------

void display_init() {
	ssd1306_Init();
}

void display_write_aligned(uint8_t y, const char *str, FontDef font,
		DISPLAY_ALIGNMENT alignment) {
	
	size_t len = strlen(str) * font.FontWidth;
	size_t x = 0;
	
	if(alignment == ALIGN_CENTER)
		x = (128 - len)/2;
	else if(alignment == ALIGN_RIGHT)
		x = 128 - len;
	
	ssd1306_SetCursor((x > 0 ? x : 0), y);
	ssd1306_WriteString(str, font, White);
}

void display_draw_stat(chrono_stat_t stat) {
	float deviation = stdev(stat.count, stat.m_sum, stat.m_sqsum);
	float average = (float) stat.m_sum / stat.count;
	
	const char *mode_str, *unit_str;
	char buffer[129];
	
	switch(stat.mode) {
		case mode_fps: mode_str = "fps"; unit_str = "fps"; break;
		case mode_mps: mode_str = "mps"; unit_str = "m/s"; break;
		case mode_joule: mode_str = "joule"; unit_str = "J"; break;
		case mode_rps: mode_str = "rps"; unit_str = "rps"; break;
		default: mode_str = "?"; unit_str = "?";
	}
	
	mini_snprintf(buffer, 129, "%s | .%d", mode_str, stat.weight);
	display_write_aligned(0, buffer, Font_6x8, ALIGN_LEFT);
	
	mini_snprintf(buffer, 129, "%02d", stat.count);
	display_write_aligned(0, buffer, Font_6x8, ALIGN_RIGHT);
	
	if(stat.count)
		mini_snprintf(buffer, 129, "%d.%02d",
			(int) stat.measurement, fract_part(stat.measurement, 2));
	else
		mini_snprintf(buffer, 129, "---");
	
	display_write_aligned(20, buffer, Font_11x18, ALIGN_CENTER);
	
	mini_snprintf(buffer, 129, "Average: %d.%02d %s",
		(int) average, fract_part(average, 2), unit_str);
	display_write_aligned(48, buffer, Font_6x8, ALIGN_CENTER);
	
	mini_snprintf(buffer, 129, "Deviation: %d.%02d %s",
		(int) deviation, fract_part(deviation, 2), unit_str);
	display_write_aligned(56, buffer, Font_6x8, ALIGN_CENTER);
	
	ssd1306_UpdateScreen();
}
