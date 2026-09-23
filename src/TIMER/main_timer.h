#ifndef CORE_COUNTER_H
#define CORE_COUNTER_H

#include <zephyr/kernel.h>
#include "../types.h" // For "motor_data_t"

// Forward declaring state machine type
struct sm;

// Struct to be passed into move alarm
struct move_alarm_data{
    int (*post_event_fp)(struct sm*, struct event_struct);
    struct sm *sm_p;
    struct event_struct event_s;
};

int core_counter_init();
int start_core_counter();
int reset_core_counter();
int set_move_alarm(int (*)(struct sm*, struct event_struct),
                   struct sm *state_machine_p,
                   struct event_struct event_s);
int64_t get_current_ticks();
uint32_t get_ticks_from_us(uint64_t us);
void get_current_ticks_unpacked(uint8_t *unpack_arr);
void stop_core_counter();

#endif