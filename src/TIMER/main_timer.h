#ifndef CORE_COUNTER_H
#define CORE_COUNTER_H

#include <zephyr/kernel.h>

int core_counter_init();
int start_core_counter();
int reset_core_counter();
int set_move_alarm(uint32_t target_ticks);
int64_t get_current_ticks();
void get_current_ticks_unpacked(uint8_t *unpack_arr);
void stop_core_counter();

#endif