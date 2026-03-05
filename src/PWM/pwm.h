#ifndef PWM_H
#define PWM_H

#include <zephyr/kernel.h>

// ============================================================================
// Data types
// ============================================================================
typedef enum {STAT, CLKW, CCLKW} Dir; // Motor direction
struct pwm_msgq_data{Dir direction;
                     uint8_t pwr; // 0 -> 100
                     uint16_t angle; // 0 -> 360
                    };

// ============================================================================
// Variables
// ============================================================================
extern struct k_msgq pwm_msgq; // PWM message queue variable


// ============================================================================
// Functions
// ============================================================================
void pwm_init(); // PWM initializer function
void pwm_thread_start(void *arg_1, void *arg_2, void *arg_3); // Thread entry
                                                              // point.

#endif