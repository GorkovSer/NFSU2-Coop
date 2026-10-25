/* Optional exact-EXE contact hooks, separate from the ordinary co-op hooks. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "native_contact_impulse.h"
static void __attribute__((thiscall)) pair_hook(void *);
static void __attribute__((thiscall)) impulse_hook(void *,const float *,const float *);
static struct {uint32_t va;uint8_t expected[6];void *hook,*original;DWORD protection;} ni_sites[]={
 {0x5b25b0,{0x55,0x8b,0xec,0x83,0xe4,0xf0},pair_hook,0,0},
 {0x5a0a40,{0x55,0x8b,0xec,0x83,0xe4,0xf0},impulse_hook,0,0}
};
static int installed;
static DWORD ni_owner_thread;
static uintptr_t ni_state_function=0x59b8f0;
#ifdef CONTACT_HOOK_TEST
void ni_test_rebase(uint32_t base){for(int i=0;i<2;i++)ni_sites[i].va+=base-0x400000;ni_state_function+=base-0x400000;}
#endif
static void ni_jump(uint8_t *at,uintptr_t to){at[0]=0xe9;int32_t d=(int32_t)(to-(uintptr_t)(at+5));memcpy(at+1,&d,4);}
int ni_hooks_install(void){
 if(installed)return 1;
 for(int i=0;i<2;i++){
  MEMORY_BASIC_INFORMATION mi;
  if(!VirtualQuery((void*)(uintptr_t)ni_sites[i].va,&mi,sizeof mi)||mi.State!=MEM_COMMIT||
     (mi.Protect&(PAGE_NOACCESS|PAGE_GUARD))||memcmp((void*)(uintptr_t)ni_sites[i].va,ni_sites[i].expected,6))return 0;
 }
 uint8_t *tramp=VirtualAlloc(0,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!tramp)return 0;
 for(int i=0;i<2;i++){
  ni_sites[i].original=tramp+32*i;memcpy(tramp+32*i,ni_sites[i].expected,6);
  ni_jump(tramp+32*i+6,ni_sites[i].va+6);
 }
 DWORD old;
 if(!VirtualProtect(tramp,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(tramp,0,MEM_RELEASE);return 0;}
 FlushInstructionCache(GetCurrentProcess(),tramp,4096);
 int prepared=0;
 for(;prepared<2;prepared++)if(!VirtualProtect((void*)(uintptr_t)ni_sites[prepared].va,6,PAGE_EXECUTE_READWRITE,&ni_sites[prepared].protection))break;
 if(prepared!=2){
  for(int i=prepared-1;i>=0;i--){DWORD unused;VirtualProtect((void*)(uintptr_t)ni_sites[i].va,6,ni_sites[i].protection,&unused);}
  VirtualFree(tramp,0,MEM_RELEASE);return 0;
 }
 for(int i=0;i<2;i++){ni_jump((void*)(uintptr_t)ni_sites[i].va,(uintptr_t)ni_sites[i].hook);*((uint8_t*)(uintptr_t)ni_sites[i].va+5)=0x90;FlushInstructionCache(GetCurrentProcess(),(void*)(uintptr_t)ni_sites[i].va,6);}
 for(int i=1;i>=0;i--){DWORD unused;VirtualProtect((void*)(uintptr_t)ni_sites[i].va,6,ni_sites[i].protection,&unused);}
 ni_owner_thread=GetCurrentThreadId();installed=1;return 1;
}
static void __attribute__((thiscall)) pair_hook(void *pair){
 if(GetCurrentThreadId()==ni_owner_thread)ni_solve(pair,(ni_solver_fn)ni_sites[0].original);
 else ((ni_solver_fn)ni_sites[0].original)(pair);
}
void ni_apply(uint32_t body,const float *impulse,const float *arm){
 ((void(__attribute__((thiscall))*)(void*,const float*,const float*))ni_sites[1].original)((void*)(uintptr_t)body,impulse,arm);
}
static void __attribute__((thiscall)) impulse_hook(void *body,const float *impulse,const float *arm){
 float before[6];uint32_t info=GetCurrentThreadId()==ni_owner_thread?npi_before((uint32_t)(uintptr_t)body,before):0;
 ni_apply((uint32_t)(uintptr_t)body,impulse,arm);
 if(GetCurrentThreadId()==ni_owner_thread){
  ni_observe((uint32_t)(uintptr_t)body,impulse,arm);
  if(info)npi_after((uint32_t)(uintptr_t)body,info,before);
 }
}
void ni_set_state(uint32_t info,const float *p,const float *matrix,const float *v,const float *w){
 ((void(__attribute__((thiscall))*)(void*,const float*,const float*,const float*,const float*,void*))ni_state_function)
  ((void*)(uintptr_t)info,p,matrix,v,w,0);
}
