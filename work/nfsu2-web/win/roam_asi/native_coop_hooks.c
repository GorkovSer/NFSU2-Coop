/* Exact-EXE hooks; installed on the game thread before career is entered.
 * All originals retain whole instructions, without PC-relative operands.
 * No game reads or calls from the network thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "native_coop.h"
typedef struct {uint32_t va;uint8_t n,expected[12];void *target,*original;DWORD protection;} Patch;
#ifdef NATIVE_NEXT
int nm_finish(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
int nm_career_win(uint32_t);
int nm_can_continue(uint32_t);
uint32_t nm_downhill_event(uint32_t);
int nm_downhill_grid(void);
void nm_downhill_board_built(void);
int ncp_intro_hold(uint32_t);
int ncp_nis_hold(uint32_t,uint32_t,uint32_t);
static int __attribute__((thiscall)) nis_flow_hook(void *,uint32_t,uint32_t);
static void __cdecl board_build_hook(void);
static void __attribute__((thiscall)) intro_hook(void *);
static void __attribute__((thiscall)) series_init_hook(void *,uint32_t);
static void __cdecl career_config_hook(uint32_t,uint32_t);
static void __attribute__((naked)) virtual_id_hook(void);
__attribute__((used)) static volatile uintptr_t nm_virtual_continue;
static DWORD nc_owner_thread;
static int __attribute__((thiscall)) series_continue_hook(void *);
static int __attribute__((thiscall)) career_result_hook(void *,uint32_t);
int nr_free_brain(uint32_t);
int nr_free_car(uint32_t);
int nr_engage_intercept(void);
int np_contacts_enabled(void);
void np_before_zero(uint32_t car);
int nc_pause_gate(uint32_t world,int native_paused);
int nc_pause_clock_enter(uint32_t clock,uint32_t *saved);
void nc_pause_clock_leave(uint32_t saved);
static int __attribute__((thiscall)) pause_hook(void *world);
static float __attribute__((thiscall)) clock_hook(void *clock,float dt);
static int __attribute__((thiscall)) engage_hook(void *manager);
void nr_goals(void(*)(void));
static void __cdecl goals_hook(void);
int nr_reward_scope(uint32_t career,uint32_t event);
void nr_reward_solo_start(void);
void nv_solo_start(void);
int nes_block_solo_start(void);
int nv_owned_performance(uint32_t ride);
static void __attribute__((thiscall)) performance_hook(void *,uint32_t,uint32_t,uint32_t,uint32_t);
static void __attribute__((thiscall)) win_hook(void *,uint32_t);
static void __attribute__((naked)) credit_a_hook(void);
static void __attribute__((naked)) credit_b_hook(void);
__attribute__((used)) static volatile uint32_t nr_cash_career;
__attribute__((used)) static volatile uintptr_t nr_credit_a_original,nr_credit_b_original,nr_credit_a_continue,nr_credit_b_continue;
#endif
static void __cdecl start_hook(void){if(!nc_start()){
#ifdef NATIVE_NEXT
    if(nes_block_solo_start())return;
    nr_reward_solo_start();
    nv_solo_start();
#endif
    nc_original_start();}}
static void __attribute__((thiscall)) state_hook(void *race,uint32_t state){
    if(!nc_state((uint32_t)(uintptr_t)race,state))nc_original_state((uint32_t)(uintptr_t)race,state);
}
static void __attribute__((thiscall)) driver_hook(void *brain,float dt);
static void __attribute__((thiscall)) physics_hook(void *car,uint32_t mode);
static void __attribute__((thiscall)) finish_hook(void *race,uint32_t index,uint32_t position,uint32_t flag,uint32_t completion){
    uint32_t r=(uint32_t)(uintptr_t)race;
#ifdef NATIVE_NEXT
    if(GetCurrentThreadId()==nc_owner_thread&&nm_finish(r,index,position,flag,completion))return;
#endif
    if(nc_finish_block(r))return;
    nc_original_finish(r,index,position,flag,completion);nc_finished(r,index);
}
static Patch patches[]={
 {0x499ae0,5,{0xa1,0x44,0x34,0x86,0},start_hook,0,0},
 {0x5fc900,5,{0x55,0x8b,0x6c,0x24,0x08},state_hook,0,0},
 {0x42c100,5,{0x83,0xec,0x14,0x53,0x56},driver_hook,0,0},
 {0x5f46a0,7,{0x6a,0xff,0x68,0xe2,0x9a,0x77,0},physics_hook,0,0},
 {0x606cb0,6,{0x51,0x8b,0x44,0x24,0x08,0x53},finish_hook,0,0}
#ifdef NATIVE_NEXT
 ,{0x42cda0,5,{0xa1,0x80,0,0x89,0},goals_hook,0,0}
 ,{0x541380,5,{0x51,0x53,0x56,0x8b,0xf1},win_hook,0,0}
 ,{0x5413fc,8,{0x8b,0x96,0x34,0x71,0,0,0x03,0xd0},credit_a_hook,0,0}
 ,{0x54143a,6,{0x01,0x8e,0x34,0x71,0,0},credit_b_hook,0,0}
 ,{0x639ce0,6,{0x56,0x8b,0xf1,0x83,0x3e,0xff},performance_hook,0,0}
 ,{0x500d70,6,{0x8b,0x91,0xe8,0x1a,0,0},engage_hook,0,0}
 ,{0x5ea320,5,{0xa1,0x88,0,0x89,0},pause_hook,0,0}
 ,{0x601a60,9,{0x83,0xec,0x0c,0xd9,0x05,0x38,0x57,0x7a,0},clock_hook,0,0}
 ,{0x528730,6,{0x55,0x8b,0xec,0x51,0x8b,0xd1},career_result_hook,0,0}
 ,{0x513f00,6,{0x8b,0x81,0xe4,0x34,0,0},series_continue_hook,0,0}
 ,{0x532df0,9,{0x55,0x8b,0xec,0x81,0xec,0x28,0x0d,0,0},series_init_hook,0,0}
 ,{0x532240,8,{0x55,0x8b,0xec,0x83,0xe4,0xf0,0x6a,0xff},career_config_hook,0,0}
 ,{0x56dabd,8,{0x89,0x74,0x24,0x38,0x89,0x44,0x24,0x3c},virtual_id_hook,0,0}
 ,{0x56d7b0,6,{0x55,0x8b,0xec,0x83,0xe4,0xf0},board_build_hook,0,0}
 ,{0x5f94e0,6,{0x56,0x8b,0xf1,0x83,0x3e,0x03},intro_hook,0,0}
 ,{0x603bc0,5,{0x8b,0x44,0x24,0x04,0x48},nis_flow_hook,0,0}
#endif
};
static int installed;
static uintptr_t nv_final_physics=0x5b4f60;
#ifdef NATIVE_NEXT
static uintptr_t nm_flush_style=0x56d670,nm_refresh_board=0x56dd30,nm_build_results=0x54cbe0,nm_results_target=0x85036c;
#endif
#ifdef COOP_HOOK_TEST
static uint32_t nc_test_base;
/* Test-only relocation of patch sites. Never present in the shipped ASI. */
void nc_test_rebase(uint32_t image_base){
    nc_test_base=image_base;
    for(unsigned i=0;i<sizeof patches/sizeof patches[0];i++)patches[i].va+=image_base-0x400000u;
    nv_final_physics+=image_base-0x400000u;
#ifdef NATIVE_NEXT
    nm_flush_style+=image_base-0x400000u;nm_refresh_board+=image_base-0x400000u;nm_build_results+=image_base-0x400000u;nm_results_target+=image_base-0x400000u;
#endif
}
#endif
static void jump(uint8_t *p,uintptr_t target){p[0]=0xe9;int32_t rel=(int32_t)(target-(uintptr_t)(p+5));memcpy(p+1,&rel,4);}
int nc_hooks_install(void){
    if(installed)return 1;
    const unsigned count=sizeof patches/sizeof patches[0];
    for(unsigned i=0;i<count;i++){
        Patch *p=&patches[i];MEMORY_BASIC_INFORMATION mi;
        if(!VirtualQuery((void*)(uintptr_t)p->va,&mi,sizeof mi)||mi.State!=MEM_COMMIT||
            (mi.Protect&(PAGE_NOACCESS|PAGE_GUARD))||memcmp((void*)(uintptr_t)p->va,p->expected,p->n))return 0;
    }
    uint8_t *tramp=VirtualAlloc(0,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!tramp)return 0;
    for(unsigned i=0;i<count;i++){
        patches[i].original=tramp+32*i;memcpy(tramp+32*i,patches[i].expected,patches[i].n);
        jump(tramp+32*i+patches[i].n,(uintptr_t)patches[i].va+patches[i].n);
    }
#if defined(COOP_HOOK_TEST) && defined(NATIVE_NEXT)
    /* Only synthetic images relocate copied absolute loads. */
    {uint32_t a=nc_test_base+0x490088;memcpy(tramp+32*11+1,&a,4);
     a=nc_test_base+0x3a5738;memcpy(tramp+32*12+5,&a,4);}
#endif
    DWORD previous;if(!VirtualProtect(tramp,4096,PAGE_EXECUTE_READ,&previous)){VirtualFree(tramp,0,MEM_RELEASE);return 0;}
#ifdef NATIVE_NEXT
    nr_credit_a_original=(uintptr_t)patches[7].original;nr_credit_b_original=(uintptr_t)patches[8].original;
    nr_credit_a_continue=patches[7].va+patches[7].n;nr_credit_b_continue=patches[8].va+patches[8].n;
#endif
    FlushInstructionCache(GetCurrentProcess(),tramp,4096);
    unsigned prepared=0;
    for(;prepared<count;prepared++)if(!VirtualProtect((void*)(uintptr_t)patches[prepared].va,patches[prepared].n,PAGE_EXECUTE_READWRITE,&patches[prepared].protection))break;
    if(prepared!=count){
        for(unsigned i=prepared;i>0;i--){Patch *p=&patches[i-1];DWORD unused;VirtualProtect((void*)(uintptr_t)p->va,p->n,p->protection,&unused);}
        VirtualFree(tramp,0,MEM_RELEASE);return 0;
    }
    for(unsigned i=0;i<count;i++){
        Patch *p=&patches[i];uint8_t *site=(void*)(uintptr_t)p->va;jump(site,(uintptr_t)p->target);
        for(int j=5;j<p->n;j++)site[j]=0x90;
        FlushInstructionCache(GetCurrentProcess(),site,p->n);
    }
    /* Sites may share an OS page (42c100 and 42cda0). Keep every page
     * writable until all writes finish, then undo protections in reverse
     * order so a later site's observed RWX never replaces the original RX. */
    for(unsigned i=count;i>0;i--){Patch *p=&patches[i-1];DWORD unused;VirtualProtect((void*)(uintptr_t)p->va,p->n,p->protection,&unused);}
#ifdef NATIVE_NEXT
    nc_owner_thread=GetCurrentThreadId();nm_virtual_continue=patches[17].va+patches[17].n;
#endif
    installed=1;return 1;
}
void nc_original_start(void){((void(__cdecl*)(void))patches[0].original)();}
void nc_original_state(uint32_t race,uint32_t state){((void(__attribute__((thiscall))*)(void*,uint32_t))patches[1].original)((void*)(uintptr_t)race,state);}
void nc_original_finish(uint32_t race,uint32_t index,uint32_t position,uint32_t flag,uint32_t completion){
    ((void(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,uint32_t,uint32_t))patches[4].original)((void*)(uintptr_t)race,index,position,flag,completion);
}
static void __attribute__((thiscall)) driver_hook(void *brain,float dt){
    int remote=nc_remote_brain((uint32_t)(uintptr_t)brain);
#ifdef NATIVE_NEXT
    remote|=nr_free_brain((uint32_t)(uintptr_t)brain);
#endif
    if(!remote)((void(__attribute__((thiscall))*)(void*,float))patches[2].original)(brain,dt);
}
static void __attribute__((thiscall)) physics_hook(void *car,uint32_t mode){
    /* Ghosts remain the default. The opt-in contact adapter retains native
     * physics, and disables its body before any engine-driven mode-zero reset. */
    if(!mode){
#ifdef NATIVE_NEXT
        if(np_contacts_enabled())np_before_zero((uint32_t)(uintptr_t)car);
#endif
        ((void(__attribute__((thiscall))*)(void*,uint32_t))patches[3].original)(car,mode);return;
    }
    int remote=nc_remote_car((uint32_t)(uintptr_t)car);
#ifdef NATIVE_NEXT
    remote|=nr_free_car((uint32_t)(uintptr_t)car);
#endif
    if(!remote
#ifdef NATIVE_NEXT
       ||np_contacts_enabled()
#endif
       )((void(__attribute__((thiscall))*)(void*,uint32_t))patches[3].original)(car,mode);
}
#ifdef NATIVE_NEXT
void ni_style_finalize(uint32_t score){((void(__attribute__((thiscall))*)(void*))nm_flush_style)((void*)(uintptr_t)score);}
static void __cdecl board_build_hook(void){
 ((void(__cdecl*)(void))patches[18].original)();
 if(GetCurrentThreadId()==nc_owner_thread)nm_downhill_board_built();
}
static void __attribute__((thiscall)) intro_hook(void *manager){
 if(GetCurrentThreadId()==nc_owner_thread&&ncp_intro_hold((uint32_t)(uintptr_t)manager))return;
 ((void(__attribute__((thiscall))*)(void*))patches[19].original)(manager);
}
int nc_original_nis(uint32_t flow,uint32_t action,uint32_t arg){
 return ((int(__attribute__((thiscall))*)(void*,uint32_t,uint32_t))patches[20].original)((void*)(uintptr_t)flow,action,arg);
}
static int __attribute__((thiscall)) nis_flow_hook(void *flow,uint32_t action,uint32_t arg){
 if(GetCurrentThreadId()==nc_owner_thread&&ncp_nis_hold((uint32_t)(uintptr_t)flow,action,arg))return 1;
 return nc_original_nis((uint32_t)(uintptr_t)flow,action,arg);
}
static void __attribute__((thiscall)) series_init_hook(void *series,uint32_t event){
 uint32_t mapped=GetCurrentThreadId()==nc_owner_thread?nm_downhill_event(event):event;
 ((void(__attribute__((thiscall))*)(void*,uint32_t))patches[15].original)(series,mapped);
}
static void __cdecl career_config_hook(uint32_t event,uint32_t refresh){
 uint32_t mapped=GetCurrentThreadId()==nc_owner_thread?nm_downhill_event(event):event;
 ((void(__cdecl*)(uint32_t,uint32_t))patches[16].original)(mapped,refresh);
}
__attribute__((used)) static uint32_t nm_virtual_id(uint32_t id){
 return GetCurrentThreadId()==nc_owner_thread&&id>=100&&id<=102&&nm_downhill_grid()?id+1:id;
}
/* Only the stored label changes. The native ESI value still selects the
 * original score multiplier and opponent, so vanilla challenge is preserved.
 * At entry [esp+38]/[esp+3c] are the two overwritten native stores. */
