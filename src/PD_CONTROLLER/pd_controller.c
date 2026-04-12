#include "pd_controller.h"
#include "../PWM/pwm.h" // To access 'pwm_msgq_data' 


// ============================================================================
// MACROS
// ============================================================================
#define PDCNTRL_THREAD_STACK_SIZE 1024 
#define PDCNTRL_THREAD_PRIORITY 6 // Set to higher priority than PWM thread.
#define PDCNTRL_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.

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

