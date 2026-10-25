#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void CPU;
static uint8_t car_memory[0x9e0];static unsigned checks,calls;static int solid;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define MRF32(a) (*(float*)(uintptr_t)(a))
#define PARK_Z -10000.f
#define FN_CAR_SETPHYS 1
#define FN_CAR_SECTION 2
static uint32_t rd32(uint32_t a){return MR32(a);}
static void body_set(CPU *c,uint32_t car,int on){(void)c;CHECK(car==(uint32_t)(uintptr_t)car_memory&&!on);solid=on;}
static void gcall1(CPU *c,int fn,uint32_t car,uint32_t mode){
 (void)c;CHECK(fn==FN_CAR_SETPHYS&&!mode&&!solid);
 CHECK(MRF32(car+0x60)==100&&MRF32(car+0x68)==3);MR32(car+0x28)=0;calls++;
}
static void gcall0(CPU *c,int fn,uint32_t car){(void)c;CHECK(fn==FN_CAR_SECTION&&!solid&&!MR32(car+0x28)&&MRF32(car+0x68)==PARK_Z);}
#include "native-park-extracted.inc"
int main(void){
 for(int round=0;round<1000;round++)for(int mode=0;mode<=5;mode++){
  memset(car_memory,0,sizeof car_memory);uint32_t car=(uint32_t)(uintptr_t)car_memory;
  MR32(car+0x28)=mode;MRF32(car+0x60)=100;MRF32(car+0x68)=3;MRF32(car+0x70)=55;MRF32(car+0x110)=2;solid=1;calls=0;
  soft_park(0,car);CHECK(!solid&&!MR32(car+0x28)&&calls==(mode!=0));CHECK(MRF32(car+0x60)==0&&MRF32(car+0x50)==0&&MRF32(car+0x58)==PARK_Z&&MRF32(car+0x70)==0&&MRF32(car+0x110)==0);
 }
 printf("PASS generated native soft_park: %u checks/6000 cases; body disabled and physics withdrawn before off-map writes/section update; mock engine only\n",checks);return 0;
}
