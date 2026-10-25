/* Executes only synthetic tails, never gameplay code. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_coop.h"
void nc_test_rebase(uint32_t image_base);
static int block,starts,states,brains,cars,finishes;
#ifdef NATIVE_NEXT
#define HOOKS 18
static int continue_answer=-1,map_event,virtual_scope,map_calls;
int nm_can_continue(uint32_t p){if(!p)abort();return continue_answer;}
uint32_t nm_downhill_event(uint32_t p){map_calls++;return map_event?p+16:p;}
int nm_downhill_grid(void){return virtual_scope;}
/* Link adapters only; the existing fixture does not exercise these hooks. */
void nm_downhill_board_built(void){}
int ncp_intro_hold(uint32_t manager){(void)manager;return 0;}
int ncp_nis_hold(uint32_t flow,uint32_t action){(void)flow;(void)action;return 0;}
static int mode_finish_block,mode_finish_calls,career_answer=-1;
int nm_finish(uint32_t r,uint32_t i,uint32_t p,uint32_t f,uint32_t c){if(r!=0x12345678||i!=2||p!=1||f||c!=1)abort();mode_finish_calls++;return mode_finish_block;}
int nm_career_win(uint32_t p){if(p!=0x12345678)abort();return career_answer;}
static int pause_queries,clock_enters,clock_leaves;
int nc_pause_gate(uint32_t world,int original){if(world!=0x12345678||original!=123)abort();pause_queries++;return block?1:original;}
int nc_pause_clock_enter(uint32_t clock,uint32_t *saved){if(clock!=0x12345678)abort();clock_enters++;*saved=321;return block;}
void nc_pause_clock_leave(uint32_t saved){if(saved!=321)abort();clock_leaves++;}
int nr_engage_intercept(void){return block;}
static int contacts,zero_calls;
int np_contacts_enabled(void){return contacts;}
void np_before_zero(uint32_t car){if(car!=0x12345678)abort();zero_calls++;}
static int goals;
static struct {uint32_t series,config,virtuals,next,career;void *vehicle,*money;int next_result,career_result;} thread_args;
static DWORD WINAPI mode_thread(void *unused){
 (void)unused;
 ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)thread_args.series)(thread_args.vehicle,0x88);
 ((void(__cdecl*)(uint32_t,uint32_t))(uintptr_t)thread_args.config)(0x88,0x99);
 ((void(__cdecl*)(void))(uintptr_t)thread_args.virtuals)();
 thread_args.next_result=((int(__attribute__((thiscall))*)(void*))(uintptr_t)thread_args.next)(thread_args.money);
 thread_args.career_result=((int(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)thread_args.career)((void*)0x12345678,0x10203040);
 return 0;
}
static uint8_t money[0x7140];
static uint32_t credit_site_a,credit_site_b;
static int wins;
int nr_reward_scope(uint32_t c,uint32_t event){if(c!=(uint32_t)(uintptr_t)money||event!=0x88)abort();return block;}
void nr_reward_solo_start(void){}
void nv_solo_start(void){}
int nes_block_solo_start(void){return 0;}
static uint32_t vehicle[16];
int nv_owned_performance(uint32_t ride){if(ride!=(uint32_t)(uintptr_t)vehicle)abort();return block;}
static void __attribute__((naked)) run_credit(uint32_t fn,uint32_t career,uint32_t amount){
 __asm__ volatile("push %esi\n\tmov 12(%esp),%esi\n\tmov 16(%esp),%eax\n\tmov %eax,%ecx\n\tcall *8(%esp)\n\tpop %esi\n\tret\n");
}
static void simulate_native_win(uint32_t career){
 run_credit(credit_site_a,career,200);run_credit(credit_site_b,career,300);
 *(uint32_t*)(money+0x7134)+=17; /* unrelated milestone income must survive */
 wins++;
}
int nr_free_car(uint32_t p){if(p!=0x12345678)abort();return 0;}
int nr_free_brain(uint32_t p){if(p!=0x12345678)abort();return 0;}
void nr_goals(void(*original)(void)){(void)original;goals++;}
#else
#define HOOKS 5
#endif
int nc_start(void){starts++;return block;}
int nc_state(uint32_t r,uint32_t s){if(r!=0x12345678||s!=4)abort();states++;return block;}
int nc_remote_brain(uint32_t b){if(b!=0x12345678)abort();brains++;return block;}
int nc_remote_car(uint32_t c){if(c!=0x12345678)abort();cars++;return block;}
int nc_finish_block(uint32_t r){if(r!=0x12345678)abort();return block;}
void nc_finished(uint32_t r,uint32_t i){if(r!=0x12345678||i!=2)abort();finishes++;}
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 uint8_t *image=VirtualAlloc(NULL,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
 if(!image){
  DWORD error=GetLastError();MEMORY_BASIC_INFORMATION mi;VirtualQuery((void*)0x400000,&mi,sizeof mi);
  printf("allocation error %lu; self=%p base=%p allocation=%p size=%lu state=%lx\n",error,GetModuleHandleA(0),mi.BaseAddress,mi.AllocationBase,(unsigned long)mi.RegionSize,(unsigned long)mi.State);return 2;
 }
 uint32_t image_base=(uint32_t)(uintptr_t)image;nc_test_rebase(image_base);
 CHECK(argc==3);FILE *f=fopen(argv[1],"rb");CHECK(f!=0);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);uint8_t *raw=malloc(size);CHECK(fread(raw,1,size,f)==(size_t)size);fclose(f);
 IMAGE_DOS_HEADER *dos=(void*)raw;IMAGE_NT_HEADERS32 *nt=(void*)(raw+dos->e_lfanew);
 memcpy(image,raw,nt->OptionalHeader.SizeOfHeaders);IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)memcpy(image+sec[i].VirtualAddress,raw+sec[i].PointerToRawData,sec[i].SizeOfRawData);
 uint32_t va[]={0x499ae0,0x5fc900,0x42c100,0x5f46a0,0x606cb0,0x42cda0,0x541380,0x5413fc,0x54143a,0x639ce0,0x500d70,0x5ea320,0x601a60,0x528730,0x513f00,0x532df0,0x532240,0x56dabd};const unsigned n[]={5,5,5,7,6,5,5,8,6,6,6,5,9,6,6,9,8,8};
 for(int i=0;i<HOOKS;i++)va[i]+=image_base-0x400000u;
 if(atoi(argv[2])==-2){
  uint8_t *copy=malloc(0x600000);
  for(int i=0;i<HOOKS;i++){
   image[va[i]-image_base]^=1;memcpy(copy,image,0x600000);
   CHECK(!nc_hooks_install());CHECK(!memcmp(copy,image,0x600000));image[va[i]-image_base]^=1;
   printf("hooks negative %d: PASS, no game image changes\n",i);
  }
  free(copy);
 }
 int bad=atoi(argv[2]);if(bad>=0&&bad<HOOKS)image[va[bad]-image_base]^=1;
 uint8_t *before=malloc(0x600000);memcpy(before,image,0x600000);
 DWORD protection;CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READ,&protection));
 int installed=nc_hooks_install();
 if(bad>=0){CHECK(!installed);CHECK(!memcmp(before,image,0x600000));printf("hooks negative %d: PASS, no game image changes\n",bad);return 0;}
 CHECK(installed);CHECK(nc_hooks_install());
 for(int i=0;i<HOOKS;i++){MEMORY_BASIC_INFORMATION mi;CHECK(VirtualQuery((void*)(uintptr_t)va[i],&mi,sizeof mi));CHECK(mi.Protect==PAGE_EXECUTE_READ);}
 unsigned changes=0;
 for(unsigned a=0;a<0x600000;a++)if(image[a]!=before[a]){
  int inside=0;for(int i=0;i<HOOKS;i++)if(a>=va[i]-image_base&&a<va[i]-image_base+n[i])inside=1;
  CHECK(inside);changes++;
 }
 CHECK(changes>0);
 CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READWRITE,&protection));
 /* Replace the untouched continuation with a minimal epilogue matching
  * each copied prologue, so forwarding exercises trampoline/ABI only. */
 const uint8_t tail[5][12]={{0xc3},{0x5d,0xc2,4,0},{0x5e,0x5b,0x83,0xc4,0x14,0xc2,4,0},{0x83,0xc4,8,0xc2,4,0},{0x5b,0x59,0xc2,16,0}};
 for(int i=0;i<5;i++)memcpy((void*)(uintptr_t)(va[i]+n[i]),tail[i],sizeof tail[i]);
