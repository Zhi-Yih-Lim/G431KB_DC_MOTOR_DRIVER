#include "t_watchdog.h"
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/task_wdt/task_wdt.h>

int t_wd_init()
{
    return task_wdt_init(NULL) ? 0 : 1; // No hardware fallback
}

int add_t_wd_chan(uint32_t timeout_us, void (*timeout_cback)(int, void*), 
                  void *usr_data)
{
    return task_wdt_add(timeout_us, timeout_cback, usr_data);
}

int feed_t_wd(int chan)
{
    return task_wdt_feed(chan);
}

int delete_t_wd(int chan)
{
    (void)task_wdt_delete(chan);
}