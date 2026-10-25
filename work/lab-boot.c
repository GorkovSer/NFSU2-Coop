/* Disposable lab only: exact local EXE signatures, no production installation. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static int patched;
static char ini[MAX_PATH],logfile[MAX_PATH],controlfile[MAX_PATH],statefile[MAX_PATH];
static uint32_t next_action,started;static int finished,positioned,main_left;
static unsigned boot_serial;
static unsigned applied_jump,applied_race;static int race_result,autopilot;
static int lab_drive;static uint32_t lab_driven_car,lab_yield_car,lab_yield_race;static unsigned lab_yield_serial;
static int readable(uintptr_t p,unsigned n){
 MEMORY_BASIC_INFORMATION m;
 return p&&VirtualQuery((void*)p,&m,sizeof m)&&m.State==MEM_COMMIT&&
 !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&p+n>=p&&p+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t read32(uintptr_t p){return readable(p,4)?*(uint32_t*)p:0;}
static void note(const char *what){FILE*f=fopen(logfile,"a");if(f){fprintf(f,"%lu %s flow=%u\n",GetTickCount(),what,read32(0x8654a4));fclose(f);}}
static int active(const char *name){
 uint32_t fe=read32(0x8384c4),mgr=read32(fe+8);
 if(!readable(mgr,0xf8))return 0;
 for(unsigned head=0xe4;head<=0xf4;head+=0x10){
  uint32_t p=read32(mgr+head);
  for(int k=0;p&&k<64&&readable(p,0x10);k++,p=read32(p+4)){
   uint32_t str=read32(p+0xc);if(readable(str,64)&&!strnicmp((char*)str,name,64))return 1;
  }
 }
 return 0;
}
static uint32_t hash(const char*s){uint32_t h=~0u;for(;*s;s++)h=h*33+(unsigned char)*s;return h;}
/* Release only this explicitly assigned lab driver BEFORE native FinishRacer
 * classifies the local participant. Never switch controllers after results. */
