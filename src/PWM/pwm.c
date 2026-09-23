#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <math.h>
#include <zephyr/logging/log.h>
#include "pwm.h"
#include "../err_msgq.h"
#include "../DRIVER_CONFIG/pid_config.h"

LOG_MODULE_REGISTER(pwm, 3); // Info level

// ============================================================================
// MACROS
// ============================================================================
//#define PWM_THREAD_STACK_SIZE 1024 // Size of PWM thread on stack
//#define PWM_THREAD_PRIORITY 7 // PWM thread priority level
#define PWM4_NODE_ID DT_NODELABEL(pwm4) // Node identifer for 'pwm4' node
//#define PWM_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.

// ============================================================================
// Locally global variables
// ============================================================================
// Get device pointer from node identifier
static const struct device *const pwm4_dev = DEVICE_DT_GET(PWM4_NODE_ID);

static const uint32_t CLK_FREQ = 170000000; // Clock frequency of timer 3
static const uint32_t DRV8871_FREQ = 90000; // Desired frequency to drive the 
                                            // DRV8871.
static uint32_t PWM_PERIOD_CLK_CYCLES;// Number of clock cycles per pwm period
static uint16_t PWM_DUTY_CLK_CYCLES;// Number of clock cycles for the specified
                                    // duty cycle.

static uint8_t pwm_ready = 0; // Flag that permits the starting of the PWM
                              // thread.

// ============================================================================
// Local helper methods
// ============================================================================
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq);
static uint16_t _calc_pwm_duty_clk_cycles(uint32_t period_clk_cycle,
                                          uint8_t on_percent);
static void _init_pwm(void);

static PWM_STATUS _set_pwm(dir direction, uint8_t power);


// ============================================================================
// PWM thread related
// ============================================================================
// PWM thread id to be used in 'K_THREAD_DEFINE' below.
//const k_tid_t pwm_tid;

// Statically defining and initializing a thread.
// The following command spawns a thread that starts immediately.
//K_THREAD_DEFINE(pwm_tid,                // Name of the thread
//                PWM_THREAD_STACK_SIZE,  // Stack size of thread in bytes 
//                pwm_thread_entry,       // Thread entry function
//                NULL, NULL, NULL,       // arg_1, arg_2, and arg_3
//                PWM_THREAD_PRIORITY,    // Thread priority 
//                0,                      // Thread options
//                0);                     // Scheduling delay

// ============================================================================
// PWM message queue related
// (!) Message queue to be exposed to PD-Controller thread.
// ============================================================================
// Message queue variable
//struct k_msgq pwm_msgq;

// PWM message queue buffer
//static char pwm_msgq_buffer[PWM_MSGQ_SIZE * sizeof(struct pwm_msgq_data)];

// ============================================================================
// Function definitions
// ============================================================================

/*
    Brief: To be invoked before starting the pwm thread
*/
int pwm_init(){
    // Check to see if PWM device is ready.
    if(!device_is_ready(pwm4_dev))
    {
        LOG_ERR("Cannot find PWM3 device!\n");
        return 0;
    }
    else{
        LOG_INF("PWM device found\n");
        return 1;
    }

    _init_pwm();
    
    pwm_ready = 1;
}

int pwm_actuate(int64_t pid_output)
{
    static uint8_t power = 0;
    static dir motor_dir = STAT;

    if(pid_output < 0){
        motor_dir = CCLKW;
        power = (uint8_t)(-pid_output/(int64_t)ANG_VEL_SCALE);
        LOG_INF("Motor direction is CCLKW and power is set to %d",
                power);
    } 
    else{
        motor_dir = CLKW;
        power = (uint8_t)(pid_output/(int64_t)ANG_VEL_SCALE);
        LOG_INF("Motor direction is CLKW and power is set to %d",
                power);
    }

    switch(_set_pwm(motor_dir, power))
    {
        case PWM_NORMAL:
            LOG_INF("PWM normal operation.");
            return 1;

        case SET_PWM_ERR:
            LOG_ERR("Failed to set PWM signal.");
            return 0;

        case PWM_INVALID_DIR_ERR:
            LOG_ERR("Invalid direction command.");
            return 0;

        case PWM_ERR_UNKNOWN:
            LOG_ERR("Unknown error.");
            return 0;

        default:
            LOG_ERR("Unknown state returned by _set_pwm");
            return 0;

    }
}

// ============================================================================
// Internal functions definitions
// ============================================================================
/*
    Brief: Assign the static global variables their appropriate values.
*/
static void _init_pwm(void){

    PWM_PERIOD_CLK_CYCLES = _calc_pwm_period_clk_cycles(CLK_FREQ, 
                                                        DRV8871_FREQ);

    LOG_INF("The number of clock cycles per PWM period is %d.", PWM_PERIOD_CLK_CYCLES);

}

/*
    Brief: Calculates the number of clock cycles for one period of a 
           specified PWM frequency. 

    @param clk_freq: The clock frequency.
    @param trgt_freq: The desired output PWM frequency.

    @return The number of clock cycles for one PWM period.

*/
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq){

    // Calculate the time for one cycle of the timer's frequency.
    float timer_clk_cycle_sec = 1.0f/clk_freq;

    // Calculate the time for one period of the deisred PWM output frequency.
    float pwm_period_sec = 1.0f/trgt_freq;

    // Calculate the number of clock cycles for one PWM period, rounded up.
    // (!) Personal choice for rounding up as it is okay for one PWM period to
    //     be slightly slower rather than going too fast, hitting the lower
    //     limit of DRV8871's acceptable frequency.
    return ceil(pwm_period_sec/timer_clk_cycle_sec);

}

