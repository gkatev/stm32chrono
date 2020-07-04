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

typedef unsigned int uint;
typedef unsigned long ulong;

struct peak_stat {
	uint threshold;
	uint influence;
	uint lag;
	
	uint training;
	uint elements;
	uint oldest;
	
	uint16_t *samples;
	ulong sample_sum;
};

void peak_stat_init(struct peak_stat *s, uint threshold,
	uint influence, uint lag, uint training, uint16_t *samples);

void peak_stat_reset(struct peak_stat *s);
// int peak_detect(struct peak_stat *s, uint16_t value);

inline int peak_detect(struct peak_stat s, uint16_t value) {
	uint16_t new_value = value;
	int status = 0;
	
	if(s.elements >= s.training) {
		uint16_t average = s.sample_sum / s.elements;
		
		if(value > average && (uint)(value - average) > s.threshold) {
			uint16_t previous = s.samples[(s.oldest + s.lag - 1) % s.lag];
			new_value = s.influence * value + (1 - s.influence) * previous;
			
			status = 1;
		}
	}
	
	if(s.elements < s.lag)
		s.elements++;
	
	s.sample_sum -= s.samples[s.oldest];
	s.sample_sum += new_value;
	
	s.samples[s.oldest] = new_value;
	s.oldest = (s.oldest + 1) % s.lag;
	
	return status;
}

#endif
