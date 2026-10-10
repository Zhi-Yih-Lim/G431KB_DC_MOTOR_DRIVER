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

/**
 * Initializes the state machine instance.
 *
 * @param sm Pointer to a state machine instance. Must not be NULL.
 * @return 1 on success, 0 if initialization fails (e.g., invalid pointer
 *         or thread creation failure).
 */
int sm_init(struct sm *sm);

/**
 * Posts an event to the state machine's event queue.
 *
 * Events are delivered asynchronously to the state machine thread for
 * processing. The function uses a non-blocking put operation; if the queue
 * is full, the event is discarded and an error is returned.
 *
 * @param sm Pointer to a state machine instance. Must not be NULL.
 * @param event_s The event to post. The caller retains ownership of any
 *                memory referenced by the event structure.
 * @return 1 on success, 0 if the event could not be queued (e.g., invalid
 *         pointer or full queue).
 */
int sm_post_event(struct sm *sm, struct event_struct event_s);

/**
 * Returns the current state of the state machine.
 *
 * @param sm Pointer to a state machine instance. Must not be NULL.
 * @return The current state, or SM_STATE_ERROR if sm is NULL.
 */
sm_state_t sm_get_state(struct sm *sm);

#endif