#ifdef NATIVE_NEXT
 credit_site_a=va[7];credit_site_b=va[8];
 const uint8_t credit_tail[]={0x89,0x96,0x34,0x71,0,0,0xc3};
 memcpy((void*)(uintptr_t)(va[7]+n[7]),credit_tail,sizeof credit_tail);
 *(uint8_t*)(uintptr_t)(va[8]+n[8])=0xc3;
 uint8_t win_tail[]={0x56,0xb8,0,0,0,0,0xff,0xd0,0x83,0xc4,4,0x5e,0x5b,0x59,0xc2,4,0};
 uint32_t helper=(uint32_t)(uintptr_t)simulate_native_win;memcpy(win_tail+2,&helper,4);
 memcpy((void*)(uintptr_t)(va[6]+n[6]),win_tail,sizeof win_tail);
 const uint8_t performance_tail[]={0xff,0x46,4,0x5e,0xc2,16,0};
 memcpy((void*)(uintptr_t)(va[9]+n[9]),performance_tail,sizeof performance_tail);
 /* Synthetic final calculator records ECX and all three args, ret 12. */
 const uint8_t calculate[]={0xff,0x41,4,0x8b,0x44,0x24,4,0x89,0x41,8,0x8b,0x44,0x24,8,0x89,0x41,12,0x8b,0x44,0x24,12,0x89,0x41,16,0xc2,12,0};
 memcpy(image+0x1b4f60,calculate,sizeof calculate);
 const uint8_t engage_tail[]={0x8b,0xc2,0xc3}; /* EAX = manager+1ae8 */
 memcpy((void*)(uintptr_t)(va[10]+n[10]),engage_tail,sizeof engage_tail);
 *(uint32_t*)(image+0x490088)=123;*(float*)(image+0x3a5738)=1.f;
 *(uint8_t*)(uintptr_t)(va[11]+n[11])=0xc3;
 const uint8_t clock_tail[]={0xd8,0x4c,0x24,0x10,0x83,0xc4,0x0c,0xc2,4,0};
 memcpy((void*)(uintptr_t)(va[12]+n[12]),clock_tail,sizeof clock_tail);
 const uint8_t career_tail[]={0x8b,0xc2,0x33,0x45,8,0xc9,0xc2,4,0};
 memcpy((void*)(uintptr_t)(va[13]+n[13]),career_tail,sizeof career_tail);
 *(uint8_t*)(uintptr_t)(va[14]+n[14])=0xc3;
 const uint8_t series_tail[]={0x8b,0x45,8,0x89,0x01,0xc9,0xc2,4,0};
 memcpy((void*)(uintptr_t)(va[15]+n[15]),series_tail,sizeof series_tail);
 uint8_t config_tail[]={0x8b,0x45,8,0xa3,0,0,0,0,0x8b,0x45,12,0xa3,0,0,0,0,0xc9,0xc3};
 uint32_t dest=(uint32_t)(uintptr_t)(vehicle+1);memcpy(config_tail+4,&dest,4);dest+=4;memcpy(config_tail+12,&dest,4);
 memcpy((void*)(uintptr_t)(va[16]+n[16]),config_tail,sizeof config_tail);
 *(uint8_t*)(uintptr_t)(va[17]+n[17])=0xc3;
 const uint8_t results_tail[]={0x8b,0x44,0x24,4,0x89,0x01,0xc2,4,0};memcpy(image+0x14cbe0,results_tail,sizeof results_tail);
 static uint32_t observed[4];
 uint8_t vthunk[]={0x56,0x83,0xec,0x48,0xbe,100,0,0,0,0xb8,0x34,0x12,0,0,0xe8,0,0,0,0,0x89,0x35,0,0,0,0,0xa3,0,0,0,0,0x8b,0x54,0x24,0x34,0x89,0x15,0,0,0,0,0x8b,0x54,0x24,0x38,0x89,0x15,0,0,0,0,0x83,0xc4,0x48,0x5e,0xc3};
 int32_t rel=(int32_t)(va[17]-(image_base+0x3000+19));memcpy(vthunk+15,&rel,4);
 unsigned at[]={21,26,36,46};for(int i=0;i<4;i++){dest=(uint32_t)(uintptr_t)(observed+i);memcpy(vthunk+at[i],&dest,4);}memcpy(image+0x3000,vthunk,sizeof vthunk);
 const uint8_t style_tail[]={0xff,0x01,0xc3};memcpy(image+0x16d670,style_tail,sizeof style_tail);
 uint8_t board_tail[]={0xff,0x05,0,0,0,0,0xc3};uint32_t board_count=(uint32_t)(uintptr_t)(vehicle+15);memcpy(board_tail+2,&board_count,4);memcpy(image+0x16dd30,board_tail,sizeof board_tail);
