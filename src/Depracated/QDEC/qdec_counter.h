#ifndef QDEC_COUNTER_H
#define QDEC_COUNTER_H

#include <zephyr/kernel.h>

void qdec_counter_init(uint32_t readout_period_us);
void start_qdec_counter();
void stop_qdec_counter();

#endif