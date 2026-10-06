#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration
struct sm;

typedef struct pid{
    // Gains represented in Q16.16 format
    int32_t kp, ki, kd;
    
    // Accumulated error for calculating "i_term"
    int64_t accum_err;

    // Previous error for derivative term in "ANG_VEL_SCALE"
    int64_t prev_err_scaled;
    bool has_prev_err;

    // Output clamp
    int64_t output_min, output_max;

    // Task watchdog channel
    int task_wd_chan;

} pid_t;

int pid_init(pid_t *pid, struct sm *machine);
void pid_reset(pid_t *pid);
int64_t pid_clamp(int64_t val, int64_t lo, int64_t hi);
int64_t pid_update(pid_t *pid, int64_t target_scaled,
                   int64_t measured_scaled, uint32_t elapsed_us);


#endif