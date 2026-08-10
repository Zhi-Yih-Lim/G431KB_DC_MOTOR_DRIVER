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
    uint32_t trgt_counter;
    dir direction;
    uint16_t angular_vel; // rad/sec
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

#endif