static void __attribute__((naked)) virtual_id_hook(void){__asm__ volatile(
 "pushfl\n\tpushal\n\tpush %esi\n\tcall _nm_virtual_id\n\tadd $4,%esp\n\t"
 "mov %eax,0x5c(%esp)\n\tmov 28(%esp),%eax\n\tmov %eax,0x60(%esp)\n\t"
 "popal\n\tpopfl\n\tjmp *_nm_virtual_continue\n");}
void ni_race_results(uint32_t race){((void(__attribute__((thiscall))*)(void*,uint32_t))nm_build_results)((void*)nm_results_target,race);}
static int __attribute__((thiscall)) series_continue_hook(void *series){
 int answer=GetCurrentThreadId()==nc_owner_thread?nm_can_continue((uint32_t)(uintptr_t)series):-1;
 if(answer>=0)return answer;
 return ((int(__attribute__((thiscall))*)(void*))patches[14].original)(series);
}
void ni_style_board(void){((void(__cdecl*)(void))nm_refresh_board)();}
static int __attribute__((thiscall)) career_result_hook(void *series,uint32_t finished){
 int answer=GetCurrentThreadId()==nc_owner_thread?nm_career_win((uint32_t)(uintptr_t)series):-1;
 if(answer>=0)return answer;
 return ((int(__attribute__((thiscall))*)(void*,uint32_t))patches[13].original)(series,finished);
}
int nc_original_world_paused(uint32_t world){
 return ((int(__attribute__((thiscall))*)(void*))patches[11].original)((void*)(uintptr_t)world);
}
static int __attribute__((thiscall)) pause_hook(void *world){
 uint32_t w=(uint32_t)(uintptr_t)world;
 return nc_pause_gate(w,nc_original_world_paused(w));
}
static float __attribute__((thiscall)) clock_hook(void *clock,float dt){
 uint32_t saved=0;int held=nc_pause_clock_enter((uint32_t)(uintptr_t)clock,&saved);
 float result=((float(__attribute__((thiscall))*)(void*,float))patches[12].original)(clock,held?0.f:dt);
 if(held)nc_pause_clock_leave(saved);
 return result;
}
static int __attribute__((thiscall)) engage_hook(void *manager){
    /* Player's Engage path asks this before setting race+c30 (solo Outrun
     * pending). Consume a network offer here; ordinary bot calls forward. */
    if(nr_engage_intercept())return 0;
    return ((int(__attribute__((thiscall))*)(void*))patches[10].original)(manager);
}
static void __attribute__((thiscall)) performance_hook(void *ride,uint32_t ai,uint32_t engine,uint32_t drivetrain,uint32_t handling){
 if(nv_owned_performance((uint32_t)(uintptr_t)ride)){
  /* SetRideInfo calls 639ce0 with AI=1 for a racer record. Its 6399d0
   * prepass replaces upgrades from an AI average, even when a full owner
   * configuration was just copied in. Preserve that configuration and run
   * only the same native final calculation, with its original three flags. */
  ((void(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,uint32_t))nv_final_physics)
      ((uint8_t*)ride+0x10,engine,drivetrain,handling);
 }else ((void(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,uint32_t,uint32_t))patches[9].original)
      (ride,ai,engine,drivetrain,handling);
}
static void __cdecl goals_hook(void){nr_goals((void(*)(void))patches[5].original);}
static void __attribute__((thiscall)) win_hook(void *career,uint32_t event){
    uint32_t saved=nr_cash_career;
    nr_cash_career=nr_reward_scope((uint32_t)(uintptr_t)career,event)?(uint32_t)(uintptr_t)career:0;
    ((void(__attribute__((thiscall))*)(void*,uint32_t))patches[6].original)(career,event);
    nr_cash_career=saved;
}
/* Only the two prize additions inside Career::EventWon are suppressed.
 * Other income, sponsor milestones and purchases run through their originals.
 * ESI is the native career pointer. Preserve every register on the normal
 * path and keep the native continuation/stack exactly intact. */
