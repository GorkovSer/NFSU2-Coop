/* Best-effort diagnostics, never exception recovery. No game reads, CRT,
 * session/log locks or network calls. DbgHelp is preloaded outside DllMain.
 * Tests use their own exception contexts and a workspace output directory. */
#include "native_crash.h"
#include <dbghelp.h>
#include <tlhelp32.h>
#include <stdint.h>
typedef BOOL (WINAPI *dump_fn)(HANDLE,DWORD,HANDLE,MINIDUMP_TYPE,
 const MINIDUMP_EXCEPTION_INFORMATION *,const MINIDUMP_USER_STREAM_INFORMATION *,const MINIDUMP_CALLBACK_INFORMATION *);
static LPTOP_LEVEL_EXCEPTION_FILTER previous;
static HMODULE self,debug_library;static dump_fn write_dump;
static volatile LONG installed,preparing,writing;
static volatile LONG report_serial;
static wchar_t directory[1024];static int directory_ready;
static struct {uintptr_t base;DWORD size;char name[3*MAX_PATH];} modules[128];
static unsigned module_count;
/* Captured on the game thread, never game-object reads in the exception
 * handler. Keep bounded history of resource/controller lifetime calls. */
typedef struct {DWORD fn,object,frame,after,world,flow,pool,head,next,used,capacity;} call_sample;
static call_sample calls[64],first_bad_pool;static unsigned call_serial;static int have_bad_pool;
static DWORD sample_word(DWORD address){DWORD value=0;SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)address,&value,4,&got)&&got==4?value:0;}
void ncr_native_call(DWORD fn,DWORD object,DWORD frame,int after){
 switch(fn){case 0x601110:case 0x63bc70:case 0x63bcd0:case 0x61ca50:case 0x63ae00:case 0x63a1b0:case 0x5f6e20:case 0x5f6d50:case 0x5f46a0:case 0x494180:break;default:return;}
 call_sample s={0};s.fn=fn;s.object=object;s.frame=frame;s.after=after;
 s.world=sample_word(0x890080);s.flow=sample_word(0x8654a4);s.pool=sample_word(0x870954);
 if(s.pool&&s.pool!=0xeeeeeeeeu){s.head=sample_word(s.pool+0x10);s.used=sample_word(s.pool+0x18);s.capacity=sample_word(s.pool+0x2c);if(s.head)s.next=sample_word(s.head);}
 calls[call_serial++%64]=s;
 if(!have_bad_pool&&(s.pool==0xeeeeeeeeu||s.head==0xeeeeeeeeu||s.next==0xeeeeeeeeu)){first_bad_pool=s;have_bad_pool=1;}
}
#ifndef ROAM_ASI_VERSION
#define ROAM_ASI_VERSION "unversioned-native-crash-handler"
#endif
static const char *version=ROAM_ASI_VERSION;
static void copyw(wchar_t *d,const wchar_t *s,unsigned cap){unsigned i=0;while(i+1<cap&&s[i]){d[i]=s[i];i++;}d[i]=0;}
static int appendw(wchar_t *d,const wchar_t *s,unsigned cap){unsigned i=0,j=0;while(i<cap&&d[i])i++;while(i+j+1<cap&&s[j]){d[i+j]=s[j];j++;}if(i+j>=cap)return 0;d[i+j]=0;return !s[j];}
static void text(HANDLE f,const char *s){DWORD n=0,written;while(s[n])n++;if(n)WriteFile(f,s,n,&written,0);}
static void hex(HANDLE f,uintptr_t value){char b[11]="0x00000000";const char *digits="0123456789abcdef";for(int k=0;k<8;k++)b[9-k]=digits[(value>>(4*k))&15];text(f,b);}
static void field(HANDLE f,const char *name,uintptr_t value){text(f,name);text(f,"=");hex(f,value);text(f,"\r\n");}
static void numberw(wchar_t *out,unsigned value,unsigned digits){for(unsigned k=0;k<digits;k++){out[digits-1-k]=(wchar_t)(L'0'+value%10);value/=10;}}
static int make_path(wchar_t *out,const wchar_t *ext){
 SYSTEMTIME t;GetLocalTime(&t);copyw(out,directory,1200);
 wchar_t name[64]=L"\\NFSU2Crash-00000000-000000-000-0000000000-0000";
 numberw(name+12,t.wYear,4);numberw(name+16,t.wMonth,2);numberw(name+18,t.wDay,2);
 numberw(name+21,t.wHour,2);numberw(name+23,t.wMinute,2);numberw(name+25,t.wSecond,2);
 numberw(name+28,t.wMilliseconds,3);numberw(name+32,GetCurrentProcessId(),10);
 numberw(name+43,(unsigned)InterlockedIncrement(&report_serial),4);
 return appendw(out,name,1200)&&appendw(out,ext,1200);
}
void ncr_prepare(void){
 if(InterlockedCompareExchange(&preparing,1,0))return;
 DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",directory,960);
 if(!n||n>=960){n=GetTempPathW(960,directory);if(!n||n>=960)return;}
 if(!appendw(directory,L"\\NFSU2-Coop",1024))return;
 if(!CreateDirectoryW(directory,0)&&GetLastError()!=ERROR_ALREADY_EXISTS)return;
 if(!appendw(directory,L"\\Crashes",1024))return;
 if(!CreateDirectoryW(directory,0)&&GetLastError()!=ERROR_ALREADY_EXISTS)return;
 directory_ready=1;
 /* Absolute system path avoids loading a DLL from the game directory. */
 wchar_t library[MAX_PATH];n=GetSystemDirectoryW(library,MAX_PATH);
 if(n&&n<MAX_PATH&&appendw(library,L"\\dbghelp.dll",MAX_PATH)){
  debug_library=LoadLibraryW(library);
  if(debug_library)write_dump=(dump_fn)(uintptr_t)GetProcAddress(debug_library,"MiniDumpWriteDump");
 }
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
 if(snap!=INVALID_HANDLE_VALUE){
  MODULEENTRY32W m;m.dwSize=sizeof m;
  if(Module32FirstW(snap,&m))do{
   if(module_count==128)break;unsigned k=module_count++;
   modules[k].base=(uintptr_t)m.modBaseAddr;modules[k].size=m.modBaseSize;
   WideCharToMultiByte(CP_UTF8,0,m.szExePath,-1,modules[k].name,sizeof modules[k].name,0,0);
  }while(Module32NextW(snap,&m));
  CloseHandle(snap);
 }
}
int ncr_report(EXCEPTION_POINTERS *ep,const char *reason){
 if(!directory_ready||!ep||!ep->ExceptionRecord||!ep->ContextRecord||InterlockedCompareExchange(&writing,1,0))return 0;
 wchar_t log_path[1200],dump_path[1200];int result=0;
 if(!make_path(log_path,L".txt"))goto done;
 HANDLE f=CreateFileW(log_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE)goto done;
 text(f,"NFSU2 cooperative crash report\r\nversion=");text(f,version);text(f,"\r\nreason=");text(f,reason?reason:"unknown");text(f,"\r\n");
 field(f,"process",GetCurrentProcessId());field(f,"thread",GetCurrentThreadId());field(f,"plugin_base",(uintptr_t)self);
 EXCEPTION_RECORD *e=ep->ExceptionRecord;CONTEXT *c=ep->ContextRecord;
 field(f,"exception",e->ExceptionCode);field(f,"address",(uintptr_t)e->ExceptionAddress);field(f,"flags",e->ExceptionFlags);
 if((e->ExceptionCode==EXCEPTION_ACCESS_VIOLATION||e->ExceptionCode==EXCEPTION_IN_PAGE_ERROR)&&e->NumberParameters>=2){field(f,"access_0_read_1_write_8_execute",e->ExceptionInformation[0]);field(f,"access_address",e->ExceptionInformation[1]);}
 field(f,"EIP",c->Eip);field(f,"ESP",c->Esp);field(f,"EBP",c->Ebp);field(f,"EFLAGS",c->EFlags);
 field(f,"EAX",c->Eax);field(f,"EBX",c->Ebx);field(f,"ECX",c->Ecx);field(f,"EDX",c->Edx);field(f,"ESI",c->Esi);field(f,"EDI",c->Edi);
 text(f,"\r\nCached game-thread lifetime calls (frame fn object after world flow pool head next used capacity):\r\n");
 unsigned count=call_serial<64?call_serial:64;
 for(unsigned k=0;k<count+(unsigned)have_bad_pool;k++){
  const call_sample *s=k<count?&calls[(call_serial-count+k)%64]:&first_bad_pool;
  if(k==count)text(f,"FIRST_OBSERVED_BAD_POOL ");
  const DWORD values[]={s->frame,s->fn,s->object,s->after,s->world,s->flow,s->pool,s->head,s->next,s->used,s->capacity};
  for(unsigned j=0;j<11;j++){hex(f,values[j]);text(f,j==10?"\r\n":" ");}
 }
 text(f,"\r\nModules cached before fault (base, size, path):\r\n");
 for(unsigned k=0;k<module_count;k++){hex(f,modules[k].base);text(f," ");hex(f,modules[k].size);text(f," ");text(f,modules[k].name);text(f,"\r\n");}
 text(f,"\r\nStack words (not an unwound call stack):\r\n");
 for(unsigned k=0;k<64;k++){
  uintptr_t address=(uintptr_t)c->Esp+4*k;DWORD word;SIZE_T read=0;
  if(address<(uintptr_t)c->Esp||!ReadProcessMemory(GetCurrentProcess(),(void*)address,&word,sizeof word,&read)||read!=sizeof word)break;
  hex(f,address);text(f," ");hex(f,word);text(f,"\r\n");
 }
 DWORD dump_error=ERROR_PROC_NOT_FOUND;int dump_ok=0;
 /* Derive the dump name from the exact report basename, not a second clock. */
 copyw(dump_path,log_path,1200);unsigned end=0;while(dump_path[end])end++;
 if(end>=4){dump_path[end-3]=L'd';dump_path[end-2]=L'm';dump_path[end-1]=L'p';}
 if(write_dump){
  HANDLE d=CreateFileW(dump_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
  if(d!=INVALID_HANDLE_VALUE){
   MINIDUMP_EXCEPTION_INFORMATION info={GetCurrentThreadId(),ep,FALSE};
   MINIDUMP_TYPE kind=(MINIDUMP_TYPE)(MiniDumpWithDataSegs|MiniDumpWithIndirectlyReferencedMemory);
   dump_ok=write_dump(GetCurrentProcess(),GetCurrentProcessId(),d,kind,&info,0,0);dump_error=dump_ok?0:GetLastError();CloseHandle(d);
  }else dump_error=GetLastError();
 }
 field(f,"minidump_written",dump_ok);field(f,"minidump_error",dump_error);
 text(f,"Diagnostics only. Unhandled exceptions continue to the previous handler/Windows.\r\n");
 FlushFileBuffers(f);CloseHandle(f);result=1;
done:InterlockedExchange(&writing,0);return result;
}
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep){
 ncr_report(ep,"unhandled exception");
 return previous&&previous!=crash_filter?previous(ep):EXCEPTION_CONTINUE_SEARCH;
}
void ncr_install(HMODULE h){
 if(InterlockedCompareExchange(&installed,1,0))return;self=h;previous=SetUnhandledExceptionFilter(crash_filter);
}
void ncr_remove(void){
 if(!InterlockedExchange(&installed,0))return;
 LPTOP_LEVEL_EXCEPTION_FILTER current=SetUnhandledExceptionFilter(previous);
 if(current!=crash_filter)SetUnhandledExceptionFilter(current);
 /* No FreeLibrary under loader lock; the OS frees it on process exit. */
}
