#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Kp and Kd constants for PD controller
const int32_t CNTRL_KP = 10;
const int32_t CNTRL_KD = 10;

// Refresh rate of PD controller
const uint32_t PD_REFR_US = 100;

#endif