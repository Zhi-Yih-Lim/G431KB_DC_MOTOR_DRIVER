#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <zephyr/kernel.h>

// ============================================================================
// States
// ============================================================================
typedef enum{
    SM_STATE_IDLE = 0,
    SM_STATE_COUNTER_RESET,
    SM_STATE_MOVE,
    SM_STATE_ERROR
} sm_state_t;

// ============================================================================
// Events
// ============================================================================
typedef enum{
    SM_EVENT_COUNTER_RESET = 0, // CAN counter reset command
    SM_EVENT_MOVE,              // CAN Move command
    SM_EVENT_ERROR
} sm_event_t;

// ============================================================================
// State machine instance
// ============================================================================
typedef struct {
    sm_state_t state;

    // A message queue for events.
    struct k_msgq event_queue;

    // A state machine thread that is responsible for transition logic
    struct k_thread sm_thread;
    k_tid_t sm_tid;

    // A PD-controller thread that is spawned only in the MOVE state.

} sm_t;

// ============================================================================
// Public APIs
// ============================================================================
int sm_init(sm_t *sm);
int sm_post_event(sm_t *sm, sm_event_t event);
sm_state_t sm_get_state(sm_t *sm);

#endif