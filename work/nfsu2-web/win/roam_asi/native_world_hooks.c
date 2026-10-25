/* Exact local EXE only. All patch sites are verified before any mutation. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "native_world.h"
static void __attribute__((thiscall)) manager_hook(void *,float);
static void __attribute__((thiscall)) brain_hook(void *,float);
static void __attribute__((thiscall)) basic_brain_hook(void *,float);
static uint8_t __attribute__((thiscall)) spawn_hook(void *,uint32_t,const float *);
static uint8_t __attribute__((thiscall)) eligible_hook(void *,uint32_t);
static float __attribute__((thiscall)) distance_hook(void *,const float *);
static float __attribute__((thiscall)) distance3_hook(void *,const float *);
static struct {uint32_t va;unsigned n;uint8_t bytes[12];void *hook,*orig;DWORD prot;} sites[]={
 {0x42f950,6,{0x55,0x8b,0xec,0x83,0xec,0x18},manager_hook,0,0},
 {0x42df50,5,{0x83,0xec,0x08,0x53,0x56},brain_hook,0,0},
 {0x42f840,6,{0x51,0xa1,0xbc,0x00,0x89,0x00},spawn_hook,0,0},
 {0x417be0,7,{0x55,0x56,0x57,0x8b,0x7c,0x24,0x10},eligible_hook,0,0},
 {0x429dd0,9,{0x51,0x56,0x8b,0xf1,0xb9,0x88,0x7e,0x82,0x00},basic_brain_hook,0,0},
 {0x5f0fb0,7,{0x83,0xec,0x10,0x8b,0x44,0x24,0x14},distance_hook,0,0},
 {0x5f1160,7,{0x83,0xec,0x10,0x56,0x8b,0xf1,0x57},distance3_hook,0,0}
};
#define NW_HOOKS ((int)(sizeof sites/sizeof sites[0]))
static int installed;
static DWORD owner_thread;
static uint32_t pool_site=0x61c73e,pool_global=0x8a1d00;
int nw_pool_prepare(void){
 /* Native CarLoader's exact constructor call: name, count, element size,
  * result store, call, pool-global store. 96 is positive in push imm8. */
 static const uint8_t expected[]={0x68,0x9c,0x86,0x7a,0,0x6a,0x16,0x68,0x40,0x12,0,0,0xa3,0xfc,0x1c,0x8a,0,0xe8,0xec,0x46,0xe2,0xff,0xa3,0,0x1d,0x8a,0};
 uint8_t *at=(void*)(uintptr_t)pool_site;MEMORY_BASIC_INFORMATION mi;
 if(!VirtualQuery(at,&mi,sizeof mi)||mi.State!=MEM_COMMIT||(mi.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return 0;
 if(!VirtualQuery((void*)(uintptr_t)pool_global,&mi,sizeof mi)||mi.State!=MEM_COMMIT||(mi.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return 0;
 if(*(uint32_t*)(uintptr_t)pool_global)return 0; /* Too late: never resize a live pool. */
 if(at[6]==96){uint8_t copy[sizeof expected];memcpy(copy,at,sizeof copy);copy[6]=22;return !memcmp(copy,expected,sizeof copy);}
 if(memcmp(at,expected,sizeof expected))return 0;
 DWORD old;if(!VirtualProtect(at+6,1,PAGE_EXECUTE_READWRITE,&old))return 0;
 at[6]=96;FlushInstructionCache(GetCurrentProcess(),at+6,1);DWORD unused;VirtualProtect(at+6,1,old,&unused);return 1;
}
#ifdef WORLD_HOOK_TEST
static uint32_t nw_test_base;
void nw_test_rebase(uint32_t base){nw_test_base=base;for(int i=0;i<NW_HOOKS;i++)sites[i].va+=base-0x400000;pool_site+=base-0x400000;pool_global+=base-0x400000;}
#endif
static void branch(uint8_t *at,uintptr_t to){int32_t rel=(int32_t)(to-(uintptr_t)(at+5));at[0]=0xe9;memcpy(at+1,&rel,4);}
int nw_hooks_install(void){
 if(installed)return 1;
 for(int i=0;i<NW_HOOKS;i++){
  MEMORY_BASIC_INFORMATION mi;
  if(!VirtualQuery((void*)(uintptr_t)sites[i].va,&mi,sizeof mi)||mi.State!=MEM_COMMIT||
     (mi.Protect&(PAGE_NOACCESS|PAGE_GUARD))||memcmp((void*)(uintptr_t)sites[i].va,sites[i].bytes,sites[i].n))return 0;
 }
 uint8_t *tramp=VirtualAlloc(0,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!tramp)return 0;
 for(int i=0;i<NW_HOOKS;i++){sites[i].orig=tramp+32*i;memcpy(tramp+32*i,sites[i].bytes,sites[i].n);branch(tramp+32*i+sites[i].n,sites[i].va+sites[i].n);}
#ifdef WORLD_HOOK_TEST
 /* The spawn prologue reads an absolute native global. Relocate that read
  * only in this synthetic harness, never in the production fixed-EXE DLL. */
 {uint32_t global=nw_test_base+0x4900bc;memcpy(tramp+64+2,&global,4);}
#endif
 DWORD previous;
 if(!VirtualProtect(tramp,4096,PAGE_EXECUTE_READ,&previous)){VirtualFree(tramp,0,MEM_RELEASE);return 0;}
 FlushInstructionCache(GetCurrentProcess(),tramp,4096);
 int prepared=0;
 for(;prepared<NW_HOOKS;prepared++)if(!VirtualProtect((void*)(uintptr_t)sites[prepared].va,sites[prepared].n,PAGE_EXECUTE_READWRITE,&sites[prepared].prot))break;
 if(prepared!=NW_HOOKS){for(int i=prepared-1;i>=0;i--){DWORD unused;VirtualProtect((void*)(uintptr_t)sites[i].va,sites[i].n,sites[i].prot,&unused);}VirtualFree(tramp,0,MEM_RELEASE);return 0;}
 for(int i=0;i<NW_HOOKS;i++){
  uint8_t *at=(void*)(uintptr_t)sites[i].va;branch(at,(uintptr_t)sites[i].hook);
  for(unsigned j=5;j<sites[i].n;j++)at[j]=0x90;
  FlushInstructionCache(GetCurrentProcess(),at,sites[i].n);
 }
 for(int i=NW_HOOKS-1;i>=0;i--){DWORD unused;VirtualProtect((void*)(uintptr_t)sites[i].va,sites[i].n,sites[i].prot,&unused);}
 owner_thread=GetCurrentThreadId();installed=1;return 1;
}
/* The native distance helpers already take the minimum of World+80/+90.
 * Supply the real second observer for ALL their game-thread callers, not
 * only the traffic manager. Keep the original distance policy and restore
 * the cache on return; never mark a map section loaded or enable a body. */
static float nw_distance(void *world,const float *p,int index){
 nw_interest_scope scope;memset(&scope,0,sizeof scope);
 if(GetCurrentThreadId()==owner_thread)nw_interest_begin(&scope);
 if(scope.active&&scope.world!=(uint32_t)(uintptr_t)world){nw_interest_end(&scope);scope.active=0;}
 float result=((float(__attribute__((thiscall))*)(void*,const float*))sites[index].orig)(world,p);
 nw_interest_end(&scope);return result;
}
static float __attribute__((thiscall)) distance_hook(void *world,const float *p){return nw_distance(world,p,5);}
static float __attribute__((thiscall)) distance3_hook(void *world,const float *p){return nw_distance(world,p,6);}
static void __attribute__((thiscall)) manager_hook(void *manager,float dt){
 /* Clearing +8 did not stop native recycling/controller calls. The client
  * owns its complete traffic pool while a current host roster is applied;
  * its ordinary manager must not spawn, recycle or query these replicas. */
 if(nw_manager_owned((uint32_t)(uintptr_t)manager))return;
 nw_interest_scope scope;nw_interest_begin(&scope);
 ((void(__attribute__((thiscall))*)(void*,float))sites[0].orig)(manager,dt);
 nw_interest_end(&scope);
}
static void __attribute__((thiscall)) brain_hook(void *brain,float dt){
 if(!nw_brain_owned((uint32_t)(uintptr_t)brain)){
  nw_interest_scope scope;nw_interest_begin(&scope);
  ((void(__attribute__((thiscall))*)(void*,float))sites[1].orig)(brain,dt);
  nw_interest_end(&scope);
 }
}
static uint8_t __attribute__((thiscall)) spawn_hook(void *manager,uint32_t car,const float *density){
 return nw_spawn(manager,car,density,(nw_spawn_fn)sites[2].orig);
}
static void __attribute__((thiscall)) basic_brain_hook(void *brain,float dt){
 if(!nw_brain_owned((uint32_t)(uintptr_t)brain)){
  nw_interest_scope scope;nw_interest_begin(&scope);
  ((void(__attribute__((thiscall))*)(void*,float))sites[4].orig)(brain,dt);
  nw_interest_end(&scope);
 }
}
static uint8_t __attribute__((thiscall)) eligible_hook(void *manager,uint32_t car){
 return nw_recycle(manager,car,(nw_eligible_fn)sites[3].orig);
}
