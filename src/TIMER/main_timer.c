#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h> // To use Zephyr's counter interface
#include <zephyr/drivers/gpio.h> // To control the Core Timer's LED
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "main_timer.h" 

LOG_MODULE_REGISTER(core_counter, 3); // Info level

// Flag to see if the counter device has been properly set up.
// Failure prevents counter from starting.
static int core_counter_ready = 0;

// Work Items
static struct k_work change_led_work;
// The following embeds the work item to be put on the system
// work queue when the motor alarm triggers into a larger structure
// that includes the function pointer to the method that will 
// transition the main State Machine into the "MOVE" state.
// When the work handler is triggered, the function pointer can 
// be accessed from within the work handler through the use of 
// "CONTAINER_OF."
static struct motor_alarm_fn_pntr_bundle{
    struct k_work change_state_machine_work;
    // TODO: Function pointer to change machine's state.
} motor_alarm_bundle_inst; // Creates an instance of the struct.

// Alarm configuration structs
static struct counter_alarm_cfg led_alarm_cfg, motor_alarm_cfg;

// TODO: A function pointer that is passed in during the initialization
//       of the counter that serves to change the state of the main 
//       stae machine.

#define LED_TOGGLE_PERIOD_US 1000000 // Micro seconds between LED toggling
#define MIN_MOTOR_ALARM_DELTA_TICKS 50 // The minimum amount of difference in 
                                       // ticks allowable between the current 
                                       // ticks when "set_move_alarm()" was 
                                       // invoked and the target tick value
                                       // of the alarm.
#define CORE_COUNTER_NODE_ID DT_NODELABEL(timer2_counter) // Node identifer for 
                                                          // timer 2's counter
#define CORE_COUNTER_LED_ID DT_NODELABEL(timer_led) // Node identifier for core
                                                    // timer's indicating LED
#define LED_ALRM_CHAN_ID 0 // Channel for LED toggling alarm
#define MOTOR_ALRM_CHAN_ID 1 // Alarm for changing the state machine into 
                             // "MV" state.
// Alarm will fire at N ticks from now and will
// invoke the associated callback immediately upon detection of 
// a late alarm.
#define LED_ALARM_FLAGS COUNTER_ALARM_CFG_EXPIRE_WHEN_LATE                           
#define MOTOR_ALARM_FLAGS COUNTER_ALARM_CFG_EXPIRE_WHEN_LATE

// Get device pointer from node identifier
static const struct device *core_cntr_dev = DEVICE_DT_GET(
                                                CORE_COUNTER_NODE_ID);
static const struct gpio_dt_spec timer_led = GPIO_DT_SPEC_GET(
                                                CORE_COUNTER_LED_ID,
                                                gpios);

static uint8_t core_timer_led_state = 0; // 1: On, 0: Off

static uint32_t ttl_ticks_per_toggle_period;

//=============================================================================
// Forward Declarations
//=============================================================================
void stop_core_counter();
void _cancel_all_alarms();
void _change_led_work_handler(struct k_work *work);
void _change_state_machine_work_handler(struct k_work *work);

// ============================================================================
// Function Definitions
// ============================================================================

/*
    Brief: A callback to service the timeout of the LED's alarm. Callback first
           uses the number of ticks passed into this callback to calculate
           the ticks for the next alarm. It then toggles the LED's gpio pin 
           state and subtracts off the delay - in ticks - taken to reach the
           end of the function, from the ticks for the next alarm before
           proceeding to set the alarm.

    @param: Follows Zephyr's "counter_alarm_callback_t" typedef,

    @return: None.
*/
static void led_alarm_cback(const struct device *dev, 
                            uint8_t chan_id, uint32_t ticks, 
                            void *user_data)
{
    uint32_t ticks_on_nxt_alarm = ticks + ttl_ticks_per_toggle_period;
    uint32_t current_ticks;
    uint64_t useconds;
    int ret;

    if(!core_counter_ready){
        LOG_ERR("Core counter is not ready.");
        return;
    }

    LOG_INF("In LED alarm callback, current ticks are %u.", ticks);
    
    struct counter_alarm_cfg *alrm_confg = 
        (struct counter_alarm_cfg *)user_data;
    
    // Submit LED toggling work to system work queue
    k_work_submit(&change_led_work);
    
    // Get the current ticks
    ret = counter_get_value(dev, &current_ticks);

    if (ret < 0){
        LOG_ERR("Error (%d), failed to get the current number of ticks.", ret);
        // Stop blinking the LEDs
        core_counter_ready = 0;
        return;
    }
    else{
        LOG_INF("The current ticks is %u", current_ticks);
    }

    LOG_INF("Ticks on next alarm will be %u.", ticks_on_nxt_alarm);

    // Configure and set the next alarm
    alrm_confg->ticks = ticks_on_nxt_alarm - current_ticks;

    ret = counter_set_channel_alarm(dev, LED_ALRM_CHAN_ID,
					alrm_confg);

	if (ret != 0) {
		LOG_ERR("Alarm could not be set.");
        core_counter_ready = 0;
        return;
	}

    LOG_INF("Next alarm will trigger in %u ticks", alrm_confg->ticks);
}

