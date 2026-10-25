/* Host-owned pause ledger. Units: 4000 ticks/second, as native NFS results. */
#ifndef RACE_CLOCK_H
#define RACE_CLOCK_H
#include <stdint.h>
#include <string.h>
#define RK_SPANS 512
typedef struct {uint32_t begin,end;} rk_span;
typedef struct {rk_span span[RK_SPANS];unsigned n;uint32_t last,begin;int active,held,fault;} rk_clock;
static int rk_step(rk_clock *c,uint32_t wall,int held){
 if(c->fault)return 0;
 if(!c->active){c->active=1;c->held=1;c->begin=0;}
 if(wall<c->last){c->fault=1;return 0;}c->last=wall;
 if(c->held&&!held){
  if(wall>c->begin){if(c->n==RK_SPANS){c->fault=1;return 0;}c->span[c->n++]=(rk_span){c->begin,wall};}
 }else if(!c->held&&held)c->begin=wall;
 c->held=!!held;return 1;
}
static int rk_elapsed(const rk_clock *c,uint32_t wall,uint32_t *elapsed){
 if(!c->active||c->fault||wall>c->last)return 0;
 uint32_t stopped=0;
 for(unsigned i=0;i<c->n;i++){
  uint32_t a=c->span[i].begin,b=c->span[i].end;
  if(wall<=a)break;stopped+=(wall<b?wall:b)-a;
 }
 if(c->held&&wall>c->begin)stopped+=wall-c->begin;
 if(stopped>wall)return 0;*elapsed=wall-stopped;return 1;
}
#endif
