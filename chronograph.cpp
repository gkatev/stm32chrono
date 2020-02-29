#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>

#include <core/usb_vcp.h>
#include <core/millis.h>

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
}

int main() {
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	
	millis_init();
	vcp_init();
	
	gpio_adc_init();
	
	adc_init();
	adc_channel(ADC_CHANNEL0);
	
	vcp_printf("Chronograph starting!\n");
	
	uint32_t last_adc = 0xFFFFFFFF;
	
	/* while(1) {
		uint32_t t = millis();
		
		for(int i = 0; i < 1000000; i++) {
			uint32_t val = adc_read();
		}
		
		uint32_t t2 = millis();
		
		vcp_printf("time = %u ms\n", t2 - t);
	} */
	
	while(1) {
		uint32_t adc_val = adc_read();
		
		if(adc_val != last_adc) {
			vcp_printf("ADC: %u\n", adc_val);
			last_adc = adc_val;
		}
	}
}
