/* Real native Engage adapter with synthetic UI/slot state. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NSLOT 2
#define G_PLAYER1 1
#define G_RACE 2
#define G_RACETYPE ((uint32_t)(uintptr_t)&racetype)
#define PL_MARKER 0x9ac
#define DU_PROMPT_M 40.f
#define DU_KEEP_M 60.f
#define DU_IDLE 0
#define DU_SENT 1
#define DU_GOT 2
#define DU_ACCEPTED 3
#define DU_RACE 4
#define MR32(a) (*(uint32_t *)(uintptr_t)(a))
static uint32_t pl[0x9b0/4],race[0xc44/4],scene=123,racetype;
static struct {uint32_t icon,id;int st;} g_du;
static struct {uint32_t car;int hidden,able;float distance;} g_sl[NSLOT];
static int g_in_roam=1,in_city=1,enabled=1,checks;
static uint32_t a(void *p){return (uint32_t)(uintptr_t)p;}
static uint32_t rd32(uint32_t p){if(p==G_PLAYER1)return a(pl);if(p==G_RACE)return a(race);if(p==G_RACETYPE)return racetype;return MR32(p);}
static int ok(uint32_t p,unsigned n){return (p==a(pl)&&n<=sizeof pl)||(p==a(race)&&n<=sizeof race);}
static int nr_outruns_enabled(void){return enabled;}
static int roaming(void){return in_city;}
static uint32_t world(void){return scene;}
static int du_able(int k){return g_sl[k].able;}
static float du_dist(uint32_t car){return g_sl[car==100?0:1].distance;}
static int du_slot(uint32_t id){return id==200?0:-1;}
#include "native_outrun_input.inc"
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 race[0x24/4]=1;g_sl[0].car=100;g_sl[0].able=1;g_sl[0].distance=30;
 CHECK(!nr_engage_intercept());CHECK(!nr_take_engage());
 g_du.icon=100;CHECK(nr_engage_intercept());CHECK(nr_take_engage());CHECK(!nr_take_engage());
 CHECK(nr_engage_intercept());scene++;CHECK(!nr_take_engage());
 pl[PL_MARKER/4]=1;CHECK(!nr_engage_intercept());pl[PL_MARKER/4]=0;
 g_sl[0].hidden=1;CHECK(!nr_engage_intercept());g_sl[0].hidden=0;
 g_sl[0].able=0;CHECK(!nr_engage_intercept());g_sl[0].able=1;
 g_sl[0].distance=41;CHECK(!nr_engage_intercept());g_sl[0].distance=30;
 enabled=0;CHECK(!nr_engage_intercept());enabled=1;
 in_city=0;CHECK(!nr_engage_intercept());in_city=1;
 g_in_roam=0;CHECK(!nr_engage_intercept());g_in_roam=1;
 race[0x24/4]=2;CHECK(!nr_engage_intercept());race[0x24/4]=1;
 racetype=7;CHECK(!nr_engage_intercept());racetype=0;
 for(int st=DU_SENT;st<=DU_ACCEPTED;st++){
  g_du.st=st;g_du.id=200;g_du.icon=0;g_sl[0].distance=55;
  CHECK(nr_engage_intercept()&&nr_take_engage());
  g_sl[0].distance=61;CHECK(!nr_engage_intercept());
 }
 g_du.st=DU_RACE;CHECK(!nr_engage_intercept());
 printf("PASS native Engage adapter: %d checks; challenge/accept latch, one-shot consumption, marker/race/disabled/stale-slot/distance/world guards\n",checks);return 0;
}
