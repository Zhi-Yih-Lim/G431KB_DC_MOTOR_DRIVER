#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <zephyr/kernel.h>
#include "../types.h"


// ============================================================================
// State machine instance
// ============================================================================
struct sm{
    sm_state_t state;

    // A message queue for events.
    struct k_msgq event_queue;

    // A state machine thread that is responsible for transition logic
    struct k_thread sm_thread;
    k_tid_t sm_tid;

    // A PD-controller thread that is spawned only in the MOVE state.
    k_tid_t pid_tid;
};

// ============================================================================
// Public APIs
// ============================================================================
int sm_init(struct sm *sm);
int sm_post_event(struct sm *sm, struct event_struct event_s);
sm_state_t sm_get_state(struct sm *sm);

#endif