#ifndef PD_CONT_H
#define PD_CONT_H

#include <zephyr/kernel.h>
#include "../types.h"

// ============================================================================
// Data types
// ============================================================================
struct pdcntrl_msgq_data{Dir direction;
                         uint16_t angle; // 0 -> 360
                        };

// ============================================================================
// Variables
// ============================================================================
extern struct k_msgq err_msgq; // Error message queue defined in main to put 
                               // any thread related errors to. Defined in 
                               // Motor Driver

int init_pd_controller(int32_t (*qdec_get_angle_fn)(void));



#endif