#include "pid.h"
#include "../STATE_MACHINE/state_machine.h" // For "sm_post_event"
#include "../DRIVER_CONFIG/pid_config.h"
#include "../TASK_WATCHDOG/t_watchdog.h"
#include "../types.h"
#include <math.h> // For "llround"

// Custom struct definitions
struct pid_wd_timeout_data_s{
    struct sm *sm_p;
    pid_t pid;
};

// Forward declarations
void _pid_wd_timeout_cback(int chan_id, void *usr_data);

// Helper function that converts a doubled gain value into Q16.16.
static inline int32_t scaled_pid_gain_from_double(double gain){
    return (int32_t)llround(gain * (double)PID_GAIN_SCALE);
}

void pid_init(pid_t *pid, struct sm *machine)
{
    // (!) Requires prior initialization of Zephyr's task watchdog.
    pid->ki = scaled_pid_gain_from_double(KI);
    pid->kd = scaled_pid_gain_from_double(KP);
    pid->kp = scaled_pid_gain_from_double(KD);

    pid->accum_err = 0;

    pid->prev_err_scaled = 0;
    pid->has_prev_err = false;

    pid->output_min = -100000000LL;
    pid->output_max = 100000000LL;

    // TODO: Need to figure out if user data is copied entirely or only by pointer.
    struct pid_wd_timeout_data_s temp_s = {.sm_p = machine, .pid = pid};

    pid->task_wd_chan = task_wdt_add(PID_WD_TIMEOUT_mS, _pid_wd_timeout_cback, 
                                     (void *)machine);

    if(pid->task_wd_chan < 0){
        LOG_ERR("Failed to add pid task watchdog channel, Error %d", pid->task_wd_chan);
        return 0;
    }

    return 1;

}

void pid_reset(pid_t *pid)
{
    pid->accum_err = 0;
    pid->prev_err_scaled = 0;
    pid->has_prev_err = false;
    delete_t_wd(pid->task_wd_chan);
}


int64_t pid_clamp(int64_t val, int64_t lo, int64_t hi)
{
    if(val < lo){
        val = lo;
    }
    if(val > hi){
        val = hi;
    }
    return val;
}

int64_t pid_update(pid_t *pid, int64_t target_scaled,
                   int64_t measured_scaled, uint32_t elapsed_us)
{
    // Calculate the error
    int64_t error_scaled = target_scaled - measured_scaled;
    
    // Calculate the proportional term (in ANG_VEL_SCALED units)
    int64_t p_term = (error_scaled * (int64_t)pid->kp) >> PID_GAIN_SHIFT;

    // Update the accumulated error
    pid->accum_err += error_scaled * (int64_t)elapsed_us;
    
    // Calculate the integral term (in ANG_VEL_SCALED units)
    int64_t i_term = (pid->accum_err * (int64_t)pid->ki)/
                     (PID_GAIN_SCALE * 100000000LL);

    // Check to see if "i_term" is beyond the acceptable threshold
    int64_t i_term_clamped = pid_clamp(i_term, I_TERM_MIN, I_TERM_MAX);

    // If "i_term" was clamped, that means that the accumulated error producing
    // the initially computed "i_term" had been "wound-up". We will re-calibrate
    // the accumulated error to be a value that produces "i_term_clamped" so that 
    // the next computation of "i_term" will not be over-wound again.
    if(i_term_clamped != i_term){
        // Back-calculation for "accum_err"
        pid->accum_err = (i_term_clamped * PID_GAIN_SCALE * 100000000LL)/(int64_t)pid->ki;
    }

    i_term = i_term_clamped;

    // (!) "p_term" left uncalculated for now.

    // Combine output
    int64_t output = p_term + i_term;

    // Clamp the output if need be.
    return pid_clamp(output, pid->output_min, pid->output_max);

}


// Internal functions
void _pid_wd_timeout_cback(int chan_id, void *usr_data)
{
    LOG_ERR("PID watchdog timer timed out.");

    struct sm *sm_p = (struct sm *)usr_data;
    struct event_struct event_s = {.event = SM_EVENT_PID_WD_TIMEOUT};

    sm_post_event(sm_p, event_s);

}