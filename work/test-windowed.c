/* Test-copy-only window flag, found with ThirteenAG's documented signature. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
static void apply(void){
 const uint8_t *p=(const uint8_t*)(uintptr_t)0x005b9a0f;
 if((uintptr_t)GetModuleHandleA(NULL)!=0x400000)return;
 if(p[0]!=0xa1 || *(const uint32_t*)(p+1)!=0x0087098c || p[5]!=0x33 || p[6]!=0xff)return;
 *(volatile uint32_t*)(uintptr_t)0x0087098c=1;
}
__declspec(dllexport) void InitializeASI(void){apply();}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID unused){
 (void)h;(void)unused;if(reason==DLL_PROCESS_ATTACH)apply();return TRUE;
}
