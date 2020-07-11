#ifndef CHRONOGRAPH_H
#define CHRONOGRAPH_H

#include <libopencm3/stm32/adc.h>
#include <core/types.h>

// -------------------------------------------------

// Verbose Output
#define DEBUG

// Photodiode ADC channels
#define CHANNEL_FRONT ADC_CHANNEL0
#define CHANNEL_REAR ADC_CHANNEL1

// Peak Detection
#define PEAK_LAG 64
#define PEAK_THRESHOLD 80
#define PEAK_INFLUENCE 0

// Max ticks measured = TIMER_ARR
// Max time measured = TIMER_ARR/TIMER_FREQ
// One tick equals 1/TIMER_FREQ seconds
#define TIMER_ARR 0xFFFF
#define TIMER_FREQ ((int) 30e06)

// Convert timer ticks to micro seconds
#define TICKS_TO_US(ticks) ((float) (ticks) / TIMER_FREQ / 1e06)

// Distance of diodes (10^-6 m)
#define DISTANCE_UM 30000

// Fine-grain calibration
#define SPEED_CALIBRATION_FACTOR 1

// Used to convert m/s to fps
#define MPS_TO_FPS_FACTOR ((float) 3.2808398950131);

// -------------------------------------------------

typedef enum {
	mode_fps,
	mode_mps,
	mode_joule,
	mode_rps
} chrono_mode_t;

typedef struct {
	chrono_mode_t mode;
	int weight;
	
	float measurement;
	int count;
	
	float m_sum;
	float m_sqsum;
} chrono_stats_t;

#endif
