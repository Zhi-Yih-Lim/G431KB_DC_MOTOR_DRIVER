#ifndef PWM_H
#define PWM_H

// Motor direction
typedef enum {STAT, CLKW, CCLKW} Dir;

// ============================================================================
// Functions
// ============================================================================

// PWM thread entry point
void pwm_thread_start(void *arg_1, void *arg_2, void *arg_3);

#endif