// Callback for motor move alarm
static void motor_alarm_cback(const struct device *dev, 
                              uint8_t chan_id, uint32_t ticks, 
                              void *user_data)
{
    LOG_INF("Submitting work to change the state machine's state \
            to MOVE on the system work queue.");
    // Submit the work item that changes the state of the main
    // state machine into the "MOVE" state onto the system queue.
    k_work_submit(&motor_alarm_bundle_inst.change_state_machine_work);
}


int core_counter_init(){// TODO: Fn pointer to state changing method.
    LOG_INF("core_counter_init");

    int ret;

    // Check to see if timer device is ready
    if(!device_is_ready(core_cntr_dev))
    {
        LOG_ERR("Cannot find Timer 2's Counter device.");
        return 0;
    }
    else{
        LOG_INF("Timer 2's Counter device found.");
    }

    if(!gpio_is_ready_dt(&timer_led)){
        LOG_ERR("Unable to find LED device.");
        return 0;
    }

    // Configures the pin as an output and sets its logical 
    // state to be inactive/low immediately.
    ret = gpio_pin_configure_dt(&timer_led, GPIO_OUTPUT_INACTIVE);
    
    if(ret < 0){
        LOG_ERR("Failed to configure timer led pin to inactive.");
        return 0;
    }

    // Initializing work items.
    k_work_init(&change_led_work, _change_led_work_handler);
    k_work_init(&motor_alarm_bundle_inst.change_state_machine_work, 
                _change_state_machine_work_handler);

    // Initialize the LED alarm's configuration
    led_alarm_cfg.flags = LED_ALARM_FLAGS;
    led_alarm_cfg.callback = led_alarm_cback;
    led_alarm_cfg.user_data = &led_alarm_cfg;// To be accessed within callback. 
    led_alarm_cfg.ticks = 10;

    // Initialize the Motor's alarm configuration
    motor_alarm_cfg.flags = MOTOR_ALARM_FLAGS;
    motor_alarm_cfg.callback = motor_alarm_cback; // TODO: To be implemented
    motor_alarm_cfg.user_data = &motor_alarm_cfg; // Accessed within callback.
    motor_alarm_cfg.ticks = 0; // To be set by tick value sent from CAN

    // Based on the desired "LED_TOGGLE_PERIOD_US", calculate the corresponding
    // number of ticks.
    ttl_ticks_per_toggle_period = counter_us_to_ticks(core_cntr_dev, 
                                           LED_TOGGLE_PERIOD_US);

    core_counter_ready = 1;

    return 1;
}


int start_core_counter(){
    int ret;

    if(core_counter_ready){
        ret = counter_start(core_cntr_dev);
        if(ret < 0){
            LOG_ERR("Error (%d): Failed to start counter", ret);
            return 0;
        }

        LOG_INF("Core counter started.");

        // Set the alarm for LED toggling
        ret = counter_set_channel_alarm(core_cntr_dev, LED_ALRM_CHAN_ID,
                                        &led_alarm_cfg);
        
        if(!ret){
            LOG_INF("Set LED alarm.");
        }
        else{
            LOG_ERR("Failed to set LED alarm.");
            stop_core_counter();
            core_counter_ready = 0;
            return 0;
        }

        return 1;
    }    
    else{
        LOG_ERR("Unable to start core counter as core counter \
                is not ready.");
        return 0;
    }
}

