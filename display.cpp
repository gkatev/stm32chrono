#include <string.h>
#include <math.h>

#include <libopencm3/stm32/i2c.h>
#include <core/lib/mini_printf.h>

#include "ssd1306/ssd1306.h"
#include "chronograph.h"
#include "display.h"

#include <core/usb_vcp.h>
#include <core/UART.h>

static float stdev(int count, float sum, float sqsum) {
	return count ? sqrt((float) (count*sqsum - sum*sum) / (count * count)) : 0;
}

static int fract_part(float num, int points) {
	int div = pow(10, points);
	return (int) (num * div) % div;
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

void display_init() {
	ssd1306_Init();
}

void display_draw_stats(chrono_stats_t stats) {
	float deviation = stdev(stats.count, stats.m_sum, stats.m_sqsum);
	float average = (float) stats.m_sum / stats.count;
	
	const char *mode_str, *unit_str;
	char buffer[129];
	
	switch(stats.mode) {
		case mode_fps: mode_str = "fps"; unit_str = "fps"; break;
		case mode_mps: mode_str = "mps"; unit_str = "m/s"; break;
		case mode_joule: mode_str = "joule"; unit_str = "J"; break;
		case mode_rps: mode_str = "rps"; unit_str = "rps"; break;
		default: mode_str = "?"; unit_str = "?";
	}
	
	mini_snprintf(buffer, 128, "%s | .%d", mode_str, stats.weight);
	display_write_aligned(0, buffer, Font_6x8, ALIGN_LEFT);
	
	mini_snprintf(buffer, 128, "%02d", stats.count);
	display_write_aligned(0, buffer, Font_6x8, ALIGN_RIGHT);
	
	mini_snprintf(buffer, 128, "%d.%02d",
		(int) stats.measurement, fract_part(stats.measurement, 2));
	display_write_aligned(20, buffer, Font_11x18, ALIGN_CENTER);
	
	mini_snprintf(buffer, 128, "Average: %d.%02d %s",
		(int) average, fract_part(average, 2), unit_str);
	display_write_aligned(48, buffer, Font_6x8, ALIGN_CENTER);
	
	mini_snprintf(buffer, 128, "Deviation: %d.%02d %s",
		(int) deviation, fract_part(deviation, 2), unit_str);
	display_write_aligned(56, buffer, Font_6x8, ALIGN_CENTER);
	
	ssd1306_UpdateScreen();
}

void display_test() {
	display_init();
	
	display_draw_stats({
		.mode = mode_fps,
		.weight = 28,
		.measurement = 315.67,
		.count = 6,
		.m_sum = 1878,
		.m_sqsum = 588008,
	});
	
	while(1);
	
	/* display_write_aligned(0, "fps | .20", Font_6x8, ALIGN_LEFT);
	// display_write_aligned(0, "fps .20", Font_6x8, ALIGN_LEFT);
	// display_write_aligned(0, "fps", Font_6x8, ALIGN_LEFT);
	// display_write_aligned(0, ".20", Font_6x8, ALIGN_CENTER);
	// display_write_aligned(8, ".20", Font_6x8, ALIGN_LEFT);
	display_write_aligned(0, "03", Font_6x8, ALIGN_RIGHT);
	
	display_write_aligned(20, "320.41", Font_11x18, ALIGN_CENTER);
	
	display_write_aligned(48, "Average: 328.73 fps", Font_6x8, ALIGN_CENTER);
	display_write_aligned(56, "Deviation: 2.45 fps", Font_6x8, ALIGN_CENTER);
	
	ssd1306_UpdateScreen();
	
	while(1); */
}
