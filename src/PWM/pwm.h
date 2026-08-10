#ifndef PWM_H
#define PWM_H

#include <zephyr/kernel.h>
#include "../types.h"

// ============================================================================
// Data types
// ============================================================================
typedef enum {PWM_NORMAL = 0, SET_PWM_ERR,
              PWM_INVALID_DIR_ERR, 
              PWM_ERR_UNKNOWN} PWM_STATUS; // Status Codes

struct pwm_msgq_data{dir direction;
                     uint8_t power; // 0 -> 100 duty cycle
                    };

// ============================================================================
// Variables
// ============================================================================
extern struct k_msgq pwm_msgq; // PWM message queue variable
extern struct k_msgq err_msgq; // Error message queue defined in main to put 
                               // any thread related errors to. Defined in
                               // Motor Driver

// ============================================================================
// Functions
// ============================================================================
void pwm_init(); // PWM initializer function

#endif