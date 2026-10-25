/* Synthetic x86 continuations only. Does not run a native game routine. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_contact_impulse.h"
void ni_test_rebase(uint32_t);
static unsigned checks;static int block,solves,observes;static uint32_t *expected_body;
static const float *expected_j,*expected_r;
static int before_calls,after_calls;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t npi_before(uint32_t body,float before[6]){
 CHECK(body==(uint32_t)(uintptr_t)expected_body&&expected_body[0]==0);before_calls++;
 for(int k=0;k<6;k++)before[k]=(float)k;return 0x123456;
}
void npi_after(uint32_t body,uint32_t info,const float before[6]){
 CHECK(body==(uint32_t)(uintptr_t)expected_body&&info==0x123456&&expected_body[0]==1&&observes==1);
 for(int k=0;k<6;k++)CHECK(before[k]==(float)k);after_calls++;
}
typedef struct {uint32_t *body;const float *j,*r;uint32_t pair,impulse;} worker_args;
static DWORD WINAPI worker(void *data){
 worker_args *a=data;
 ((ni_solver_fn)(uintptr_t)a->pair)(a->body);
 ((void(__attribute__((thiscall))*)(void*,const float*,const float*))(uintptr_t)a->impulse)(a->body,a->j,a->r);
 return 0;
}
void ni_solve(void *pair,ni_solver_fn original){CHECK(pair==expected_body);solves++;if(!block)original(pair);}
void ni_observe(uint32_t body,const float *j,const float *r){
 CHECK(body==(uint32_t)(uintptr_t)expected_body&&j==expected_j&&r==expected_r);
 CHECK(expected_body[0]==1);observes++;
}
int main(int argc,char **argv){
 int mismatch=argc>1?atoi(argv[1]):-1;
 uint8_t *image=VirtualAlloc(0,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(image!=0);
 const uint8_t start[]={0x55,0x8b,0xec,0x83,0xe4,0xf0};
 uint32_t va[2]={(uint32_t)(uintptr_t)(image+0x1b25b0),(uint32_t)(uintptr_t)(image+0x1a0a40)};
 for(int k=0;k<2;k++)memcpy((void*)(uintptr_t)va[k],start,sizeof start);
 memcpy(image+0x19b8f0,start,sizeof start);
 const uint8_t setter_tail[]={0x8b,0x45,8,0x89,1,0x8b,0x45,12,0x89,0x41,4,0x8b,0x45,16,0x89,0x41,8,0x8b,0x45,20,0x89,0x41,12,0x8b,0x45,24,0x89,0x41,16,0x8b,0xe5,0x5d,0xc2,20,0};
 memcpy(image+0x19b8f0+6,setter_tail,sizeof setter_tail);
 const uint8_t pair_tail[]={0xff,0x01,0x8b,0xe5,0x5d,0xc3};
 const uint8_t impulse_tail[]={0xff,0x01,0x8b,0x45,8,0x89,0x41,4,0x8b,0x55,12,0x89,0x51,8,0x8b,0,0x89,0x41,12,0x8b,0x52,4,0x89,0x51,16,0x8b,0xe5,0x5d,0xc2,8,0};
 memcpy((void*)(uintptr_t)(va[0]+6),pair_tail,sizeof pair_tail);
 memcpy((void*)(uintptr_t)(va[1]+6),impulse_tail,sizeof impulse_tail);
 if(mismatch>=0&&mismatch<2)*(uint8_t*)(uintptr_t)va[mismatch]^=1;
 DWORD old;CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READ,&old));
 ni_test_rebase((uint32_t)(uintptr_t)image);
 if(mismatch>=0&&mismatch<2){
  CHECK(!ni_hooks_install());
  for(int k=0;k<2;k++){uint8_t copy[6];memcpy(copy,(void*)(uintptr_t)va[k],6);if(k==mismatch)copy[0]^=1;CHECK(!memcmp(copy,start,6));}
 }else{
  CHECK(ni_hooks_install());CHECK(ni_hooks_install());
  uint32_t body[5]={0};expected_body=body;
  ((ni_solver_fn)(uintptr_t)va[0])(body);CHECK(body[0]==1&&solves==1);
  block=1;((ni_solver_fn)(uintptr_t)va[0])(body);CHECK(body[0]==1&&solves==2);
  body[0]=0;float j[3]={11,22,33},r[3]={44,55,66};expected_j=j;expected_r=r;
  ((void(__attribute__((thiscall))*)(void*,const float*,const float*))(uintptr_t)va[1])(body,j,r);
  CHECK(observes==1&&body[0]==1&&body[1]==(uint32_t)(uintptr_t)j&&body[2]==(uint32_t)(uintptr_t)r);
  CHECK(body[3]==0x41300000&&body[4]==0x425c0000);
  ni_apply((uint32_t)(uintptr_t)body,j,r);CHECK(body[0]==2&&observes==1);
  worker_args args={body,j,r,va[0],va[1]};HANDLE thread=CreateThread(0,0,worker,&args,0,0);CHECK(thread!=0);
  CHECK(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);
  CHECK(body[0]==4&&observes==1&&solves==2); /* other thread forwards only */
  CHECK(before_calls==1&&after_calls==1);
  uint32_t state[5]={0};float matrix[16]={0};
  ni_set_state((uint32_t)(uintptr_t)state,j,matrix,j,r);
  CHECK(state[0]==(uint32_t)(uintptr_t)j&&state[1]==(uint32_t)(uintptr_t)matrix&&state[2]==(uint32_t)(uintptr_t)j&&state[3]==(uint32_t)(uintptr_t)r&&state[4]==0);
 }
 for(int k=0;k<2;k++){MEMORY_BASIC_INFORMATION mi;CHECK(VirtualQuery((void*)(uintptr_t)va[k],&mi,sizeof mi));CHECK(mi.Protect==PAGE_EXECUTE_READ);}
 printf("PASS exact-contact hooks: %u checks; mode %d, mismatch refusal/whole-instruction forwarding, thiscall body + two vector pointers, original-before-observation and replay without recapture (synthetic x86 only)\n",checks,mismatch);return 0;
}
