#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
enum {CC_INVITED=1,CC_LOADING=3,CC_DONE=6};
static struct {int host;struct {uint32_t event;int phase;}control;} NC;
static uint8_t catalog[3][0x88];static uint32_t unlocked[64],count=3;static int valid=1;
#define MR32(p) (*(uint32_t*)(uintptr_t)(p))
static uint32_t rd32(uint32_t p){
 if(p==0x8383e0)return (uint32_t)(uintptr_t)catalog;
 if(p==0x8383e4)return count;
 if(p>=0x85ad40+0x71c8&&p<0x85ad40+0x71c8+sizeof unlocked)return unlocked[(p-0x85ad40-0x71c8)/4];
 return 0;
}
static int ok(uint32_t p,uint32_t n){return valid&&p>=(uint32_t)(uintptr_t)catalog&&p+n>=p&&p+n<=(uint32_t)(uintptr_t)(catalog+3);}
static int nc_event_supported(uint32_t p){return ((uint8_t*)(uintptr_t)p)[15]==0;}
static uint32_t shared_hash;
static uint32_t nes_available(uint32_t hash){return valid&&hash==shared_hash?(uint32_t)(uintptr_t)catalog[1]:0;}
#include "native_event_available.inc"
int main(void){
 for(unsigned i=0;i<3;i++)MR32((uintptr_t)catalog[i]+8)=100+i;
 catalog[2][15]=3;unlocked[0]=(uint32_t)(uintptr_t)catalog[0];
 NC.host=1;assert(nc_available(100)==unlocked[0]);assert(!nc_available(101));
 shared_hash=101;assert(nc_available(101)==(uint32_t)(uintptr_t)catalog[1]);shared_hash=0;
 NC.host=0;NC.control.event=101;NC.control.phase=CC_INVITED;
 assert(nc_available(101)==(uint32_t)(uintptr_t)catalog[1]);
 assert(!nc_available(0));assert(!nc_available(103));
 NC.control.event=102;assert(!nc_available(102));
 NC.control.event=101;NC.control.phase=0;assert(!nc_available(101));
 NC.control.phase=CC_LOADING;assert(nc_available(101));
 count=4097;assert(!nc_available(101));count=0;assert(!nc_available(101));
 count=3;valid=0;assert(!nc_available(101));
 puts("PASS event availability: unlocked host, exact invited guest, unsupported/missing events and invalid catalog rejected");
}
