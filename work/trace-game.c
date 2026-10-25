#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 STARTUPINFOA si={sizeof(si)};PROCESS_INFORMATION pi={0};
 si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
 if(!CreateProcessA(argv[1],NULL,NULL,NULL,FALSE,DEBUG_ONLY_THIS_PROCESS,NULL,argv[2],&si,&pi)){printf("CreateProcess failed %lu\n",GetLastError());return 3;}
 printf("child=%lu\n",pi.dwProcessId);fflush(stdout);
 DebugSetProcessKillOnExit(FALSE);
 DWORD until=GetTickCount()+600000;int done=0;int initial_break=1;
 while((int)(until-GetTickCount())>0&&!done){
  DEBUG_EVENT ev={0};if(!WaitForDebugEvent(&ev,500))continue;
  DWORD status=DBG_CONTINUE;
  if(ev.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){if(ev.u.CreateProcessInfo.hFile)CloseHandle(ev.u.CreateProcessInfo.hFile);}
  if(ev.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){if(ev.u.LoadDll.hFile)CloseHandle(ev.u.LoadDll.hFile);}
  if(ev.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
   DWORD code=ev.u.Exception.ExceptionRecord.ExceptionCode;
   int loader_break=code==EXCEPTION_BREAKPOINT&&initial_break;
   if(loader_break)initial_break=0;
   if(!loader_break&&code!=0x406d1388){
    printf("exception code=%08lx address=%p first=%lu",code,ev.u.Exception.ExceptionRecord.ExceptionAddress,ev.u.Exception.dwFirstChance);
    HANDLE th=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,ev.dwThreadId);CONTEXT ctx={0};ctx.ContextFlags=CONTEXT_FULL;
    if(th&&GetThreadContext(th,&ctx)){
     uint32_t stack[128]={0};SIZE_T got=0;ReadProcessMemory(pi.hProcess,(void*)(uintptr_t)ctx.Esp,stack,sizeof(stack),&got);
     printf(" eax=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx ebp=%08lx esp=%08lx stack=",ctx.Eax,ctx.Ecx,ctx.Edx,ctx.Esi,ctx.Edi,ctx.Ebp,ctx.Esp);
     for(unsigned i=0;i<got/4;i++)printf("%08x,",stack[i]);
     if(code==EXCEPTION_BREAKPOINT||code==EXCEPTION_ACCESS_VIOLATION){
      uint32_t addresses[]={ctx.Edx,ctx.Esi,0x828164,0x890080,0x890118,0x863470};
      for(unsigned j=0;j<sizeof(addresses)/sizeof(addresses[0]);j++){
       uint32_t data[32]={0};SIZE_T n=0;
       if(ReadProcessMemory(pi.hProcess,(void*)(uintptr_t)addresses[j],data,sizeof(data),&n)){
        printf("\n memory[%08x]=",addresses[j]);for(unsigned z=0;z<n/4;z++)printf("%08x,",data[z]);
       }
      }
     }
    }
    if(th)CloseHandle(th);puts("");fflush(stdout);
   }
   status=loader_break?DBG_CONTINUE:DBG_EXCEPTION_NOT_HANDLED;
  }
  if(ev.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){printf("exit=%08lx\n",ev.u.ExitProcess.dwExitCode);done=1;}
  ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,status);
 }
 if(!done){DebugSetProcessKillOnExit(FALSE);DebugActiveProcessStop(pi.dwProcessId);printf("still running=%lu\n",pi.dwProcessId);}
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 0;
}
