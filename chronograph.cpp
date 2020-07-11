#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include <core/usb_vcp.h>
#include <core/millis.h>

#include "chronograph.h"
#include "display.h"
#include "peak.h"

// --------------------------------------------

volatile enum {front_s, back_s, timeout_s} state;

// --------------------------------------------

void adc_init();
void adc_channel(uint8_t channel);
uint32_t adc_read();
void gpio_adc_init();

void timer_init();
uint16_t timer_read();
void timer_start();
void timer_stop();

void chrono();
void chrono_stat_update(chrono_stat_t& stat, float measurement);
float calc_fps(uint16_t ticks);

// --------------------------------------------

extern "C" void tim2_isr(void) {
	if(timer_get_flag(TIM2, TIM_SR_UIF)) {
		timer_clear_flag(TIM2, TIM_SR_UIF);
		
		state = timeout_s;
	}
}

int main() {
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	
	millis_init();
	vcp_init();
	
	gpio_adc_init();
	adc_init();
	
	timer_init();
	display_init();
	
	chrono();
}

void chrono() {
	uint16_t samples[PEAK_LAG];
	peak_stat_t peak_stat;
	chrono_stat_t chrono_stat;
	
	peak_stat_init(peak_stat, PEAK_THRESHOLD,
		PEAK_INFLUENCE, PEAK_LAG, samples);
	
	chrono_stat = {.mode = mode_fps, .weight = 20};
	display_draw_stat(chrono_stat);
	
	adc_channel(CHANNEL_FRONT);
	state = front_s;
	
	vcp_printf("Measuring!\n");
	
	while(1) {
		uint16_t ticks, adc_val;
		
		if(state == timeout_s) {
			adc_channel(CHANNEL_FRONT);
			state = front_s;
			
			#ifdef DEBUG
				vcp_printf("TIMEOUT\n");
			#endif
		}
		
		ticks = timer_read();
		adc_val = adc_read();
		
		if(!peak_detect(peak_stat, adc_val))
			continue;
		
		/* Peak detected */
		
		peak_stat_reset(peak_stat);
		
		if(state == front_s) {
			timer_start();
			
			adc_channel(CHANNEL_REAR);
			state = back_s;
			
			#ifdef DEBUG
				vcp_printf("FD\n");
			#endif
		} else if(state == back_s) {
			timer_stop();
			
			float fps = calc_fps(ticks);
			vcp_printf("FPS = %d\n", (int) fps);
			
			chrono_stat_update(chrono_stat, fps);
			display_draw_stat(chrono_stat);
			
			adc_channel(CHANNEL_FRONT);
			state = front_s;
		}
	}
}

void chrono_stat_update(chrono_stat_t& stat, float measurement) {
	stat.measurement = measurement;
	stat.count++;
	
	stat.m_sum += measurement;
	stat.m_sqsum += measurement*measurement;
}

float calc_fps(uint16_t ticks) {
	if(ticks == 0) {
		#ifdef DEBUG
			vcp_printf("ticks: 0\n");
		#endif
		
		return 0;
	}
	
	float dt_us = TICKS_TO_US(ticks);
	float speed = DISTANCE_UM / dt_us * SPEED_CALIBRATION_FACTOR;
	float fps = speed * MPS_TO_FPS_FACTOR;
	
	#ifdef DEBUG
		vcp_printf("ticks: %u dt(us): %u U(m/s): %u U(fps): %u\n",
			ticks, (int) dt_us, (int) speed, (int) fps);
	#endif
	
	return fps;
}

// --------------------------------------------

void adc_init() {
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV2);
	
	adc_power_off(ADC1);
	rcc_periph_reset_pulse(RST_ADC1);
	
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_1DOT5CYC);
    adc_enable_external_trigger_regular(ADC1, ADC_CR2_EXTSEL_SWSTART);
    
    adc_power_on(ADC1);
    
	for(int i = 0; i < 50; i++)
		__asm__("nop");
	
	adc_reset_calibration(ADC1);
    adc_calibrate(ADC1);
}

void adc_channel(uint8_t channel) {
    adc_set_regular_sequence(ADC1, 1, &channel);
}

uint32_t adc_read() {
	adc_start_conversion_regular(ADC1);
	while(!adc_eoc(ADC1));
	
	return adc_read_regular(ADC1);
}

void gpio_adc_init() {
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO0);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO1);
}

void timer_init() {
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_reset_pulse(RST_TIM2);
	
	timer_set_prescaler(TIM2, (rcc_apb1_frequency * 2) / TIMER_FREQ);
	timer_set_period(TIM2, TIMER_ARR);
	timer_one_shot_mode(TIM2);
	
	/* For some reason, the first time the counter is enabled,
	 * UEV is generated immediately. So, we handle this here. */
	timer_set_counter(TIM2, TIMER_ARR);
	timer_enable_counter(TIM2);
	while(!timer_get_flag(TIM2, TIM_SR_UIF));
	timer_clear_flag(TIM2, TIM_SR_UIF);
	
	nvic_enable_irq(NVIC_TIM2_IRQ);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
}

uint16_t timer_read() {
	return timer_get_counter(TIM2);
}

void timer_start() {
	timer_set_counter(TIM2, 0);
	timer_enable_counter(TIM2);
}

void timer_stop() {
	timer_disable_counter(TIM2);
}
