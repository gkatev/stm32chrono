#include <string.h>

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

void chrono();
void chrono_stat_update(chrono_stat_t& stat, float measurement);
float calc_fps(uint16_t ticks);

void test_sample_time();
void test_peak_samples();

void adc_init();
void adc_channel(uint8_t channel);
uint32_t adc_read();
void gpio_adc_init();

void timer_init();
uint16_t timer_read();
void timer_start();
void timer_stop();

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
	
	// test_sample_time();
	// test_peak_samples();
	
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
	
	/* Discard the first ADC measurement,
	 * which for some reason is way off.. */
	adc_read();
	
	while(1) {
		uint16_t ticks, adc_val;
		
		if(state == timeout_s) {
			adc_channel(CHANNEL_FRONT);
			state = front_s;
			
			peak_stat_reset(peak_stat);
			
			DEBUG_PRINTF("TIMEOUT\n");
		}
		
		if(vcp_available()) {
			if(vcp_read() == 'r') {
				chrono_stat = {.mode = mode_fps, .weight = 20};
				display_draw_stat(chrono_stat);
			}
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
			
			DEBUG_PRINTF("FD\n");
		} else if(state == back_s) {
			timer_stop();
			
			float fps = calc_fps(ticks);
			
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
		DEBUG_PRINTF("ticks: 0\n");
		return 0;
	}
	
	float dt_us = TICKS_TO_US(ticks);
	float speed = DISTANCE_UM / dt_us * SPEED_CALIBRATION_FACTOR;
	float fps = speed * MPS_TO_FPS_FACTOR;
	
	vcp_printf("ticks: %u dt(us): %u U(m/s): %u U(fps): %u\n",
		ticks, (int) dt_us, (int) speed, (int) fps);
	
	return fps;
}

// --------------------------------------------

void test_sample_time() {
	uint16_t samples[16];
	peak_stat_t peak_stat;
	
	peak_stat_init(peak_stat, 5000, 0, 16, samples);
	
	uint32_t t = millis();
	
	for(int i = 0; i < 1000000; i++) {
		uint32_t val = adc_read();
		peak_detect(peak_stat, val);
	}
	
	uint32_t t2 = millis();
	
	vcp_printf("Sample time = %u.%u us\n",
		(t2 - t)/1000, (t2 - t) % 1000 / 100);
}

void test_peak_samples() {
	uint16_t samples[PEAK_LAG];
	peak_stat_t peak_stat;
	
	peak_stat_init(peak_stat, PEAK_THRESHOLD,
		0, PEAK_LAG, samples);
	
	adc_channel(CHANNEL_FRONT);
	// adc_channel(CHANNEL_REAR);
	
	while(1) {
		uint16_t adc_val = adc_read();
		
		if(peak_detect(peak_stat, adc_val)) {
			int samples = 1;
			int avg = peak_stat.sample_sum/peak_stat.elements;
			
			while(peak_detect(peak_stat, adc_read()))
				samples++;
			
			vcp_printf("Peak detected. First (diff): %d. "
				"Total samples: %d\n", adc_val - avg, samples);
		}
	}
}

// --------------------------------------------

void adc_init() {
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV2);
	
	adc_power_off(ADC1);
	rcc_periph_reset_pulse(RST_ADC1);
	
	/* These measurements include the time for the peak detection
	 * algorithm to work, and are obtained with an ADC prescaler
	 * value of 2 (RCC_CFGR_ADCPRE_PCLK2_DIV2) unless otherwise
	 * specified, and varying sample times.
	 *
	 * Initially, the smallest possible sampling time, 1DOT5CYC, was
	 * used. It gave 1.7 us sampling time, but the measurements were
	 * pretty unstable. (perhaps also because of external factors
	 * belonging in the magestic realm of analog electronics)
	 *
	 * We can actually go higher with our sampling times. The main
	 * limiting factor is the maximum projectile speed we wish to
	 * measure, coupled with the sensing distance of our diodes.
	 * Is some cases, if the peak detection algorithm, has an
	 * "on-the-fly" training period the diode distance could also
	 * affect the sampling time.
	 *
	 * With ADC_SMPR_SMP_28DOT5CYC, we get 2.4us/sample, and the
	 * measurement are already much more stable. But, we can go
	 * even higher.
	 *
	 * While this is unconfirmed/untested, we would wish to have
	 * higher sampling cycles because we appear to get more stable
	 * values this way. More stable values allow us to have higher
	 * tolerances in our peak detection, which in turn can help
	 * translate into higher sensitivity (so more user friendliness),
	 * along with higher accuracy.
	 *
	 * With my specific 3D-printed chrono case, I measured the detection
	 * area to be between 3~5 mm. Assuming a maximum measurement speed
	 * of ~250 m/s (820 fps), supposing the worst case of 3 mm, a speed
	 * of 250 m/s, and a mininum required amount of 2 samples, we arrive
	 * at a sampling time of 5-6 us.
	 *
	 * ADC_SMPR_SMP_71DOT5CYC gives a sample time of 3.6 us.
	 * ADC_SMPR_SMP_239DOT5CYC gives a sample time of 8.4 us.
	 *
	 * ADC_SMPR_SMP_71DOT5CYC and RCC_CFGR_ADCPRE_PCLK2_DIV4
	 * give a sample time of 6 us.
	 *
	 * However, there also other concerns when choosing high sample times:
	 * 1. If the trigger is off by even one sample, a higher speed fault
	 *   will be introduced compared to a smaller sample time.
	 * 2. Does it even make sense trying to read a stable measurement,
	 *   if the environment is volatile whilst sampling it?
	 *
	 * Regarding point (1), at 100 m/s, 1 sample's fault is worth:
	 *   At 2.4us/sample, 0.8 m/s or 2.6 fps
	 *   At 6us/sample, 2 m/s or 6.5 fps
	 *   At 8us/sample, 2.6 m/s or 8.5 fps
	 */
	
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_28DOT5CYC);
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
	 * a UEV is generated immediately. So, we handle this here. */
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
