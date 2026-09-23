#ifndef QDEC_H
#define QDEC_H

#include <zephyr/drivers/sensor.h>
// ============================================================================
// Functions
// ============================================================================
int qdec_init(); // QDEC initializer function
int qdec_read_angle(struct sensor_value *deg); // Perform a readout on the displaced angle

#endif