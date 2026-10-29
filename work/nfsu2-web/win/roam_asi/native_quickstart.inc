/* Launcher-only startup, executed by the existing guarded game-thread frame.
 * No worker, synthetic keys, save writes, autopilot or laboratory dependency. */
#include <string.h>
static struct {
 int enabled,done,loaded,left,city_requested;DWORD started,next,thread;
 char profile[32];unsigned char file[54966];
} NQ;
static void *nq_reader;
static uint32_t __attribute__((thiscall)) nq_load(void *profile,const void *data,uint32_t size,uint32_t apply){
 typedef uint32_t(__attribute__((thiscall))*Reader)(void*,const void*,uint32_t,uint32_t);
 int ours=NQ.enabled&&!NQ.done&&!NQ.loaded&&NQ.thread==GetCurrentThreadId()&&(uintptr_t)profile==0x83a9e0&&
          *(volatile uint32_t*)0x8654a4!=6;
 /* Intercept the normal native load instead of reloading car collections
  * under a live frontend preview. Native I/O completion stays in charge. */
 uint32_t result=((Reader)nq_reader)(profile,ours?NQ.file+36:data,ours?54930:size,apply);
 if(ours){
  NQ.loaded=!memcmp(profile,NQ.profile,32)&&*(volatile unsigned char*)0x86347c;
  if(!NQ.loaded){NQ.done=1;roam_log("[quickstart] native profile load verification failed\n");}
  else roam_log("[quickstart] selected profile %s loaded by native I/O completion\n",NQ.profile);
 }
 return result;
}
static uint32_t nq_u32(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint32_t nq_checksum(const unsigned char *p,unsigned n){
 if(n<4)return 0;
 uint32_t r=~((uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]);
 for(unsigned i=4;i<n;i++){
  uint32_t t=r&0xff000000u;for(unsigned k=0;k<8;k++)t=t&0x80000000u?(t<<1)^0x04c11db7u:t<<1;
  r=(r<<8|p[i])^t;
 }
 return ~r;
}
static uint32_t nq_read(uint32_t p){return readable(p,4)?*(volatile uint32_t*)(uintptr_t)p:0;}
static int nq_active(const char *name){
 uint32_t fe=nq_read(0x8384c4),mgr=nq_read(fe+8);
 if(!readable(mgr,0xf8))return 0;
 for(unsigned off=0xe4;off<=0xf4;off+=0x10){
  uint32_t p=nq_read(mgr+off);
  for(unsigned i=0;p&&i<64&&readable(p,16);i++,p=nq_read(p+4)){
   uint32_t text=nq_read(p+12);
   if(text&&readable(text,64)&&!lstrcmpiA((const char*)(uintptr_t)text,name))return 1;
  }
 }
 return 0;
}
static uint32_t nq_hash(const char *s){uint32_t h=~0u;for(;*s;s++)h=h*33+(unsigned char)*s;return h;}
static int nq_notify(const char *name,uint32_t msg){
 uint32_t args[4]={msg,0,1,0};uint32_t arg=(uint32_t)(uintptr_t)name;
 uint32_t rec=roam_win_call(0x51bad0,0x8637b0,0,1,&arg);
 uint32_t obj=nq_read(rec+0x14),table=nq_read(obj),fn=nq_read(table+8);
 if(!obj||!readable(obj,0x20)||fn<EXE_TEXT_LO||fn>=EXE_TEXT_HI)return 0;
 roam_win_call(fn,obj,0,4,args);return 1;
}
static void nq_leave_crib(void){
 if(NQ.city_requested)return;
 uint32_t arg=(uint32_t)(uintptr_t)"UI_CareerCrib.fng";
 uint32_t rec=roam_win_call(0x51bad0,0x8637b0,0,1,&arg);
 uint32_t obj=nq_read(rec+0x14);
 /* Exact CareerCrib receiver, after native menu initialization. Generic
  * ACCEPT only starts the selected button animation; it is not the crib's
  * career-entry action. Use its native save/check/queued-entry path, once.
  * Do not directly call StartCareer or synthesize memory-card completion. */
 if(!readable(obj,0x150)||nq_read(obj)!=0x797548||
    !*(volatile unsigned char*)(uintptr_t)(obj+0x7b)||nq_read(obj+0x14c))return;
 NQ.city_requested=1;
 roam_log("[quickstart] garage ready; requesting native career entry once\n");
 roam_win_call(0x4af260,obj,0,0,0);
}
__attribute__((naked)) static void nq_skip_movies(void){
 __asm__ volatile("movl $1,0x8650a8\n\tpushl $0x5805d8\n\tret");
}
static void nq_init(void){
 wchar_t path[32768];char request[8];
 if(GetEnvironmentVariableA("NFSU2_COOP_QUICKSTART",request,sizeof request)!=1||request[0]!='1')return;
 DWORD len=GetEnvironmentVariableW(L"NFSU2_COOP_PROFILE",path,32768);
 if(!len||len>=32768)return;
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 DWORD got=0;int valid=f!=INVALID_HANDLE_VALUE;
 if(valid){valid=GetFileSize(f,0)==sizeof NQ.file&&ReadFile(f,NQ.file,sizeof NQ.file,&got,0)&&got==sizeof NQ.file;CloseHandle(f);}
 unsigned char *b=NQ.file;
 if(!valid||memcmp(b,"20CM",4)||nq_u32(b+4)!=54966||nq_u32(b+8)!=8||nq_u32(b+12)!=54930||
    nq_u32(b+28)!=0x10d||nq_u32(b+32)!=54930||nq_u32(b+16)!=nq_checksum(b+28,8)||
    nq_u32(b+20)!=nq_checksum(b+36,54930)||nq_u32(b+24)!=nq_checksum(b,24)||!b[0xad7d]){
  roam_log("[quickstart] disabled: profile is invalid or has no existing career\n");return;
 }
 unsigned n=0;for(;n<31&&b[0xd225+n];n++){
  unsigned char ch=b[0xd225+n];
  if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'||ch==' '))return;
  NQ.profile[n]=(char)ch;
 }
 if(!n||(n==31&&b[0xd225+n]))return;
 /* Preserve the startup parser's registers/flags; override only SkipMovies.
  * Signature must be pristine. Do not overwrite another mod's boot hook. */
 static const unsigned char expected[5]={0xa3,0xa8,0x50,0x86,0};
 static const unsigned char prologue[6]={0x55,0x8b,0xec,0x83,0xec,0x24};
 unsigned char *site=(void*)0x5805d3,*reader=(void*)0x534420;DWORD old,old_reader;
 if(!readable(0x5805d3,5)||memcmp(site,expected,5)||!readable(0x534420,6)||memcmp(reader,prologue,6)){
  roam_log("[quickstart] disabled: startup/profile reader site differs\n");return;
 }
 unsigned char *tramp=VirtualAlloc(0,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!tramp)return;
 memcpy(tramp,prologue,6);tramp[6]=0xe9;
 int32_t rel=(int32_t)(0x534426u-((uintptr_t)tramp+11));memcpy(tramp+7,&rel,4);
 DWORD unused;
 if(!VirtualProtect(tramp,32,PAGE_EXECUTE_READ,&unused)){VirtualFree(tramp,0,MEM_RELEASE);return;}
 if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(tramp,0,MEM_RELEASE);return;}
 if(!VirtualProtect(reader,6,PAGE_EXECUTE_READWRITE,&old_reader)){
  VirtualProtect(site,5,old,&unused);VirtualFree(tramp,0,MEM_RELEASE);return;
 }
 nq_reader=tramp;
 site[0]=0xe9;rel=(int32_t)((uintptr_t)nq_skip_movies-0x5805d8u);memcpy(site+1,&rel,4);
 reader[0]=0xe9;rel=(int32_t)((uintptr_t)nq_load-0x534425u);memcpy(reader+1,&rel,4);reader[5]=0x90;
 FlushInstructionCache(GetCurrentProcess(),tramp,32);
 FlushInstructionCache(GetCurrentProcess(),site,5);FlushInstructionCache(GetCurrentProcess(),reader,6);
 VirtualProtect(reader,6,old_reader,&unused);VirtualProtect(site,5,old,&unused);
 NQ.enabled=1;
 roam_log("[quickstart] enabled for existing profile %s; startup only, native career load\n",NQ.profile);
}
static void nq_frame(void){
 if(!NQ.enabled||NQ.done)return;
 if(!NQ.thread)NQ.thread=GetCurrentThreadId();
 if(NQ.thread!=GetCurrentThreadId())return;
 DWORD now=GetTickCount();if(!NQ.started)NQ.started=now;
 if(now-NQ.started>120000){NQ.done=1;roam_log("[quickstart] stopped after timeout; use native menu\n");return;}
 if(nq_read(0x8654a4)==6&&nq_active("HUD_SingleRace.fng")){
  NQ.done=1;roam_log("[quickstart] complete: career city ready, automatic UI stops\n");return;
 }
 if((int32_t)(now-NQ.next)<0)return;NQ.next=now+500;
 /* Memory-card completion only. Never confirm save/create/delete dialogs,
  * and never invoke MC_Main's generic button handler with a null UI object. */
 if(nq_active("MC_Main.fng")){
  uint32_t card=nq_read(0x836498),flags=nq_read(0x836514);
  if((flags&0x0f000000)==0x03000000&&card&&readable(card,0x1c)&&!nq_read(card+0x18))
   nq_notify("MC_Main.fng",0x8867412d);
  return;
 }
 if(nq_active("UI_Main.fng")){
  if(!NQ.loaded)return;
  if(!NQ.left){if(nq_notify("UI_Main.fng",nq_hash("PAD_LEFT")))NQ.left=1;}
  else nq_notify("UI_Main.fng",0x0c407210);
 }else if(NQ.loaded&&nq_active("UI_StartCareer.fng"))nq_notify("UI_StartCareer.fng",0x0c407210);
 else if(NQ.loaded&&nq_active("UI_CareerCrib.fng"))nq_leave_crib();
 else if(nq_active("UG_LS_Splash.fng"))nq_notify("UG_LS_Splash.fng",nq_hash("PAD_ACCEPT"));
 else if(nq_active("MC_Background.fng"))nq_notify("MC_Background.fng",nq_hash("PAD_ACCEPT"));
}
