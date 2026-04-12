#ifndef ERR_MSGQ_H
#define ERR_MSGQ_H


#include <zephyr/kernel.h>

/*
    Used for exposing the 'err_msgq_data' struct to be used as a data item for
    the 'err_msgq' defined in main.c. This message queue can be used by other
    threads to put their errors onto.
*/

// ============================================================================
// Custom data types
// ============================================================================
typedef enum {PWM = 0, 
              QDEC, 
              CAN, 
              RGB, 
              PD_CNTRL} m_thread_id; // ID to identify the running threads.

// Data item for Error message queue
struct err_msgq_data{m_thread_id thread;
                     uint32_t err_no; // Refer to error enums of different 
                                      // threads.
                    };


#endif