#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <math.h>
#include "pwm.h"
#include "../err_msgq.h"

// ============================================================================
// MACROS
// ============================================================================
#define PWM_THREAD_STACK_SIZE 1024 // Size of PWM thread on stack
#define PWM_THREAD_PRIORITY 7 // PWM thread priority level
#define PWM3_NODE_ID DT_NODELABEL(pwm3) // Node identifer for 'pwm3' node
#define PWM_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.


// ============================================================================
// Locally global variables
// ============================================================================
// Get device pointer from node identifier
static const struct device *const pwm3_dev = DEVICE_DT_GET(PWM3_NODE_ID);

static const uint32_t CLK_FREQ = 170000000; // Clock frequency of timer 3
static const uint32_t DRV8871_FREQ = 90000; // Desired frequency to drive the 
                                            // DRV8871.
static uint32_t PWM_PERIOD_CLK_CYCLES;// Number of clock cycles per pwm period
static uint16_t PWM_DUTY_CLK_CYCLES;// Number of clock cycles for the specified
                                    // duty cycle.
static uint8_t PA6_S; // Indicates whether pin PA6 is being PWMed (ACTIVE LOW)
static uint8_t PA7_S; // Indicates whether pin PA7 is being PWMed (ACTIVE LOW)

static const uint32_t pwm_thread_sleep_ms = 500; // Sleep period for the PWM
                                                 // thread, determines how 
                                                 // fast motion commands gets
                                                 // updated.

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

static PWM_ERR _set_pwm(Dir direction, uint8_t power, uint16_t angle);


// ============================================================================
// PWM thread related
// ============================================================================
// PWM thread id to be used in 'K_THREAD_DEFINE' below.
extern const k_tid_t pwm_tid;

// Statically defining and initializing a thread.
K_THREAD_DEFINE(pwm_tid,                // Name of the thread
                PWM_THREAD_STACK_SIZE,  // Stack size of thread in bytes 
                pwm_thread_start,       // Thread entry function
                NULL, NULL, NULL,       // arg_1, arg_2, and arg_3
                PWM_THREAD_PRIORITY,    // Thread priority 
                0,                      // Thread options
                0);                     // Scheduling delay

// ============================================================================
// PWM message queue related
// ============================================================================
// Message queue variable
struct k_msgq pwm_msgq;

// PWM message queue buffer
static char pwm_msgq_buffer[PWM_MSGQ_SIZE * sizeof(struct pwm_msgq_data)];

// ============================================================================
// Function definitions
// ============================================================================

/*
    Brief: To be invoked before starting the pwm thread
*/
void pwm_init(){
    // Check to see if PWM device is ready.
    if(!device_is_ready(pwm3_dev))
    {
        printk("Cannot find PWM3 device!\n");
        return;
    }
    else{
        printk("PWM device found\n");
    }

    _init_pwm();

    // Initialize PWM message queue
    k_msgq_init(&pwm_msgq, pwm_msgq_buffer, 
                sizeof(struct pwm_msgq_data), PWM_MSGQ_SIZE);
    
    pwm_ready = 1;
}