__declspec(dllexport) void __cdecl NativeLabYieldDriver(uint32_t car){
 uint32_t player=read32(0x8900ac),race=read32(0x890118);
 if(!lab_drive||!car||car!=lab_driven_car||read32(player+4)!=car||
    read32(0x8654a4)!=6||!read32(0x863470)||read32(race+0x24)<2||!readable(car,0x9e0))return;
 ((void(__attribute__((thiscall))*)(void*))0x5faf10)((void*)player);
 lab_drive=autopilot=0;lab_driven_car=0;lab_yield_car=car;lab_yield_race=race;lab_yield_serial=applied_race;
 note("lab driver yielded before native finish");
}
static void controls(void){
 static DWORD next;static unsigned jump,race_serial;
 DWORD now=GetTickCount();if((int32_t)(now-next)<0||read32(0x8654a4)!=6)return;next=now+100;
 uint32_t player=read32(0x8900ac),car=read32(player+4);
 if(!readable(player,0x10)||!readable(car,0x9e0))return;
 unsigned request=GetPrivateProfileIntA("Lab","RaceSerial",0,controlfile);
 if(request!=race_serial){
  race_serial=request;
  HMODULE module=GetModuleHandleA("NFSU2FreeRoam.asi");
  typedef int(__cdecl *RaceCommand)(unsigned,uint32_t);
  RaceCommand command=module?(RaceCommand)GetProcAddress(module,"NativeLabRaceCommand"):0;
  int result=command?command(GetPrivateProfileIntA("Lab","RaceCommand",0,controlfile),GetPrivateProfileIntA("Lab","RaceEvent",0,controlfile)):-99;
  applied_race=request;race_result=result;
  char text[100];snprintf(text,sizeof text,"lab race command result=%d serial=%u",result,request);note(text);
 }
 unsigned serial=GetPrivateProfileIntA("Lab","JumpSerial",0,controlfile);
 if(serial!=jump&&!read32(0x863470)){
  char text[128];float pos[3];int valid=1;
  const char *keys[]={"X","Y","Z"};
  for(int k=0;k<3;k++){GetPrivateProfileStringA("Lab",keys[k],"nan",text,sizeof text,controlfile);pos[k]=(float)atof(text);if(!isfinite(pos[k])||fabsf(pos[k])>20000)valid=0;}
  jump=serial;
  if(valid){
   memcpy((void*)0x890140,pos,sizeof pos);
   *(uint16_t*)0x8900d0=(uint16_t)GetPrivateProfileIntA("Lab","Heading",0,controlfile);
   *(uint8_t*)0x8900ce=1;*(uint8_t*)0x8900cd=1;note("lab native hot-position request");
   applied_jump=serial;
  }
 }
 int wanted=!!GetPrivateProfileIntA("Lab","AutoDrive",0,controlfile);
 uint32_t race=read32(0x890118);
 /* A native finish takes ownership of the car for its results/cinematic.
  * Do not restore a human controller from this lab toggle on that screen. */
 unsigned racers=read32(race+0x24);int completed=0;
 if(read32(0x863470)&&racers>=2&&racers<=6&&readable(race,0xc44)){
  for(unsigned i=0;i<racers;i++){
   uint32_t rec=race+0x30+0x1f0*i;
   if(read32(rec)==car&&(read32(rec+0x10)&0xffffu)){completed=1;break;}
  }
 }
 if(completed){
  if(lab_drive)note("lab driver released to native post-race controller");
  lab_drive=autopilot=0;lab_driven_car=0;return;
 }
 /* Career's selected-event pointer is set before the old one-car city
  * world is replaced. Enabling AutoDrive there cancels the queued start. */
 if(read32(0x863470)&&read32(race)==4&&read32(race+0x24)>1&&GetPrivateProfileIntA("Lab","AutoRace",0,controlfile))wanted=1;
 if(car==lab_yield_car&&race==lab_yield_race&&applied_race==lab_yield_serial)wanted=0;
 if(wanted!=lab_drive||(wanted&&lab_driven_car!=car)){
  typedef void(__attribute__((thiscall))*Auto)(void*);
  ((Auto)(wanted?0x5fae20:0x5faf10))((void*)player);lab_drive=wanted;lab_driven_car=car;note(wanted?"lab native auto-lab_drive on":"lab native auto-lab_drive off");
  autopilot=wanted;
 }
}
static void status(void){
 static DWORD next;DWORD now=GetTickCount();if((int32_t)(now-next)<0)return;next=now+500;
 uint32_t car=read32(read32(0x8900ac)+4),race=read32(0x890118);float p[3]={0};
 if(readable(car+0x60,12))memcpy(p,(void*)(car+0x60),12);
 char temp[MAX_PATH];snprintf(temp,sizeof temp,"%s.tmp",statefile);
 FILE*f=fopen(temp,"w");if(!f)return;
 fprintf(f,"{\"pid\":%lu,\"tick\":%lu,\"flow\":%u,\"event\":%u,\"race_state\":%u,\"racers\":%u,\"car\":%u,\"driver\":%u,\"position\":[%.6f,%.6f,%.6f],\"jump_serial\":%u,\"race_serial\":%u,\"race_result\":%d,\"autopilot\":%d}",
  GetCurrentProcessId(),now,read32(0x8654a4),read32(0x863470),read32(race),read32(race+0x24),car,read32(car+0x24),p[0],p[1],p[2],applied_jump,applied_race,race_result,autopilot);
 fclose(f);MoveFileExA(temp,statefile,MOVEFILE_REPLACE_EXISTING);
}
static void notify(const char *name,uint32_t msg){
 typedef uint32_t(__attribute__((thiscall))*Find)(void*,const char*);
 typedef void(__attribute__((thiscall))*Notify)(void*,uint32_t,uint32_t,uint32_t,uint32_t);
 uint32_t rec=((Find)0x51bad0)((void*)0x8637b0,name),obj=read32(rec+0x14),table=read32(obj),fn=read32(table+8);
 if(!readable(obj,0x20)||fn<0x400000||fn>=0x780000){note("autostart: no live screen controller");return;}
 ((Notify)(uintptr_t)fn)((void*)obj,msg,0,1,0);note(name);
}
static void message(const char *name,const char *s){
 /* Native menu packages translate PAD_ACCEPT to generic button selection.
  * Direct NotificationMessage calls must use the translated event. */
 uint32_t msg=hash(s);
 if(!strcmp(s,"PAD_ACCEPT")){
  if(!strcmp(name,"MC_Main.fng"))msg=0x8867412d;
  else if(!strncmp(name,"UI_",3))msg=0x0c407210;
 }
 notify(name,msg);note(s);
}
static void tick(void){
 controls();
 status();
 unsigned requested_boot=GetPrivateProfileIntA("Lab","BootSerial",0,controlfile);
 if(requested_boot!=boot_serial){
  boot_serial=requested_boot;started=next_action=0;finished=main_left=0;
  note("lab free-roam entry requested");
 }
 HWND wnd=(HWND)(uintptr_t)read32(0x870990);
 if(!positioned&&wnd){
  RECT rc;if(GetWindowRect(wnd,&rc)){
   int host=GetPrivateProfileIntA("FreeRoam","CoopHost",0,ini);
   int x=host?0:GetSystemMetrics(SM_CXSCREEN)-(rc.right-rc.left);
   SetWindowPos(wnd,NULL,x,60,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
   positioned=1;note(host?"host placed left":"client placed right");
  }
 }
 uint32_t now=GetTickCount();if(!started)started=now;
 /* Until menu notifications are fully verified, never race manual UI input. */
 if(!GetPrivateProfileIntA("Lab","BootEnabled",0,controlfile)||finished||(uint32_t)(now-started)>180000)return;
 if(read32(0x8654a4)==6&&active("HUD_SingleRace.fng")){
  finished=1;note("AUTOSTART COMPLETE: live world");return;
 }
 if((int32_t)(now-next_action)<0)return;
 next_action=now+1200;
 if(active("MC_Main.fng")){
  /* 461a18ee resets the profile. Only acknowledge the native completion
   * dialog after its 0x3000000 button phase and the I/O task are finished. */
  uint32_t card=read32(0x836498),flags=read32(0x836514);
  if((flags&0x0f000000)==0x03000000&&readable(card,0x1c)&&!read32(card+0x18))message("MC_Main.fng","PAD_ACCEPT");
 }
 else if(active("UI_StartCareer.fng"))message("UI_StartCareer.fng","PAD_ACCEPT");
 else if(active("UI_CareerCrib.fng"))message("UI_CareerCrib.fng","PAD_ACCEPT");
 else if(active("UI_Main.fng")){
  message("UI_Main.fng",main_left?"PAD_ACCEPT":"PAD_LEFT");main_left=1;
 }
 else if(active("MC_Background.fng"))message("MC_Background.fng","PAD_ACCEPT");
 else if(active("UG_LS_Splash.fng"))message("UG_LS_Splash.fng","PAD_ACCEPT");
}
static void __cdecl pump(void){((void(__cdecl*)(void))0x5cec30)();tick();}
__attribute__((naked)) static void skipmovies(void){
 __asm__ volatile("movl $1,0x8650a8\n\tpushl $0x5805d8\n\tret");
}
static int patch(uintptr_t at,const unsigned char *expected,const unsigned char *value,unsigned n){
 DWORD old;void *p=(void*)at;
 if(memcmp(p,expected,n)||!VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&old))return 0;
 memcpy(p,value,n);FlushInstructionCache(GetCurrentProcess(),p,n);VirtualProtect(p,n,old,&old);return 1;
}
static void apply(void){
 if(patched||(uintptr_t)GetModuleHandleA(NULL)!=0x400000)return;
 note("initializing");
 const unsigned char multi[]={0x74,0x07},allow[]={0xeb,0x07};
 const unsigned char movie[]={0xa3,0xa8,0x50,0x86,0x00};
 /* main: duplicate process-name check followed by exit(0). */
 if(memcmp((void*)0x580d60,"\x6a\x01\x68\xf8\x0d\x7a\x00",7)){note("main mismatch");return;}
 if(memcmp((void*)0x5805d3,movie,5)){note("movie mismatch");return;}
 if(!patch(0x580d71,multi,allow,2)){note("multi mismatch");return;}
 /* Boot option restore writes EAX to SkipMovies. Preserve original flags/registers
  * and replace that store with a jump to our constant setter. */
 unsigned char jump[5]={0xe9};int32_t rel=(int32_t)((uintptr_t)skipmovies-0x5805d8);
 memcpy(jump+1,&rel,4);patch(0x5805d3,movie,jump,5);
 /* The lab runs both render/game threads while neither has keyboard focus.
  * Use the native nonblocking message pump and omit focus-induced pause. */
 const unsigned char wait[]={0x74,0x11},poll[]={0x90,0x90};
 const unsigned char focus[]={0x0f,0x84,0x1b,0x02,0,0},nofocus[]={0xe9,0x1c,0x02,0,0,0x90};
 patch(0x5bf169,wait,poll,2);patch(0x5ccd13,focus,nofocus,6);
 const unsigned char original[]={0xe8,0x6e,0xd7,0x04,0x00};
 rel=(int32_t)((uintptr_t)pump-0x5814c2);memcpy(jump+1,&rel,4);jump[0]=0xe8;
 patch(0x5814bd,original,jump,5);
 *(volatile uint32_t*)0x87098c=1;
 patched=1;
 note("initialized");
}
__declspec(dllexport) void InitializeASI(void){apply();}
__declspec(dllexport) int LabSimulationEnabled(void){return patched;}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID unused){
 (void)unused;if(reason==DLL_PROCESS_ATTACH){
  GetModuleFileNameA(h,ini,sizeof ini);char *slash=strrchr(ini,'\\');if(slash)slash[1]=0;
  strcpy(logfile,ini);strcat(logfile,"LabBoot.log");strcpy(controlfile,ini);strcat(controlfile,"LabControl.ini");strcpy(statefile,ini);strcat(statefile,"LabState.json");strcat(ini,"NFSU2FreeRoam.ini");apply();
 }return TRUE;
}