#endif
 for(block=0;block<=1;block++){
  /* Its absolute data read still targets the original game. Test interception
   * here; forwarding of that original prologue requires the fixed-address run. */
  int saved=block;block=1;((void(__cdecl*)(void))(uintptr_t)va[0])();block=saved;
  ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[1])((void*)0x12345678,4);
  ((void(__attribute__((thiscall))*)(void*,float))(uintptr_t)va[2])((void*)0x12345678,0.02f);
  ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[3])((void*)0x12345678,1);
  ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[3])((void*)0x12345678,0);
  ((void(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)va[4])((void*)0x12345678,2,1,0,1);
#ifdef NATIVE_NEXT
  ((void(*)(void))(uintptr_t)va[5])();
  *(uint32_t*)(money+0x7134)=1000;
  ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[6])(money,0x88);
  CHECK(*(uint32_t*)(money+0x7134)==(block?1017u:1517u));
  /* Outside the win callback both original native credit paths still work. */
  uint32_t before_credit=*(uint32_t*)(money+0x7134);
  run_credit(va[7],(uint32_t)(uintptr_t)money,40);run_credit(va[8],(uint32_t)(uintptr_t)money,50);
  CHECK(*(uint32_t*)(money+0x7134)==before_credit+90);
  memset(vehicle,0,sizeof vehicle);vehicle[0]=1;
  ((void(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)va[9])(vehicle,1,7,8,9);
  CHECK(vehicle[1]==(block?0u:1u));CHECK(vehicle[5]==(block?1u:0u));
  if(block)CHECK(vehicle[6]==7&&vehicle[7]==8&&vehicle[8]==9);
  uint8_t manager[0x1aec]={0};*(uint32_t*)(manager+0x1ae8)=123;
  int eligibility=((int(__attribute__((thiscall))*)(void*))(uintptr_t)va[10])(manager);
  CHECK(eligibility==(block?0:123));
  int paused=((int(__attribute__((thiscall))*)(void*))(uintptr_t)va[11])((void*)0x12345678);
  CHECK(paused==(block?1:123));
  float step=((float(__attribute__((thiscall))*)(void*,float))(uintptr_t)va[12])((void*)0x12345678,.25f);
  CHECK(step==(block?0.f:.25f));
#endif
 }
 CHECK(starts==2&&states==2&&brains==2&&cars==2&&finishes==1);
