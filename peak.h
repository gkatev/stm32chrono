/**
 * Z-Score Peak Detection Algorithm, adapted from:
 * 
 * https://stackoverflow.com/questions/22583391/peak-signal-
 *   detection-in-realtime-timeseries-data/22640362#22640362
 *
 * ---------------------------------------------------------
 * 
 * Some performance tests:
 *
 * Single-sample time, measured by averaging
 *   running time of 10^6 sample operations:
 * 
 * No peak detection: 1.7 us
 * No peak detection, O2: 1.4 us
 * 
 * Original peak detection: 2.8 us
 * Original peak detection, O2: 2.2 us
 * 
 * Inlined peak detection, with pointer(s): 2.4 us
 * Inlined peak detection, no pointer(s): 1.96 us
 * 
 * Inline peak detection, no pointer(s), O2: 1.68 us
 *
 * Os -> O2, size increased from ~9kB to ~10kB
 * 
 */

#ifndef PEAK_H
#define PEAK_H

#include <stdint.h>

#include <core/usb_vcp.h>

typedef unsigned int uint;
typedef unsigned long ulong;

struct peak_stat {
	uint threshold;
	uint influence;
	uint lag;
	
	uint elements;
	uint oldest;
	
	uint16_t *samples;
	ulong sample_sum;
};

inline void peak_stat_init(struct peak_stat &s, uint threshold,
		uint influence, uint lag, uint16_t *samples) {
	
	s = (struct peak_stat) {
		.threshold = threshold,
		.influence = influence,
		.lag = lag,
		
		.elements = 0,
		.oldest = 0,
		
		.samples = samples,
		.sample_sum = 0
	};
}

inline void peak_stat_reset(struct peak_stat &s) {
	s.elements = 0;
	s.oldest = 0;
	s.sample_sum = 0;
}

inline bool peak_detect(struct peak_stat &s, uint16_t value) {
	uint16_t new_value = value;
	bool has_peak = false;
	
	if(s.elements == s.lag) {
		uint16_t average = s.sample_sum / s.elements;
		
		if(value > average && (uint)(value - average) > s.threshold) {
			uint16_t previous = s.samples[(s.oldest + s.lag - 1) % s.lag];
			new_value = s.influence * value + (1 - s.influence) * previous;
			
			has_peak = true;
		}
		
		s.sample_sum -= s.samples[s.oldest];
	} else
		s.elements++;
	
	s.sample_sum += new_value;
	
	s.samples[s.oldest] = new_value;
	s.oldest = (s.oldest + 1) % s.lag;
	
	return has_peak;
}

#endif
