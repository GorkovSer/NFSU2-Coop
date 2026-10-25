#ifndef NATIVE_CONTACT_IMPULSE_H
#define NATIVE_CONTACT_IMPULSE_H
#include <stdint.h>
typedef void (__attribute__((thiscall)) *ni_solver_fn)(void *);
int ni_hooks_install(void);
void ni_solve(void *pair,ni_solver_fn original);
void ni_observe(uint32_t body,const float *impulse,const float *arm);
void ni_apply(uint32_t body,const float *impulse,const float *arm);
uint32_t npi_before(uint32_t body,float before[6]);
void npi_after(uint32_t body,uint32_t info,const float before[6]);
void ni_set_state(uint32_t info,const float *p,const float *matrix,const float *v,const float *w);
#endif
