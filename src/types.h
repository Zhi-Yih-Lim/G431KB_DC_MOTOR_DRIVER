#ifndef TYPES_H
#define TYPES_H

// Motor direction
typedef enum {STAT, CLKW, CCLKW, TEST} dir; // TEST use for testing msgq.

// Message type
typedef enum {CMD, RQST} can_msg_type;

// Functionality of interest
typedef enum {F_MOTOR, F_COUNTER, F_ENCODER} func_contxt;

// ============================================================================
// Motor data
// ============================================================================
typedef struct {
    uint32_t trgt_ticks;
    dir direction; // 0 for C.C.W, 1 for C.W
    float angular_vel; // rad/sec. Max at 145 rpm or 15.1844 rad/s
} motor_data_t;

// ============================================================================
// CAN raw data union
// ============================================================================
typedef union {
    uint32_t counter_ticks;
    int32_t angular_velocity; // rad/sec
    // The following is an anonymous struct type. No way to refer to it
    // elsewhere - it exists only for this member 'angular_position'.
    struct {int16_t degrees; int16_t no_of_rev;} angular_position_s;
} can_raw_data_u;


// ============================================================================
// States
// ============================================================================
typedef enum{
    SM_STATE_IDLE = 0,
    SM_STATE_COUNTER_RESET,
    SM_STATE_MOVE,
    SM_STATE_GET_TICKS,
    SM_STATE_ERROR
} sm_state_t;

// ============================================================================
// Events
// ============================================================================
typedef enum{
    SM_EVENT_COUNTER_RESET = 0, // CAN counter reset command
    SM_EVENT_MOVE,              // Motor move command from alarm
    SM_EVENT_SCHED_MOVE,        // CAN Move command received
    SM_EVENT_SEND_TICKS,
    SM_EVENT_SEND_ENCODER,
    SM_EVENT_STOP_MOTOR,
    SM_EVENT_ERROR
} sm_event_t;

// Wrapper around the event enum for user to include custom data on each event 
// call.
struct event_struct {
    sm_event_t event;
    void *user_data;
};

#endif