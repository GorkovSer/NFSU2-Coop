#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roam_core.h"
#include "coop_race.h"
typedef void CPU;
static uint8_t catalog[3][0x88],marker[3][0x34],cache[64*24];static uint32_t unlocked[64];
static rc_session g_s;static struct {uint32_t peer;cc_race control;} NC;
static int live=1,checks,refreshes;static int64_t clock_now=1000000;static uint32_t live_world=99,selection;
#define A(p) ((uint32_t)(uintptr_t)(p))
static void *gp(uint32_t a){if(a>=0x85ad40&&a<0x85ad40+sizeof cache)return cache+(a-0x85ad40);return (void*)(uintptr_t)a;}
#define GPTR(a) gp(a)
#define MR32(a) (*(uint32_t*)GPTR(a))
#define MR8(a) (*(uint8_t*)GPTR(a))
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
#define rlog(...) ((void)0)
static int ok(uint32_t a,unsigned n){return (a>=0x85ad40&&a+n<=0x85ad40+sizeof cache)||(a>=A(catalog)&&a+n<=A(catalog)+sizeof catalog)||(a>=A(marker)&&a+n<=A(marker)+sizeof marker);}
static uint32_t rd32(uint32_t a){
 if(a==0x8383e0)return A(catalog);if(a==0x8383e4)return 3;if(a==0x85ad40+0x7af8)return 2;
 if(a==0x863448)return selection;
 if(a>=0x85ad40+0x71c8&&a<0x85ad40+0x72c8)return unlocked[(a-0x85ad40-0x71c8)/4];
 return ok(a,4)?MR32(a):0;
}
static int nc_event_supported(uint32_t e){return MR8(e+15)==0;}
static int roaming(void){return live;}
static uint32_t world(void){return live_world;}
static int64_t roam_plat_now_us(void){return clock_now;}
static void roam_plat_lock(void){}static void roam_plat_unlock(void){}
static uint32_t rh_call(CPU *c,uint32_t fn,uint32_t ecx,uint32_t edx,unsigned n,const uint32_t *args){
 (void)c;CHECK(!ecx&&!edx&&n==1);
 if(fn==0x4cb070){CHECK(args[0]>=0x85ad40&&args[0]<0x85ad40+sizeof cache);refreshes++;return 0;}
 CHECK(fn==0x4ffc40);for(int i=0;i<3;i++)if(args[0]==1000u+i)return A(marker[i]);return 0;
}
#include "native_event_share.inc"
int main(void){
 for(int i=0;i<3;i++){MR32(A(catalog[i])+8)=101+i;MR32(A(catalog[i])+0x28)=1000+i;MR8(A(catalog[i])+0x37)=2;MR32(A(marker[i])+0x30)=1000+i;}
 unlocked[0]=A(catalog[0]);uint32_t saved_unlocks[64];memcpy(saved_unlocks,unlocked,sizeof unlocked);
 /* Local event/shops must survive adding and withdrawing peer markers. */
 MR32(0x85ad40)=A(marker[0]);MR32(0x85ad40+4)=A(catalog[0]);MR32(0x85ad40+20)=1;
 MR32(0x85ad40+24)=A(marker[2]);MR32(0x85ad40+24+8)=0xfeed;MR32(0x85ad40+24+20)=2;
 g_s.conf.coop=1;g_s.conf.extensions=RC_EXT_CAREER_MAP;g_s.conf.self_id=1;g_s.conf.extension_nonce=100;
 rc_peer *p=&g_s.peer[0];p->id=2;p->coop=1;p->core_ver=4;p->state=RC_PS_EST;p->extension_seen=1;p->extension_at=clock_now;
 p->extensions.features=RC_EXT_CAREER_MAP;p->extensions.nonce=200;p->extensions.echo=100;p->status=RC_ST_ROAM;
 p->event_catalog_seen=1;p->event_catalog_at=clock_now;p->event_catalog.source=200;p->event_catalog.stage=2;p->event_catalog.n=2;p->event_catalog.event[0]=101;p->event_catalog.event[1]=102;
 nes_frame(0,clock_now);CHECK(rd32(0x85ad40+48+4)==A(catalog[1]));CHECK(nes_available(102)==A(catalog[1]));
 CHECK(nes_marker_event(0x85ad40,A(marker[1]))==A(catalog[1]));CHECK(!nes_marker_event(0x1234,A(marker[1])));
 for(int i=0;i<100;i++)nes_frame(0,clock_now);CHECK(refreshes==1);CHECK(!memcmp(saved_unlocks,unlocked,sizeof unlocked));
 CHECK(rd32(0x85ad40)==A(marker[0])&&rd32(0x85ad40+24+8)==0xfeed);
 p->event_catalog.n=0;nes_frame(0,clock_now);CHECK(!rd32(0x85ad40+48)&&!nes_available(102));
 selection=A(catalog[1]);CHECK(nes_block_solo_start());selection=A(catalog[0]);CHECK(!nes_block_solo_start());selection=0;
 p->event_catalog.n=1;p->event_catalog.event[0]=102;nes_frame(0,clock_now);CHECK(rd32(0x85ad40+48+4)==A(catalog[1]));
 p->lost=1;nes_frame(0,clock_now);CHECK(!rd32(0x85ad40+48));p->lost=0;
 p->event_catalog.stage=3;nes_frame(0,clock_now);CHECK(!rd32(0x85ad40+48));p->event_catalog.stage=2;
 nes_frame(0,clock_now);MR32(0x85ad40+48)=A(marker[2]);MR32(0x85ad40+48+4)=A(catalog[2]);
 p->lost=1;nes_frame(0,clock_now);CHECK(rd32(0x85ad40+48+4)==A(catalog[2]));
 CHECK(!memcmp(saved_unlocks,unlocked,sizeof unlocked));
 /* Same-stage alone never admits events unavailable to either player. */
 p->lost=0;p->event_catalog.n=1;p->event_catalog.event[0]=103;MR8(A(catalog[2])+0x37)=3;
 nes_frame(0,clock_now);CHECK(!nes_available(103));MR8(A(catalog[2])+0x37)=2;
 p->event_catalog.event[0]=102;clock_now+=4000000;p->extension_at=clock_now;
 nes_frame(0,clock_now);CHECK(!nes_available(102));p->status=RC_ST_PAUSED;
 nes_frame(0,clock_now);CHECK(nes_available(102)==A(catalog[1]));
 /* Local unlock adoption: a native event becoming available is retained
  * after the peer withdraws it; it is no longer owned by our overlay. */
 uint32_t adopted=0;for(unsigned i=0;i<64;i++)if(rd32(0x85ad40+24*i+4)==A(catalog[1]))adopted=0x85ad40+24*i;
 CHECK(adopted);unlocked[1]=A(catalog[1]);nes_frame(0,clock_now);p->event_catalog.n=0;nes_frame(0,clock_now);
 CHECK(rd32(adopted+4)==A(catalog[1]));
 unlocked[1]=0;
 /* Capacity exhaustion never overwrites any native descriptor. */
 for(unsigned i=0;i<64;i++){MR32(0x85ad40+24*i)=A(marker[2]);MR32(0x85ad40+24*i+4)=A(catalog[2]);}
 uint8_t full[sizeof cache];memcpy(full,cache,sizeof cache);p->event_catalog.n=1;p->event_catalog.event[0]=102;
 nes_frame(0,clock_now);CHECK(!memcmp(full,cache,sizeof cache));
 NC.peer=2;NC.control.event=102;NC.control.phase=CC_LOADING;NES.expires=0;live=0;
 CHECK(nes_available(102)==A(catalog[1]));NC.peer=3;CHECK(!nes_available(102));NC.peer=2;
 NC.control.event=103;CHECK(!nes_available(102));live=1;
 printf("PASS shared map: %d checks; catalog union, native descriptors/Engage, duplicate-free refresh, peer loss/withdrawal/stage isolation, native slot reuse and unchanged unlock array; mock only\n",checks);
}
