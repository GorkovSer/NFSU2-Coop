#ifndef NATIVE_COOP_H
#define NATIVE_COOP_H
#include <stdint.h>
int nc_enabled(void);
int nc_start(void);
int nc_state(uint32_t race,uint32_t state);
int nc_remote_brain(uint32_t brain);
int nc_remote_car(uint32_t car);
int nc_finish_block(uint32_t race);
void nc_finished(uint32_t race,uint32_t index);
int nc_hooks_install(void);
void nc_original_start(void);
void nc_original_state(uint32_t race,uint32_t state);
void nc_original_finish(uint32_t race,uint32_t index,uint32_t position,uint32_t flag,uint32_t completion);
int nc_ready_key(void);
int nc_cancel_key(void);
#ifdef NATIVE_NEXT
void ni_style_finalize(uint32_t score);
void ni_style_board(void);
void ni_race_results(uint32_t race);
int nc_window_active(void);
int nc_original_world_paused(uint32_t world);
#endif
#endif