static void __attribute__((naked)) credit_a_hook(void){__asm__ volatile(
    "pushfl\n\tcmp %esi,_nr_cash_career\n\tje 1f\n\tpopfl\n\tjmp *_nr_credit_a_original\n"
    "1: popfl\n\tmov 0x7134(%esi),%edx\n\tadd $0,%edx\n\tjmp *_nr_credit_a_continue\n");}
static void __attribute__((naked)) credit_b_hook(void){__asm__ volatile(
    "pushfl\n\tcmp %esi,_nr_cash_career\n\tje 1f\n\tpopfl\n\tjmp *_nr_credit_b_original\n"
    "1: popfl\n\taddl $0,0x7134(%esi)\n\tjmp *_nr_credit_b_continue\n");}
#endif
static int key(int v,int *was){
    HWND fg=GetForegroundWindow();DWORD pid=0;if(fg)GetWindowThreadProcessId(fg,&pid);
    int held=pid==GetCurrentProcessId()&&(GetAsyncKeyState(v)&0x8000)!=0;
    int edge=held&&!*was;*was=held;return edge;
}
#ifdef NATIVE_NEXT
int nc_window_active(void){
#ifdef NATIVE_ENGINE_LAB
 /* Only the separately built local lab can run two live simulation windows.
  * Normal builds retain the focus/pause barrier. */
 HMODULE lab=GetModuleHandleA("LabBoot.asi");
 if(lab){int(__cdecl *enabled)(void)=(void*)GetProcAddress(lab,"LabSimulationEnabled");if(enabled&&enabled())return 1;}
#endif
 HWND fg=GetForegroundWindow();DWORD pid=0;if(fg)GetWindowThreadProcessId(fg,&pid);
 return pid==GetCurrentProcessId();
}
#endif
int nc_ready_key(void){static int was;return key(VK_F7,&was);}
int nc_cancel_key(void){static int was;return key(VK_F8,&was);}
