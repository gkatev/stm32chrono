#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include <core/usb_vcp.h>
#include <core/millis.h>

#include "chronograph.h"
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
	
	chrono();
	
	// extern void test();
	// test();
}

void chrono() {
	struct peak_stat pstat;
	uint16_t samples[PEAK_LAG];
	
	peak_stat_init(pstat, PEAK_THRESHOLD,
		PEAK_INFLUENCE, PEAK_LAG, samples);
	
	adc_channel(CHANNEL_FRONT);
	state = front_s;
	
	vcp_printf("Measuring!\n");
	
	while(1) {
		uint16_t timer_val, adc_val;
		
		if(state == timeout_s) {
			adc_channel(CHANNEL_FRONT);
			state = front_s;
			
			#ifdef DEBUG
				vcp_printf("TIMEOUT\n");
			#endif
		}
		
		timer_val = timer_read();
		adc_val = adc_read();
		
		if(!peak_detect(pstat, adc_val))
			continue;
		
		/* Peak detected */
		
		if(state == front_s) {
			timer_start();
			
			adc_channel(CHANNEL_REAR);
			state = back_s;
			
			#ifdef DEBUG
				vcp_printf("FD\n");
			#endif
		} else if(state == back_s) {
			timer_stop();
			
			float fps = calc_fps(timer_val);
			vcp_printf("FPS = %f\n", fps);
		}
		
		peak_stat_reset(pstat);
	}
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
		vcp_printf("ticks: %u dt(us): %u U(m/s): %f U(fps): %f\n",
			ticks, dt_us, speed, fps);
	#endif
	
	return fps;
}

void test() {
	/* while(1) {
		uint32_t t = millis();
		
		for(int i = 0; i < 1000000; i++) {
			uint32_t val = adc_read();
		}
		
		uint32_t t2 = millis();
		
		vcp_printf("time = %u ms\n", t2 - t);
	} */
	
	/* uint32_t last_adc = 0xFFFFFFFF;
	
	while(1) {
		uint32_t adc_val = adc_read();
		
		if(adc_val != last_adc) {
			vcp_printf("ADC: %u\n", adc_val);
			last_adc = adc_val;
		}
	} */
	
	/* while(1) {
		uint16_t adc_val = adc_read();
		
		// vcp_printf("ADC: %d\n", adc_val);
		
		if(peak_detect(pstat, adc_val)) {
			uint16_t average = pstat.sample_sum / pstat.elements;
			vcp_printf("Peak: %u, Average: %u, Diff: %u\n",
				adc_val, average, adc_val - average);
			
			peak_stat_reset(pstat);
		}
	} */
	
	/* while(1) {
		uint32_t t = millis();
		
		for(int i = 0; i < 1000000; i++) {
			uint16_t adc_val = adc_read();
			peak_detect(pstat, adc_val);
			
			// if(adc_val > 10000000)
				// vcp_printf("FAKE");
		}
		
		uint32_t t2 = millis();
		
		vcp_printf("time = %u ms\n", t2 - t);
	} */
	
	/* timer_init();
	
	vcp_printf("starting\n");
	
	while(true) {
		state = front_s;
		
		timer_enable_counter(TIM2);
		uint32_t t = millis();
		
		// while(state != timeout_s) {}
		
		while(state != timeout_s) {
			if(millis() - t > 200) {
				timer_stop();
				delay_ms(200);
				timer_start();
				
				while(state != timeout_s);
			}
		}
		
		uint32_t t2 = millis();
		
		vcp_printf("time = %u ms\n", t2 - t);
	} */
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
