/* Executes the real adapter's goal-filtering code against mock game memory. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NSLOT 4
#define SL_ACTIVE 2
#define SL_LOADING 1
#define G_FLOW 1
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
static uint32_t memory[40],car[6][16];
static struct {uint32_t car,world;int st;} g_sl[NSLOT];
static uint32_t world(void){return (uint32_t)(uintptr_t)memory;}
static uint32_t rd32(uint32_t a){return a==G_FLOW?6:MR32(a);}
static int ok(uint32_t a,unsigned n){return a==world()&&n<=sizeof memory;}
static int env_int(const char *a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;return 1;}
int nc_hooks_install(void){return 1;}
#define rlog(...) ((void)0)
#include "native_outrun.inc"
static int calls;
static void original(void){
 calls++;
 if(memory[6]!=3||memory[8]!=(uint32_t)(uintptr_t)car[1]||memory[9]!=(uint32_t)(uintptr_t)car[3])abort();
}
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 memory[5]=1;memory[6]=5;
 for(int i=0;i<6;i++){memory[7+i]=(uint32_t)(uintptr_t)car[i];car[i][11]=0xa000+i;}
 g_sl[0].car=(uint32_t)(uintptr_t)car[2];g_sl[0].world=world();g_sl[0].st=SL_ACTIVE;
 g_sl[1].car=(uint32_t)(uintptr_t)car[4];g_sl[1].world=world();g_sl[1].st=SL_LOADING;
 /* A slot from another world must not hide the current world's bot. */
 g_sl[2].car=(uint32_t)(uintptr_t)car[3];g_sl[2].world=world()+4;g_sl[2].st=SL_ACTIVE;
 uint32_t before[40];memcpy(before,memory,sizeof before);
 CHECK(nr_free_car(g_sl[0].car));CHECK(nr_free_car(g_sl[1].car));CHECK(!nr_free_car(g_sl[2].car));CHECK(!nr_free_car(0));
 CHECK(nr_free_brain(0xa002));CHECK(nr_free_brain(0xa004));CHECK(!nr_free_brain(0xa003));CHECK(!nr_free_brain(0));
 for(int i=0;i<10000;i++){nr_goals(original);CHECK(!memcmp(before,memory,sizeof memory));}
 CHECK(calls==10000);puts("PASS outrun goal adapter: active/loading proxies excluded, ordinary bots retained, other-world slots ignored, 10000 full array/range restorations");return 0;
}