/*
    Brief: Resets the counter back to 0 when command received on CAN BUS
    (!) Note: The LED alarm will be set to go off after 1s. If alarm for
              moving the motor has been set, the alarm will be stopped 
              and NOT reset. The central motion control unit will have
              to resend the move command via the CAN BUS to set a new 
              motor alarm.
*/
int reset_core_counter(){
    int ret;
    
    _cancel_all_alarms();
    
    led_alarm_cfg.ticks = ttl_ticks_per_toggle_period; 

    ret = counter_reset(core_cntr_dev);

    if(ret < 0){
        LOG_ERR("Error (%d): Failed to reset counter.", ret);
        return 0;
    }

    LOG_INF("Successfully reset core counter.");

    // Reset LED toggling alarm
    ret = counter_set_channel_alarm(core_cntr_dev, LED_ALRM_CHAN_ID,
					                &led_alarm_cfg);

    if(!ret){
        LOG_INF("Set new LED alarm after counter reset.");
    }
    else{
        LOG_ERR("Error(%d), failed to set new LED alarm after counter reset.",
                ret);
        return 0;
    }

    return 1;
}

/*
    Brief: A method that is invoked by the CAN Bus message handler when the
           client sends a request to start moving the motors in a set tick 
           duration.
*/
int set_move_alarm(uint32_t target_ticks){

    int ret;
    uint32_t current_ticks;

    ret = counter_get_value(core_cntr_dev, &current_ticks);

    if(ret < 0){
        LOG_ERR("Error (%d), could not get counter ticks.", ret);
        return -1;
    }

    int64_t delta_ticks = (int64_t)target_ticks - (int64_t)current_ticks; 

    // Check to see if the desired tick target is shorter than
    // MIN_MOTOR_ALARM_DELTA_TICKS. If so, prevent the alarm from
    // being set.
    if(delta_ticks < 0 || 
       delta_ticks < MIN_MOTOR_ALARM_DELTA_TICKS)
    {
        LOG_ERR("Desired move alarm too short ahead of time.");        
        return 0;
    }

    motor_alarm_cfg.ticks = (uint32_t)delta_ticks; 

    ret = counter_set_channel_alarm(core_cntr_dev, MOTOR_ALRM_CHAN_ID,
                                    &motor_alarm_cfg);

    if(ret < 0){
        LOG_ERR("Error(%d), failed to set motor move alarm.", ret);
        return -1;
    }

    LOG_INF("Set motors to move after %d ticks.", motor_alarm_cfg.ticks);

    // Indicate successful setting of motor alarm
    return 1;
}


int64_t get_current_ticks(){

    int ret;
    uint32_t current_ticks;

    if(core_counter_ready){

        ret = counter_get_value(core_cntr_dev, &current_ticks);

        if(ret < 0){
            LOG_ERR("Error (%d), could not get counter ticks.", ret);
            return -1;
        }

        return (int64_t)current_ticks;
    }
    else{
        return -1;
    }
}

void stop_core_counter(){
    LOG_INF("Stopping core counter.");
    _cancel_all_alarms();
    // Turn off LED if still on
    if(core_timer_led_state){
        gpio_pin_set_dt(&timer_led, 0);
        core_timer_led_state = 0;
    }
    counter_stop(core_cntr_dev);
    core_counter_ready = 0;
}


// ============================================================================
// Internal Helper Functions
// ============================================================================
void _cancel_all_alarms()
{
    counter_cancel_channel_alarm(core_cntr_dev, LED_ALRM_CHAN_ID);
    counter_cancel_channel_alarm(core_cntr_dev, MOTOR_ALRM_CHAN_ID);
}


// Handler for "change_led_work" item.
void _change_led_work_handler(struct k_work *work)
{
    if(!gpio_is_ready_dt(&timer_led)){
        LOG_ERR("Core timer's LED is not ready.");
        return;
    }

    gpio_pin_toggle_dt(&timer_led);

    core_timer_led_state = !core_timer_led_state;
}

// Handler for "change_state_machine_work" item.
void _change_state_machine_work_handler(struct k_work *work)
{
    // Get access to the larger data structure embedding
    // the "change_state_machine_work" item to gain access
    // to the method - through a function pointer - to
    // change the state of the state machine to "MOVE".
    struct motor_alarm_fn_pntr_bundle *bundle =
        CONTAINER_OF(work, 
                    struct motor_alarm_fn_pntr_bundle,
                    change_state_machine_work);
    
    // TODO: Invoke the bundled function pointer.

                    
    LOG_INF("Change state machine to MOVE state.");
}