#ifndef PWM_H
#define PWM_H

#include <zephyr/kernel.h>

// ============================================================================
// Data types
// ============================================================================
typedef enum {STAT, CLKW, CCLKW} Dir; // Motor direction
typedef enum {PWM_NORMAL = 0, SET_PWM_ERR,
              INVALID_DIR_ERR, ERR_UNKNOWN} PWM_ERR; // Error Codes

struct pwm_msgq_data{Dir direction;
                     uint8_t power; // 0 -> 100
                     uint16_t angle; // 0 -> 360
                    };

// ============================================================================
// Variables
// ============================================================================
extern struct k_msgq pwm_msgq; // PWM message queue variable
extern struct k_msgq err_msgq; // Error message queue defined in main to put 
                               // any thread related errors to.

// ============================================================================
// Functions
// ============================================================================
void pwm_init(); // PWM initializer function
void pwm_thread_entry(void *arg_1, void *arg_2, void *arg_3); // Thread entry
                                                              // point.

#endif