/* Uses synthetic exception records. Never starts or crashes the game. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "native_crash.h"
static int called;
static LONG WINAPI chained(EXCEPTION_POINTERS *p){if(p&&p->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION)called++;return EXCEPTION_CONTINUE_SEARCH;}
int main(int argc,char **argv){
 if(argc!=2||!SetEnvironmentVariableA("LOCALAPPDATA",argv[1]))return 1;
 CONTEXT c={0};EXCEPTION_RECORD e={0};EXCEPTION_POINTERS p={&e,&c};
 uint32_t stack[64];for(int k=0;k<64;k++)stack[k]=0x12340000u+k;
 c.ContextFlags=CONTEXT_FULL;c.Eip=(DWORD)(uintptr_t)main;c.Esp=(DWORD)(uintptr_t)stack;c.Eax=0x10203040;c.EFlags=0x202;
 e.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;e.ExceptionAddress=(void*)(uintptr_t)c.Eip;e.NumberParameters=2;e.ExceptionInformation[0]=1;e.ExceptionInformation[1]=0xdeadbeef;
 if(ncr_report(&p,"before prepare"))return 2;
 SetUnhandledExceptionFilter(chained);ncr_install(GetModuleHandleA(0));ncr_prepare();ncr_prepare();
 /* Synthetic game globals in this test process only: retain the first bad
  * pool observation even after the bounded call-history ring wraps. */
 void *globals=VirtualAlloc((void*)0x870000,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(globals!=(void*)0x870000)return 7;
 uint32_t pool[12]={0};pool[4]=0xeeeeeeee;pool[6]=42;pool[11]=77;
 *(uint32_t*)0x870954=(uint32_t)(uintptr_t)pool;
 ncr_native_call(0x601110,0x11110000,100,0);pool[4]=0;
 for(int i=0;i<70;i++)ncr_native_call(0x601110,0x12340000+i,i,i&1);
 if(!ncr_report(&p,"synthetic guarded fault"))return 3;
 LPTOP_LEVEL_EXCEPTION_FILTER f=SetUnhandledExceptionFilter(0);SetUnhandledExceptionFilter(f);
 if(!f||f==chained||f(&p)!=EXCEPTION_CONTINUE_SEARCH||called!=1)return 4;
 if(!ncr_report(&p,"synthetic second report"))return 5;
 ncr_remove();f=SetUnhandledExceptionFilter(0);if(f!=chained)return 6;
 VirtualFree(globals,0,MEM_RELEASE);
 puts("PASS crash reporter: synthetic contexts, text and minidump writes, repeated reports, prepared once, previous exception filter chained/restored; no game or real exception");return 0;
}
