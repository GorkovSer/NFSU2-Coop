#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
uint32_t nes_marker_event(uint32_t career,uint32_t marker);
int nes_event_unlocked(uint32_t list,uint32_t event);
void nes_map_rebuilt(uint32_t career);
static DWORD owner;
static struct {
 uint32_t site;unsigned length;uint8_t expected[6];void *original;DWORD protection;
} hooks[]={
 {0x527710,5,{0x51,0x8b,0x44,0x24,0x08},0,0},
 {0x5007d0,5,{0x56,0x8b,0x74,0x24,0x08},0,0},
 {0x533260,6,{0x83,0xec,0x20,0x56,0x8b,0xf1},0,0}
};
static uint32_t __attribute__((thiscall)) lookup(void *career,uint32_t marker){
 uint32_t result=((uint32_t(__attribute__((thiscall))*)(void*,uint32_t))hooks[0].original)(career,marker);
 if(!result&&GetCurrentThreadId()==owner)result=nes_marker_event((uint32_t)(uintptr_t)career,marker);
 return result;
}
static uint8_t __attribute__((thiscall)) unlocked(void *list,uint32_t event){
 uint8_t result=((uint8_t(__attribute__((thiscall))*)(void*,uint32_t))hooks[1].original)(list,event);
 if(!result&&GetCurrentThreadId()==owner)result=(uint8_t)nes_event_unlocked((uint32_t)(uintptr_t)list,event);
 return result;
}
static void __attribute__((thiscall)) rebuild(void *career){
 ((void(__attribute__((thiscall))*)(void*))hooks[2].original)(career);
 if(GetCurrentThreadId()==owner)nes_map_rebuilt((uint32_t)(uintptr_t)career);
}
#ifdef EVENT_HOOK_TEST
void nes_test_rebase(uint32_t base){for(unsigned i=0;i<3;i++)hooks[i].site+=base-0x400000;}
#endif
static void nes_jump(uint8_t *p,uintptr_t target){int32_t d=(int32_t)(target-(uintptr_t)(p+5));p[0]=0xe9;memcpy(p+1,&d,4);}
int nes_hooks_install(void){
 static int installed;if(installed)return 1;
 uintptr_t targets[]={(uintptr_t)lookup,(uintptr_t)unlocked,(uintptr_t)rebuild};
 unsigned writable=0;DWORD ignored;MEMORY_BASIC_INFORMATION mi;
 /* Validate and prepare every trampoline before changing any game code. */
 for(unsigned i=0;i<3;i++){
  void *site=(void*)(uintptr_t)hooks[i].site;unsigned n=hooks[i].length;
  if(!VirtualQuery(site,&mi,sizeof mi)||mi.State!=MEM_COMMIT||
     (mi.Protect&(PAGE_GUARD|PAGE_NOACCESS))||memcmp(site,hooks[i].expected,n))goto fail;
  uint8_t *tramp=VirtualAlloc(0,32,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!tramp)goto fail;
  hooks[i].original=tramp;memcpy(tramp,site,n);nes_jump(tramp+n,hooks[i].site+n);
  if(!VirtualProtect(tramp,32,PAGE_EXECUTE_READ,&ignored))goto fail;
  FlushInstructionCache(GetCurrentProcess(),tramp,32);
 }
 for(unsigned i=0;i<3;i++){
  if(!VirtualProtect((void*)(uintptr_t)hooks[i].site,hooks[i].length,PAGE_EXECUTE_READWRITE,&hooks[i].protection))goto fail;
  writable++;
 }
 owner=GetCurrentThreadId();
 for(unsigned i=0;i<3;i++){
  uint8_t *site=(void*)(uintptr_t)hooks[i].site;nes_jump(site,targets[i]);
  if(hooks[i].length>5)memset(site+5,0x90,hooks[i].length-5);
  VirtualProtect(site,hooks[i].length,hooks[i].protection,&ignored);
  FlushInstructionCache(GetCurrentProcess(),site,hooks[i].length);
 }
 installed=1;return 1;
fail:
 for(unsigned i=0;i<writable;i++)VirtualProtect((void*)(uintptr_t)hooks[i].site,hooks[i].length,hooks[i].protection,&ignored);
 for(unsigned i=0;i<3;i++)if(hooks[i].original){VirtualFree(hooks[i].original,0,MEM_RELEASE);hooks[i].original=0;}
 return 0;
}
