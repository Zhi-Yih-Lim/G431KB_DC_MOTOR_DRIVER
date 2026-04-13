#include "pd_controller.h"
#include "../PWM/pwm.h" // To access 'pwm_msgq_data' 
#include <zephyr/drivers/counter.h> // To use Zephyr's counter interface
#include "../config.h" // To access "CNRTL_KP" and "CNTRL_KD" constants
                       // and PD_REFR_US for the controller's refresh rate.

// ============================================================================
// MACROS
// ============================================================================
#define PDCNTRL_THREAD_STACK_SIZE 1024 
#define PDCNTRL_THREAD_PRIORITY 6 // Set to higher priority than PWM thread.
#define PDCNTRL_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.

// ============================================================================
// Local variables
// ============================================================================
get_angle_fn_p qdec_get_angle_fn_p; // Function pointer of a function that
                                    // returns angular displacement upon
                                    // invocation.


// ============================================================================
// Forward declarations
// ============================================================================
static void pdcntrl_thread_entry(void *arg_1, void *arg_2, void *arg_3);

// ============================================================================
// PD-controller thread related
// (!) This thread is only put in running state when the Motor Driver is in
//     "MOVE" state.
// ============================================================================
// PD Controller thread id to be used in 'K_THREAD_DEFINE' below.
const k_tid_t pdcntrl_tid;

// Statically defining and initializing a thread.
// The following command spawns a thread that starts immediately.
K_THREAD_DEFINE(pdcntrl_tid,                // Name of the thread
                PDCNTRL_THREAD_STACK_SIZE,  // Stack size of thread in bytes 
                pdcntrl_thread_entry,       // Thread entry function
                NULL, NULL, NULL,           // arg_1, arg_2, and arg_3
                PDCNTRL_THREAD_PRIORITY,    // Thread priority 
                0,                          // Thread options
                0);                         // Scheduling delay

// ============================================================================
// PD-controller message queue related
// (!) Message queue to be exposed to Motor Driver module.
// ============================================================================
// Message queue variable
struct k_msgq pdcntrl_msgq;

// PD-controller message queue buffer
static char pdcntrl_msgq_buffer[PDCNTRL_MSGQ_SIZE * 
                                sizeof(struct pdcntrl_msgq_data)];

// ============================================================================
// Function definitions
// ============================================================================
/*
    Brief: Initializing function intended to be invoked by Motor Driver.

    @param get_angle: A function pointer to a function that can be invoked
                      to obtain the current angular displacement of the 
                      motor.

    @return: Initialization status following 'PDCTRL_STATUS' enum.
*/
PDCTRL_STATUS init_pd_ctrl(get_angle_fn_p get_angle){

    printk("init_pd_ctrl -> Initializing. \r\n");

    if(!get_angle){
        printk("init_pd_ctrl -> Function pointer to obtain angular"
               " displacement was NULL. \r\n");
        return INIT_ERR;
    }

    qdec_get_angle_fn_p = get_angle;

    // Verify that the PWM msgq is already initialized.
    struct pwm_msgq_data test_pwm_data = {.direction = TEST, .power = 0};

    if(k_msgq_put(&pwm_msgq, &test_pwm_data, K_NO_WAIT)){
        printk("init_pd_ctrl -> Error with PWM msgq: (%d). \r\n");
        return INIT_ERR;
    }

    // Check that the "CNTRL_KD", "CNTRL_KP" and "PD_REFR_US" are properly
    // set.
    if(!(CNTRL_KD > 0 && CNTRL_KP > 0 && PD_REFR_US > 0)){
        printk("init_pd_ctrl -> Config.h constants not within range. \r\n");
        return INIT_ERR;
    }
    // Setup sampling timer.

    return PDCTRL_NORMAL;
}

