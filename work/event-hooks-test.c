#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void nes_test_rebase(uint32_t);int nes_hooks_install(void);
static unsigned checks,fallbacks;static void *entry;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t nes_marker_event(uint32_t career,uint32_t marker){if(career!=0x1234||marker!=0x5678)abort();fallbacks++;return 0x1111;}
/* Link stubs for the added hooks; this existing fixture covers lookup only. */
int nes_event_unlocked(uint32_t list,uint32_t event){(void)list;(void)event;return 0;}
void nes_map_rebuilt(uint32_t career){(void)career;}
static DWORD WINAPI worker(void *unused){(void)unused;return ((uint32_t(__attribute__((thiscall))*)(void*,uint32_t))entry)((void*)0x1234,0x5678);}
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"rb");CHECK(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);uint8_t *raw=malloc(n);CHECK(fread(raw,1,n,f)==(size_t)n);fclose(f);
 uint8_t *image=VirtualAlloc(0,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);CHECK(image);
 IMAGE_DOS_HEADER *dos=(void*)raw;IMAGE_NT_HEADERS32 *nt=(void*)(raw+dos->e_lfanew);memcpy(image,raw,nt->OptionalHeader.SizeOfHeaders);
 IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)memcpy(image+sec[i].VirtualAddress,raw+sec[i].PointerToRawData,sec[i].SizeOfRawData);
 uint8_t *at=image+0x127710;nes_test_rebase((uint32_t)(uintptr_t)image);
 uint8_t saved[5];memcpy(saved,at,5);
 for(unsigned i=0;i<5;i++){at[i]^=1;CHECK(!nes_hooks_install());CHECK(at[i]==(uint8_t)(saved[i]^1));at[i]^=1;}
 uint8_t tail[]={0xb8,0,0,0,0,0x59,0xc2,4,0};memcpy(at+5,tail,sizeof tail);
 DWORD protection;CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READ,&protection));CHECK(nes_hooks_install());CHECK(nes_hooks_install());
 MEMORY_BASIC_INFORMATION mi;CHECK(VirtualQuery(at,&mi,sizeof mi)&&mi.Protect==PAGE_EXECUTE_READ);
 entry=at;CHECK(((uint32_t(__attribute__((thiscall))*)(void*,uint32_t))entry)((void*)0x1234,0x5678)==0x1111&&fallbacks==1);
 HANDLE t=CreateThread(0,0,worker,0,0,0);CHECK(t&&WaitForSingleObject(t,5000)==WAIT_OBJECT_0);DWORD answer;CHECK(GetExitCodeThread(t,&answer)&&answer==0&&fallbacks==1);CloseHandle(t);
 CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READWRITE,&protection));uint32_t native=0x2222;memcpy(at+6,&native,4);
 CHECK(((uint32_t(__attribute__((thiscall))*)(void*,uint32_t))entry)((void*)0x1234,0x5678)==native&&fallbacks==1);
 printf("PASS shared marker hook: %u checks; exact prologue mismatch refusal, RX restoration, x86 thiscall/original priority, game-thread-only fallback; synthetic tails only\n",checks);
}