/*
    Brief: Calculates the number of clock cycles for a specified PWM duty 
           cycle.

    @param period_clk_cycle: Number of clock cycles for a single PWM period.
    @param on_percent: Desired percentage (in integers) of PWM high signal. 

    @return The number of clock cycles for the desired duty cycle.

*/
static uint16_t _calc_pwm_duty_clk_cycles(uint32_t period_clk_cycle,
                                          uint8_t on_percent){
    return floor(on_percent/100.0*period_clk_cycle);

}

/*
    Brief: Controls the PWM signals of PA0 and PA1 depending on the input
           arguments.

    @param direction: The intended direction of rotation of the motor 
                      (when viewed from the front side exposed shaft).
    @param power: The power of the motor (0-100).
*/
static PWM_STATUS _set_pwm(dir direction, uint8_t power){

    uint8_t _pwr = 0;
    static dir _dir = STAT; // To track previous rotation state.

    // Set a cap on the maximum power
    if(power > 100){
        LOG_WRN("Power set to be above 100, capping power to 100.");
        _pwr = 100;
    }
    else{
        _pwr = power;
    }

    // Calculate the number of signal high clock cycles based.
    PWM_DUTY_CLK_CYCLES = _calc_pwm_duty_clk_cycles(PWM_PERIOD_CLK_CYCLES, 
                                                   _pwr);

    switch(direction){
        case 0: // Stationary
            // Set PA0 and PA1 to both be ACTIVE LOW at 0% duty-cycle 
            // (inverted polarity) to enter braking mode.

            if(_dir == 1){ // Clockwise rotation, PA1 PWMed (Active Low)
                if(pwm_set_cycles(pwm4_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 0, _dir == 1," 
                            " failed to disable PA1.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 0, _dir == 1, successfully set.");

            }
            else if(_dir == 2){ // C-clockwise rotation, PA0 PWMed (Active Low)
                if(pwm_set_cycles(pwm4_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 0, _dir == 2," 
                            " failed to disable PA0.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 0, _dir == 2, successfully set.");

            }

            _dir = 0;

            break;

        case 1: // Clockwise rotation
            // Given active low operation, PWM PA1 and keep PA0 off.

            if(_dir == 0){ // Stationary
                // PWM PA1 to the desired duty cycle
                if(pwm_set_cycles(pwm4_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 1, _dir == 0," 
                            " failed to set PA1.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 1, _dir == 0, successfully set.");
                
            }
            else if(_dir == 1){// Clockwise
                // PA1 is being PWMed and PA0 is not.
                // No direction change necessary but either power or angle
                // change.
                if(pwm_set_cycles(pwm4_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 1, _dir == 1," 
                            " failed to set PA1.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 1, _dir == 1, successfully set.");
            }
            else{// Counter clockwise
                // PA0 is being PWMed and PA1 is not.
                // Issue stop command to PA0 and wait for one PWM period.
                if(pwm_set_cycles(pwm4_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 1, _dir == 2," 
                            " failed to disable PA0.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 1, _dir == 2, entering 1 cycle wait.");

                // Wait for one PWM cycle.
                k_usleep(ceil(1.0f/DRV8871_FREQ*1000000));

                // PWM PA1 to the desired duty cycle.
                if(pwm_set_cycles(pwm4_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 1, _dir == 2," 
                            " failed to set PA1.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 1, _dir == 2, successfully set.");
            }

            _dir = 1;
            
            break;

        case 2: // Counter clockwise direction
            // Given active low operation, PWM PA0 and keep PA1 off.

            if(_dir == 0){ // Stationary
                // PWM PA0 to the desired duty cycle
                if(pwm_set_cycles(pwm4_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 2, _dir == 0," 
                            " failed to set PA0.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 2, _dir == 0, successfully set.");

            }
            else if(_dir == 1){// Currently clockwise
                // PA1 is being PWMed and PA0 is not.
                // Issue stop command to PA1 and wait for one PWM period.
                if(pwm_set_cycles(pwm4_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 2, _dir == 1," 
                            " failed to disable PA1.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 2, _dir == 1, entering 1 cycle wait.");

                // Wait for one PWM cycle.
                k_usleep(ceil(1.0f/DRV8871_FREQ*1000000));

                // PWM PA0 to the desired duty cycle.
                if(pwm_set_cycles(pwm4_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 2, _dir == 1," 
                            " failed to set PA0.");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                LOG_INF("_set_pwm -> Case 2, _dir == 1, successfully set.");

            }
            else{// Currently counter-clockwise
                // PA0 is being PWMed and PA1 is not.
                // No direction change. Either power or angle change.
                if(pwm_set_cycles(pwm4_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    LOG_ERR("_set_pwm -> Case 2, _dir == 2," 
                            " failed to set PA0.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }
                LOG_INF("_set_pwm -> Case 2, _dir == 2, successfully set.");
            }

            _dir = 2;

            break;

        default:
            LOG_ERR("_set_pwm -> Default Case.");
            // Turn PWM off to both PA0 and PA1 to keep signals 
            // of both channels high.
            pwm_set_cycles(pwm4_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                           0, PWM_POLARITY_INVERTED);
            pwm_set_cycles(pwm4_dev, 2, PWM_PERIOD_CLK_CYCLES,
                           0, PWM_POLARITY_INVERTED);
            _dir = 0;
            return PWM_INVALID_DIR_ERR;
    }

    // Indicate successful pwm setting
    return PWM_NORMAL;

}