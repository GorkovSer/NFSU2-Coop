#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NSLOT 4
typedef void CPU;
static uint8_t ride[0x8f0],part[2][14],resource[0xbc],group[0x80],keys[8],models[24];
static unsigned checks,loads,queries;static int flow=6,requested_ok=1,header_ready;
static uint32_t brands[2];
#define A(p) ((uint32_t)(uintptr_t)(p))
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define rlog(...) ((void)0)
#include "native_free_load_guard.inc"
#define G_FLOW 1
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static int ok(uint32_t p,unsigned n){return p>65535&&p+n>p;}
static uint32_t rd32(uint32_t p){return p==1?(uint32_t)flow:p?MR32(p):0;}
static uint32_t rh_call(CPU *c,uint32_t fn,uint32_t self,uint32_t edx,unsigned count,const uint32_t *a){
 (void)c;CHECK(!edx);
 if(fn==0x61b500){CHECK(count==2&&a[0]==0xebb03e66u&&!a[1]);return self==A(part[0])?brands[0]:brands[1];}
 CHECK(fn==0x494180&&!self&&count==4&&!a[1]&&!a[2]&&!a[3]);
 const char *path=(void*)(uintptr_t)a[0];CHECK(!strcmp(path,"CARS\\WHEELS\\GEOMETRY_BBS.BIN")||!strcmp(path,"CARS\\WHEELS\\GEOMETRY_ENKEI.BIN"));
 loads++;return requested_ok;
}
static uint32_t gcall1(CPU *c,uint32_t fn,uint32_t self,uint32_t arg){
 (void)c;CHECK(fn==0x48ce10&&self==0x82dbc8&&arg);queries++;return header_ready?A(resource):0;
}
#include "native_rim_resources.inc"
int main(void){
 MR32(A(ride)+0x590+29*4)=A(part[0]);MR32(A(ride)+0x590+30*4)=A(part[1]);
 CHECK(nrim_ready(0,A(ride))==1&&!loads); /* stock */
 brands[0]=brands[1]=nrim_hash("BBS");CHECK(brands[0]==0x9536);
 CHECK(nrim_ready(0,A(ride))==0&&loads==1);CHECK(nrim_ready(0,A(ride))==0&&loads==1);
 header_ready=1;CHECK(nrim_ready(0,A(ride))==0&&loads==1);
 MR32(A(resource)+0xb8)=A(group);CHECK(nrim_ready(0,A(ride))==0);
 MR32(A(group)+0xc)=100;MR32(A(group)+0x74)=A(keys);CHECK(nrim_ready(0,A(ride))==0);
 MR32(A(group)+0x78)=A(models);CHECK(nrim_ready(0,A(ride))==1&&loads==1);
 for(int i=0;i<1000;i++)CHECK(nrim_ready(0,A(ride))==1&&loads==1);
 brands[1]=nrim_hash("ENKEI");CHECK(nrim_ready(0,A(ride))==1&&loads==2);
 CHECK(nrim_ready(0,A(ride))==1&&loads==2);
 brands[1]=0x12345678;CHECK(nrim_ready(0,A(ride))==-1&&loads==2);
 brands[1]=nrim_hash("ENKEI");flow=3;unsigned prior=queries;CHECK(nrim_ready(0,A(ride))==-1&&queries==prior);flow=6;
 memset(NRIM,0,sizeof NRIM);requested_ok=0;CHECK(nrim_ready(0,A(ride))==-1&&loads==3);
 CHECK(nrim_ready(0,A(ride))==-1&&loads==3); /* missing pack not retried per frame */
 CHECK(nrim_hash("enkei")==nrim_hash("ENKEI"));
 CHECK(nfl_attempt(0,1,2,3,100));CHECK(!nfl_dependency(0,0,101));
 CHECK(nfl_attempt(0,1,2,3,30000101));CHECK(!nfl_dependency(0,0,30000101));
 CHECK(!nfl_attempt(0,1,2,3,30000102));CHECK(!nfl_dependency(0,1,30000102));
 CHECK(nfl_attempt(0,1,2,4,30000103));CHECK(nfl_dependency(0,1,30000104));
 CHECK(nfl_attempt(1,1,2,1,100));CHECK(!nfl_dependency(1,-1,101));
 CHECK(!nfl_attempt(1,1,2,1,102));CHECK(nfl_attempt(1,2,2,1,103));
 printf("PASS rim resources: %u checks; real helper, BBS/RW2 brand, two axle dependencies, parsed header barrier, one reference/brand across1000 frames, missing/unknown/frontend refusal; mock only\n",checks);
}
