#ifndef PID_CONFIG_H
#define PID_CONFIG_H

// PID constants
#define KP 10.0
#define KD 10.0
#define KI 1.0

// Scaling the "integer" part of the sensor 
// value so that it can be added directly to the 
// fractional part (one-millionth part.)
#define ANG_VEL_SCALE 1000000LL 

// Clamping for PWM contributions from I term
#define I_TERM_MAX 50 * ANG_VEL_SCALE // Scaled to same scale as errors
#define I_TERM_MIN -50 * ANG_VEL_SCALE

// Gain scale for PID constants followng Q16.16 where 
// the lowest 16-bits of the 32-bit integer represents the 
// decimal part of a float number and the top 16-bits 
// represent the integer part.
#define PID_GAIN_SHIFT 16 
#define PID_GAIN_SCALE (1LL << PID_GAIN_SHIFT) // Scales float up by 65536 or
                                               // 2^16
// Refresh rate of PD controller
#define PD_REFR_US 100

// Max duration the motors can keep moving without receiving MOVE
// command from Central Control Unit 
#define PID_WD_TIMEOUT_mS 1 // milliseconds

#define PD_THREAD_STACK_SIZE 1024
#define PD_THREAD_PRIORITY 3 // Highest priority under main.

#endif