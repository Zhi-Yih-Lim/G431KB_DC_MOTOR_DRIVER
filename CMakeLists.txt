# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(G431KB_PWM_APP)

target_sources(app PRIVATE 
	src/main.c
	src/PWM/pwm.c
	src/QDEC/qdec.c
	src/TIMER/main_timer.c
	src/CAN/fdcan.c
	src/TIMER/main_timer.c
	src/STATE_MACHINE/state_machine.c
	src/STAT_RGB/status_rgb.c
	src/PID/pid.c
	#src/UART/uart.c
)
