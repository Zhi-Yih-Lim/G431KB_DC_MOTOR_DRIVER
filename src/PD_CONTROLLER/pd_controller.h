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

typedef int32_t (*get_angle_fn_p)(void); // Function pointer that points to 
                                         // QDEC function that retrieves 
                                         // angular displacement upon 
                                         // invocation.

typedef enum {PDCTRL_NORMAL = 0, INIT_ERR} PDCTRL_STATUS; // Status Codes

// ============================================================================
// Variables
// ============================================================================
extern struct k_msgq err_msgq; // Error message queue defined in main to put 
                               // any thread related errors to. Defined in 
                               // Motor Driver

// ============================================================================
// Functions
// ============================================================================
int init_pd_ctrl(int32_t (*)(void));



#endif