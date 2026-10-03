#include <string.h>
#include <zephyr/fff.h> // Zephyr's Fake Function Framework.
#include <zephyr/ztest.h>
#include <zephyr/sys/byteorder.h>

// To access the private "_sm_thread_dispatch()" method that
// handles state machine transitions.
#include "../../../src/STATE_MACHINE/state_machine.c"

// Defines a shared storage FFF uses to track fake function calls.
DEFINE_FFF_GLOBALS;

// Test reception of "SM_EVENT_SEND_TICKS" in IDLE state.
// Fake function dependencies required by '_sm_get_ticks_entry()'
// "set_status_rgb()"
// "get_current_ticks_unpacked()"
// "fd_can_send()"

// Required variables
// "struct sm *"

typedef int (*post_event_fp_t)(struct sm*, struct event_struct);

// ============================================================================
// Fake functions
// ============================================================================
/*
    FAKE_VALUE_FUNC(rtrn_type, fake_fn_name, arg0, arg1, ...)

    (!) Need to create fake functions for all external function dependencies
        of "state_machine.c"
*/
FAKE_VALUE_FUNC(int, set_status_rgb, sm_state_t);
FAKE_VALUE_FUNC(int, reset_core_counter);
FAKE_VALUE_FUNC(int64_t, get_current_ticks);
FAKE_VALUE_FUNC(uint32_t, get_ticks_from_us, uint64_t);
FAKE_VOID_FUNC(get_current_ticks_unpacked, uint8_t *);
FAKE_VALUE_FUNC(int, fd_can_send, const uint8_t *, char *);
FAKE_VOID_FUNC(pid_init, pid_t *);
FAKE_VALUE_FUNC(int64_t, pid_update, pid_t *, int64_t, int64_t, uint32_t);
FAKE_VALUE_FUNC(int, pwm_actuate, int64_t);
FAKE_VALUE_FUNC(int, qdec_read_angle, struct sensor_value *);
FAKE_VALUE_FUNC(int, set_move_alarm, post_event_fp_t, struct sm *,
                struct event_struct);
// ============================================================================
// Statically global variables
// ============================================================================
static struct sm machine;

// ============================================================================
// Functions
// ============================================================================
/*
    @brief: A mock function to simulate response for 
            "get_current_ticks_unpacked()"
*/
static void fake_get_current_ticks_unpacked(uint8_t * arr)
{
    arr[0] = 0x12;
    arr[1] = 0x34;
    arr[2] = 0x56;
    arr[3] = 0x78;
}
/*
    @brief: A function triggering the reception of an "SM_EVENT_SEND_TICKS"
            event by "_sm_thread_dispatch()"
*/
static void trigger_ticks_command(void)
{
    _sm_thread_dispatch(&machine, (struct event_struct){
        .event = SM_EVENT_SEND_TICKS,
        .user_data = NULL,
    });
}


/*
    @brief: A resetting function to be called before each unit test.
*/

static void before(void *fixture)
{
    ARG_UNUSED(fixture);
    
    // Reset struct members of fake functions
    RESET_FAKE(set_status_rgb);
    RESET_FAKE(get_current_ticks_unpacked);
    RESET_FAKE(fd_can_send);

    // Initialize the state machine to be IDLE.
    // The reset can be left un-initialized.
    machine.state = SM_STATE_IDLE;

    // Link "get_current_ticks_unpacked_fake" to 
    // "fake_get_current_ticks_unpacked()"
    get_current_ticks_unpacked_fake.custom_fake = 
        fake_get_current_ticks_unpacked;

    // Set return values from fake functions
    fd_can_send_fake.return_val = 1;
}

// ============================================================================
// Unit Tests
// ============================================================================
ZTEST(state_machine, test_idle_to_send_ticks_on_ticks_command)
{
    uint8_t action_arr[3] = {0};
    uint8_t data[4];
    uint8_t ticks_arr[4];
    uint32_t ticks;
    uint32_t ticks_ref = 0x12345678;

    // See if the state machine is originally in "IDLE" state
    zassert_equal(sm_get_state(&machine), SM_STATE_IDLE,
                  "Precondition: state must be IDLE.");

    // Simulate reception of "send_ticks" command
    trigger_ticks_command();

    // Check to see if the final state is "SM_STATE_IDLE"
    zassert_equal(sm_get_state(&machine), SM_STATE_IDLE,
                  "State must be IDLE after ticks event.");

    // Check that the fake "set_status_rgb" was invoked and that the 
    // received state was "SM_STATE_GET_TICKS"
    zassert_equal(set_status_rgb_fake.call_count, 2,
                  "Status rgb must be called.");

    // The first call to "set_status_rgb" should be for the state 
    // "SM_STATE_GET_TICKS"
    zassert_equal(set_status_rgb_fake.arg0_history[0], SM_STATE_GET_TICKS,
                  "Set status rgb function must receive SM_STATE_GET_TICKS.");
    
    // The second call to "set_status_rgb" should be for the state
    // "SM_STATE_IDLE"
    zassert_equal(set_status_rgb_fake.arg0_history[1], SM_STATE_IDLE,
                  "Set status rgb function must receive SM_STATE_IDLE.");

    // Verify the contents of "can_out_data_arr"
    zassert_equal(fd_can_send_fake.call_count, 1,
                 "fd_can_send should be inovked.");

    zassert_equal(fd_can_send_fake.arg0_val[0], CENTRAL_CAN_ID,
                  "First element of output array should be the central CAN ID.");

    memcpy(action_arr, (fd_can_send_fake.arg0_val)+1, 2);
    action_arr[2] = '\0';

    zassert_str_equal(action_arr, "TT", 
                      "Action command must be \"TT\"");

    memcpy(data, (fd_can_send_fake.arg0_val)+3, 4);

    zassert_mem_equal(data, (uint8_t[4]){0}, 4,
                      "Data section must be zeros.");

    memcpy(ticks_arr, (fd_can_send_fake.arg0_val)+7, 4);
    
    // 'sys_get_be32' reads 4 bytes from a source array and returns a uint32_t.
    ticks = sys_get_be32(ticks_arr);

    zassert_equal(ticks, ticks_ref,
                  "Ticks extracted from array sent not equal to intended val.");


}


/* Define the test suite */
ZTEST_SUITE(state_machine, NULL, NULL, before, NULL, NULL);