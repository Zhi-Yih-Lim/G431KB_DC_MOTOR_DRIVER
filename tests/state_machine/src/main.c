#include <string.h>
#include <zephyr/fff.h> // Zephyr's Fake Function Framework.
#include <zephyr/ztest.h>

// To access the private "_sm_thread_dispatch()" method that
// handles state machine transitions.
#include "../../../src/STATE_MACHINE/state_machine.c"

// Defines a shared storage FFF uses to track fake function calls.
DEFINE_FFF_GLOBALS;

// Test reception of "SM_EVENT_SEND_TICKS" in IDLE state.
// Fake functions
// "set_status_rgb()"
// "get_current_ticks_unpacked()"
// "fd_can_send()"

// Required variables
// "struct sm *"