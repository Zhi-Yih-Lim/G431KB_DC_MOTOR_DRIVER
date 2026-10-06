#ifndef T_WD_H
#define T_WD_H

int t_wd_init();
int add_t_wd_chan(uint32_t timeout_us, void (*timeout_cback)(int, void*), 
                  void *usr_data);
int feed_t_wd(int chan);
void delete_t_wd(int chan);

#endif