#ifndef NATIVE_WORLD_H
#define NATIVE_WORLD_H
#include <stdint.h>
/* Native Activate for record kind3 selects controller4/785428.
 * Controller2/785518 is the heavier AI variant, also seen in traffic. */
static inline int nw_traffic_driver(uint32_t type){return type==2||type==4;}
static inline int nw_traffic_controller(uint32_t type,uint32_t table){return (type==2&&table==0x785518u)||(type==4&&table==0x785428u);}
int nw_enabled(void);
int nw_host(void);
int nw_hooks_install(void);
int nw_pool_prepare(void);
int nw_manager_owned(uint32_t manager);
int nw_brain_owned(uint32_t brain);
typedef struct {uint32_t world,position[3],road;int active;} nw_interest_scope;
typedef uint8_t (__attribute__((thiscall)) *nw_spawn_fn)(void *,uint32_t,const float *);
typedef uint8_t (__attribute__((thiscall)) *nw_eligible_fn)(void *,uint32_t);
void nw_interest_begin(nw_interest_scope *scope);
void nw_interest_end(const nw_interest_scope *scope);
uint8_t nw_spawn(void *manager,uint32_t car,const float *density,nw_spawn_fn original);
uint8_t nw_recycle(void *manager,uint32_t car,nw_eligible_fn original);
#endif
