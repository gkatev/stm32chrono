#include <stdint.h>
#include <string.h>

#include "peak.h"

void peak_stat_init(struct peak_stat *s, uint threshold,
		uint influence, uint lag, uint training, uint16_t *samples) {
	
	*s = (struct peak_stat) {
		.threshold = threshold,
		.influence = influence,
		.lag = lag,
		.training = training,
		.samples = samples
	};
	
	memset(samples, 0, lag * sizeof(samples[0]));
}

void peak_stat_reset(struct peak_stat *s) {
	s->elements = 0;
	s->oldest = 0;
	s->sample_sum = 0;
	
	memset(s->samples, 0, s->lag * sizeof(s->samples[0]));
}

/* int peak_detect(struct peak_stat *s, uint16_t value) {
	uint16_t new_value = value;
	int status = 0;
	
	if(s->elements >= s->training) {
		uint16_t average = s->sample_sum / s->elements;
		
		if(value > average && (uint)(value - average) > s->threshold) {
			uint16_t previous = s->samples[(s->oldest + s->lag - 1) % s->lag];
			new_value = s->influence * value + (1 - s->influence) * previous;
			
			status = 1;
		}
	}
	
	if(s->elements < s->lag)
		s->elements++;
	
	s->sample_sum -= s->samples[s->oldest];
	s->sample_sum += new_value;
	
	s->samples[s->oldest] = new_value;
	s->oldest = (s->oldest + 1) % s->lag;
	
	return status;
} */
