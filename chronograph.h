#ifndef CHRONOGRAPH_H
#define CHRONOGRAPH_H

#include <libopencm3/stm32/adc.h>
#include <core/types.h>

// -------------------------------------------------

// Verbose Output
// #define DEBUG
#ifdef DEBUG
	#define DEBUG_PRINTF(...) vcp_printf(__VA_ARGS__)
#else
	#define DEBUG_PRINTF(...) ;
#endif

// Photodiode ADC channels
#define CHANNEL_FRONT ADC_CHANNEL0
#define CHANNEL_REAR ADC_CHANNEL1

// Peak Detection
#define PEAK_LAG 50
#define PEAK_THRESHOLD 80
#define PEAK_INFLUENCE 1

// Max ticks measured = TIMER_ARR
// Max time measured = TIMER_ARR/TIMER_FREQ
// One tick equals 1/TIMER_FREQ seconds
#define TIMER_ARR 0xFFFF
#define TIMER_FREQ ((int) 30e06)

// Convert timer ticks to micro seconds
#define TICKS_TO_US(ticks) ((float) (ticks) * 1e06 / TIMER_FREQ)

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
} chrono_stat_t;

// -------------------------------------------------

#endif