#ifdef NATIVE_NEXT
 contacts=1;block=1;
 ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[3])((void*)0x12345678,3);
 ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[3])((void*)0x12345678,0);
 CHECK(cars==3&&zero_calls==1);
 CHECK(pause_queries==2&&clock_enters==2&&clock_leaves==1);
 CHECK(mode_finish_calls==2);
 mode_finish_block=1;
 ((void(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)va[4])((void*)0x12345678,2,1,0,1);
 CHECK(mode_finish_calls==3&&finishes==1);
 for(career_answer=-1;career_answer<=1;career_answer++){
  int result=((int(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[13])((void*)0x12345678,0x10203040);
  CHECK(result==(career_answer<0?(int)(0x12345678u^0x10203040u):career_answer));
 }
 *(uint32_t*)(money+0x34e4)=123;
 for(continue_answer=-1;continue_answer<=1;continue_answer++){
  int result=((int(__attribute__((thiscall))*)(void*))(uintptr_t)va[14])(money);CHECK(result==(continue_answer<0?123:continue_answer));
 }
 for(map_event=0;map_event<2;map_event++){
  vehicle[0]=vehicle[1]=vehicle[2]=0;
  ((void(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)va[15])(vehicle,0x88);
  ((void(__cdecl*)(uint32_t,uint32_t))(uintptr_t)va[16])(0x88,0x99);
  CHECK(vehicle[0]==(map_event?0x98u:0x88u)&&vehicle[1]==vehicle[0]&&vehicle[2]==0x99);
 }
 CHECK(map_calls==4);
 for(virtual_scope=0;virtual_scope<2;virtual_scope++){
  memset(observed,0,sizeof observed);((void(__cdecl*)(void))(image+0x3000))();
  CHECK(observed[0]==100&&observed[1]==0x1234&&observed[2]==(virtual_scope?101u:100u)&&observed[3]==0x1234);
 }
 ni_race_results(0xabcdef);CHECK(*(uint32_t*)(image+0x45036c)==0xabcdef);
 map_event=virtual_scope=career_answer=continue_answer=1;vehicle[0]=vehicle[1]=0;memset(observed,0,sizeof observed);
 thread_args.series=va[15];thread_args.config=va[16];thread_args.virtuals=image_base+0x3000;thread_args.next=va[14];thread_args.career=va[13];thread_args.vehicle=vehicle;thread_args.money=money;
 HANDLE worker=CreateThread(0,0,mode_thread,0,0,0);CHECK(worker!=0);CHECK(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);DWORD code;CHECK(GetExitCodeThread(worker,&code)&&code==0);CloseHandle(worker);
 CHECK(map_calls==4&&vehicle[0]==0x88&&vehicle[1]==0x88&&vehicle[2]==0x99);
 CHECK(observed[0]==100&&observed[1]==0x1234&&observed[2]==100&&observed[3]==0x1234);
 CHECK(thread_args.next_result==123&&thread_args.career_result==(int)(0x12345678u^0x10203040u));
 uint32_t style_count=0;vehicle[15]=0;ni_style_finalize((uint32_t)(uintptr_t)&style_count);ni_style_board();CHECK(style_count==1&&vehicle[15]==1);
 CHECK(goals==2&&wins==2);puts("native extra hooks: PASS, goal interception; scoped prize suppression; independent bonuses; scoped AI-upgrade bypass forwards exact ride+10 and three calculation flags, ordinary bot path retained");
#endif
 puts("hooks positive: PASS, patch ranges, idempotence, four original forwarding paths and five interception paths (relocated image, synthetic epilogues)");return 0;
}