/*
    Brief: PWM thread entry point intended to be invoked in main.

    @param arg_1 -> 3: Optional arguments to be passed into the thread.
*/
void pwm_thread_start(void *arg_1, void *arg_2, void *arg_3){
    
    while(!pwm_ready){
        printk("pwm_thread_start :: PWM device is not ready. \n");
        k_msleep(1000);
    }

    int ret = 0;
    Dir pwm_dir = STAT;

    struct pwm_msgq_data msgq_data;
    struct err_msgq_data err_msgq_data;

    // Start off the PWM in braking mode
    _set_pwm(pwm_dir, 0, 0);        

    printk("pwm_thread_start :: Entering while loop.\n");

    while(!ret){
        // Fetch a data item from the message queue.
        k_msgq_get(&pwm_msgq, &msgq_data, K_FOREVER);

        printk("pwm_thread_start :: Data fetched from PWM msgq with contents"
               "direction = %d, power = %d, and angle = %d",
               msgq_data.direction, msgq_data.power,
               msgq_data.angle);

        ret = (int)_set_pwm(msgq_data.direction, 
                            msgq_data.power,
                            msgq_data.angle);

        k_msleep(pwm_thread_sleep_ms);
    }

    // Set relevant thread id and error number.
    err_msgq_data.thread = PWM;
    err_msgq_data.err_no = ret;

    k_msgq_put(&err_msgq, &err_msgq_data, K_NO_WAIT);

    // (!) PWM thread termintates by returning.

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

    printk("_init_pwm :: The number of clock cycles per PWM period is %d\n", PWM_PERIOD_CLK_CYCLES);

    PA6_S = PA7_S = 0;

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
    Brief: Controls the PWM signals of PA6 and PA7 depending on the input
           arguments.

    @param direction: The intended direction of rotation of the motor 
                      (when viewed from the front side exposed shaft).
    @param power: The power of the motor (0-100).
    @param angle: The target angular displacement (0-360)
*/
static PWM_ERR _set_pwm(Dir direction, uint8_t power, uint16_t angle){

    ARG_UNUSED(angle); // TODO: To be implemented with qdec.

    uint8_t _pwr = 0;
    static Dir _dir = 0; // To track previous rotation state.

    // Set a cap on the maximum power
    if(power > 100){
        printk("_set_pwm :: Power set to be above 100, capping power to 100.\n");
        _pwr = 100;
    }
    else if(power < 0){
        printk("_set_pwm :: Power set to be below 100, setting power to 0.\n");
        _pwr = 0;
    }
    else{
        _pwr = power;
    }

    // Calculate he number of signal high clock cycles based.
    PWM_DUTY_CLK_CYCLES = _calc_pwm_duty_clk_cycles(PWM_PERIOD_CLK_CYCLES, 
                                                   _pwr);

    switch(direction){
        case 0: // Stationary
            // Set PA6 and PA7 to both be ACTIVE LOW at 0% duty-cycle 
            // (inverted polarity) to enter braking mode.

            if(_dir == 1){ // Clockwise rotation, PA7 PWMed (Active Low)
                if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 0, _dir == 1," 
                           " failed to disable PA7.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 0, _dir == 1, successfully set.\n");

                PA7_S = 0;
            }
            else if(_dir == 2){ // C-clockwise rotation, PA6 PWMed (Active Low)
                if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 0, _dir == 2," 
                           " failed to disable PA6.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 0, _dir == 2, successfully set.\n");

                PA6_S = 0;
            }

            _dir = 0;

            break;

        case 1: // Clockwise rotation
            // Given active low operation, PWM PA7 and keep PA6 off.

            if(_dir == 0){ // Stationary
                // PWM PA7 to the desired duty cycle
                if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 1, _dir == 0," 
                           " failed to set PA7.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 1, _dir == 0, successfully set.\n");
                
            }
            else if(_dir == 2){// Counter clockwise
                // PA6 is being PWMed and PA7 is not.
                // Issue stop command to PA6 and wait for one PWM period.
                if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 1, _dir == 2," 
                           " failed to disable PA6.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 1, _dir == 2, entering 1 cycle wait.\n");

                // Wait for one PWM cycle.
                k_usleep(ceil(1.0f/DRV8871_FREQ*1000000));

                PA6_S = 0;

                // PWM PA7 to the desired duty cycle.
                if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 1, _dir == 2," 
                           " failed to set PA7.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 1, _dir == 2, successfully set.\n");
            }

            PA7_S = 1;

            _dir = 1;
            
            break;

        case 2: // Counter clockwise direction
            // Given active low operation, PWM PA6 and keep PA7 off.

            if(_dir == 0){ // Stationary
                // PWM PA6 to the desired duty cycle
                if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 2, _dir == 0," 
                           " failed to set PA6.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 2, _dir == 0, successfully set.\n");

            }
            else if(_dir == 1){// Currently clockwise
                // PA7 is being PWMed and PA6 is not.
                // Issue stop command to PA7 and wait for one PWM period.
                if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                               0, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 2, _dir == 1," 
                           " failed to disable PA7.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 2, _dir == 1, entering 1 cycle wait.\n");

                // Wait for one PWM cycle.
                k_usleep(ceil(1.0f/DRV8871_FREQ*1000000));

                PA7_S = 0;

                // PWM PA6 to the desired duty cycle.
                if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES,
                               PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                    printk("pwm :: _set_pwm -> Case 2, _dir == 1," 
                           " failed to set PA6.\n");
                    
                    // TODO: Cut power supply to motors ??

                    return SET_PWM_ERR;
                }

                printk("_set_pwm -> Case 2, _dir == 1, successfully set.\n");

            }

            PA6_S = 1;

            _dir = 2;

            break;

        default:
            printk("_set_pwm -> Default Case.\n");
            // Turn PWM off to both PA6 and PA7 to keep signals 
            // of both channels high.
            pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                           0, PWM_POLARITY_INVERTED);
            pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                           0, PWM_POLARITY_INVERTED);
            PA6_S = 0;
            PA7_S = 0;
            _dir = 0;
            break;
    }

    // Indicate successful pwm setting
    return 0;

}