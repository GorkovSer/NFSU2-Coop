#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t cars[2][0x9e0],renders[2][0x2ec0],wm[0x100];
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t addr(void *p){return (uint32_t)(uintptr_t)p;}
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define G_FLOW 1
static int flow=6;static struct {int enabled;} NC,NW;
static int ok(uint32_t p,unsigned n){
 if(p+n<p)return 0;
 return (p>=addr(cars)&&p+n<=addr(cars)+sizeof cars)||(p>=addr(renders)&&p+n<=addr(renders)+sizeof renders)||(p>=addr(wm)&&p+n<=addr(wm)+sizeof wm);
}
static uint32_t rd32(uint32_t p){return p==G_FLOW?(uint32_t)flow:ok(p,4)?MR32(p):0;}
static uint32_t world(void){return addr(wm);}
#include "native_texture_cache.inc"
static uint32_t entry(int car,int table,int index){return addr(renders[car])+0x638+0x490*table+16*index;}
int main(void){
 NC.enabled=1;MR32(addr(wm)+0x10)=2;
 for(int c=0;c<2;c++){
  uint32_t car=addr(cars[c]),r=addr(renders[c]);MR32(car+0x554)=r;MR32(car+0x10)=100+c;MR32(car+0xc)=200;
  MR32(r)=car;MR32(r+4)=100+c;MR32(r+8)=200;MR32(addr(wm)+0x1c+4*c)=car;
 }
 /* Reported family: two MIATA instances, independently composited skins,
  * cache survives while the native texture manager replaces TextureInfo. */
 for(int step=0;step<1000;step++){
  for(int c=0;c<2;c++)for(int t=0;t<3;t++)for(int i=0;i<73;i++){
   uint32_t e=entry(c,t,i);MR32(e)=0x9b50e870+i;MR32(e+4)=0x530b82b1+c;MR32(e+8)=0x158a8900+step*4;
  }
  ntc_frame();
  for(int c=0;c<2;c++)for(int t=0;t<3;t++)for(int i=0;i<73;i++){
   uint32_t e=entry(c,t,i);CHECK(rd32(e+8)==0xffffffffu);CHECK(rd32(e)==0x9b50e870u+i&&rd32(e+4)==0x530b82b1u+c&&!rd32(e+12));
  }
 }
 uint32_t e=entry(0,0,0);MR32(e+8)=123;MR32(e+12)=456;
 MR32(entry(0,0,1)+8)=789;ntc_frame();CHECK(rd32(e+8)==123&&rd32(e+12)==456&&rd32(entry(0,0,1)+8)==789);
 MR32(e+12)=0;MR32(e)=0;ntc_frame();CHECK(rd32(e+8)==123);MR32(e)=0x9b50e870;
 MR32(addr(renders[0])+4)=999;ntc_frame();CHECK(rd32(e+8)==123);MR32(addr(renders[0])+4)=100;
 MR32(addr(renders[0]))=addr(cars[1]);ntc_frame();CHECK(rd32(e+8)==123);MR32(addr(renders[0]))=addr(cars[0]);
 for(int f=0;f<9;f++)if(f!=6){flow=f;ntc_frame();CHECK(rd32(e+8)==123);}flow=6;
 NC.enabled=0;ntc_frame();CHECK(rd32(e+8)==123);NW.enabled=1;ntc_frame();CHECK(rd32(e+8)==0xffffffffu);
 MR32(e+8)=123;MR32(addr(wm)+0x10)=23;ntc_frame();CHECK(rd32(e+8)==123);
 CHECK(!ntc_refresh_car(0));
 printf("PASS texture remap cache: %u checks; two same-model cars, 1000 texture replacements, three native tables, active-swap/owner/ride/frontend/count/disabled guards; mock only\n",checks);
 return 0;
}
