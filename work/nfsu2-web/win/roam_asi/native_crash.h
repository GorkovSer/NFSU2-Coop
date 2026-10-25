#ifndef NATIVE_CRASH_H
#define NATIVE_CRASH_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void ncr_install(HMODULE);
void ncr_prepare(void);
void ncr_remove(void);
int ncr_report(EXCEPTION_POINTERS *,const char *reason);
void ncr_native_call(DWORD fn,DWORD object,DWORD frame,int after);
#endif
