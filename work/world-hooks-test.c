/* Tests patch verification and x86 thiscall forwarding using synthetic tails.
 * No game logic or external process is executed by this harness. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_world.h"
void nw_test_rebase(uint32_t);
static int block,managers,brains,entered,left,spawned,recycled;
static uint8_t manager[16];
int nw_manager_owned(uint32_t p){if(p!=(uint32_t)(uintptr_t)manager)abort();managers++;return block;}
int nw_brain_owned(uint32_t p){if(p!=0x12345678)abort();brains++;return block;}
void nw_interest_begin(nw_interest_scope *s){memset(s,0,sizeof *s);s->world=(uint32_t)(uintptr_t)manager;s->active=1;entered++;}
void nw_interest_end(const nw_interest_scope *s){if(s->active)left++;}
static void *distance_entry;
static DWORD WINAPI distance_worker(void *unused){
 (void)unused;float p[3]={17.5f,0,0};
 return ((float(__attribute__((thiscall))*)(void*,const float*))distance_entry)(manager,p)==17.5f?0:1;
}
uint8_t nw_spawn(void *m,uint32_t car,const float *d,nw_spawn_fn original){if(m!=manager||car!=0x12345678||d[0]!=1.25f)abort();spawned++;return block?1:original(m,car,d);}
uint8_t nw_recycle(void *m,uint32_t car,nw_eligible_fn original){if(m!=manager||car!=0x12345678)abort();recycled++;return block?1:original(m,car);}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"rb");CHECK(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 uint8_t *raw=malloc(n);CHECK(fread(raw,1,n,f)==(size_t)n);fclose(f);
 uint8_t *image=VirtualAlloc(0,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);CHECK(image);
 IMAGE_DOS_HEADER *dos=(void*)raw;IMAGE_NT_HEADERS32 *nt=(void*)(raw+dos->e_lfanew);
 memcpy(image,raw,nt->OptionalHeader.SizeOfHeaders);IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)memcpy(image+sec[i].VirtualAddress,raw+sec[i].PointerToRawData,sec[i].SizeOfRawData);
 nw_test_rebase((uint32_t)(uintptr_t)image);uint8_t *copy=malloc(0x600000);
 /* One-byte early pool change, guarded by the complete constructor sequence. */
 for(unsigned k=0;k<27;k++){
  image[0x21c73e + k]^=1;memcpy(copy,image,0x600000);CHECK(!nw_pool_prepare());CHECK(!memcmp(copy,image,0x600000));image[0x21c73e + k]^=1;
 }
 *(uint32_t*)(image+0x4a1d00)=1;memcpy(copy,image,0x600000);CHECK(!nw_pool_prepare());CHECK(!memcmp(copy,image,0x600000));*(uint32_t*)(image+0x4a1d00)=0;
 memcpy(copy,image,0x600000);CHECK(nw_pool_prepare());CHECK(nw_pool_prepare());
 for(unsigned i=0;i<0x600000;i++)CHECK(image[i]==copy[i]||(i==0x21c744&&image[i]==96));
 const unsigned offsets[]={0x2f950,0x2df50,0x2f840,0x17be0,0x29dd0,0x1f0fb0,0x1f1160},length[]={6,5,6,7,9,7,7};
 for(int i=0;i<7;i++){
  image[offsets[i]]^=1;memcpy(copy,image,0x600000);CHECK(!nw_hooks_install());CHECK(!memcmp(copy,image,0x600000));image[offsets[i]]^=1;
 }
 memcpy(copy,image,0x600000);DWORD protection;CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READ,&protection));CHECK(nw_hooks_install());CHECK(nw_hooks_install());
 for(int i=0;i<7;i++){MEMORY_BASIC_INFORMATION mi;CHECK(VirtualQuery(image+offsets[i],&mi,sizeof mi));CHECK(mi.Protect==PAGE_EXECUTE_READ);}
 for(unsigned i=0;i<0x600000;i++)if(copy[i]!=image[i]){int inside=0;for(int j=0;j<7;j++)if(i>=offsets[j]&&i<offsets[j]+length[j])inside=1;CHECK(inside);}
 CHECK(VirtualProtect(image,0x600000,PAGE_EXECUTE_READWRITE,&protection));
 /* manager: expose enable byte to the caller, then unwind copied prologue.
  * brain: return 7, then unwind sub esp,8 / push ebx / push esi. */
 const uint8_t manager_tail[]={0x0f,0xb6,0x41,8,0x88,0x41,9,0x8b,0xe5,0x5d,0xc2,4,0};
 const uint8_t brain_tail[]={0xb8,7,0,0,0,0x5e,0x5b,0x83,0xc4,8,0xc2,4,0};
 memcpy(image+offsets[0]+length[0],manager_tail,sizeof manager_tail);
 memcpy(image+offsets[1]+length[1],brain_tail,sizeof brain_tail);
 const uint8_t basic_tail[]={0xb8,7,0,0,0,0x5e,0x83,0xc4,4,0xc2,4,0};
 memcpy(image+offsets[4]+length[4],basic_tail,sizeof basic_tail);
 const uint8_t spawn_tail[]={0xb8,7,0,0,0,0x59,0xc2,8,0};
 const uint8_t recycle_tail[]={0xb8,7,0,0,0,0x5f,0x5e,0x5d,0xc2,4,0};
 memcpy(image+offsets[2]+length[2],spawn_tail,sizeof spawn_tail);memcpy(image+offsets[3]+length[3],recycle_tail,sizeof recycle_tail);
 /* x87 float return + thiscall argument/stack preservation in BOTH native
  * distance helpers, including the saved ESI/EDI in the second prologue. */
 const uint8_t distance_tail[]={0xd9,0x00,0x83,0xc4,0x10,0xc2,4,0};
 const uint8_t dist3[]={0x8b,0x44,0x24,0x1c,0xd9,0x00,0x5f,0x5e,0x83,0xc4,0x10,0xc2,4,0};
 memcpy(image+offsets[5]+length[5],distance_tail,sizeof distance_tail);
 memcpy(image+offsets[6]+length[6],dist3,sizeof dist3);
 float position[3]={123.25f,0,0};
 for(int i=5;i<7;i++)CHECK(((float(__attribute__((thiscall))*)(void*,const float*))(image+offsets[i]))(manager,position)==123.25f);
 CHECK(((float(__attribute__((thiscall))*)(void*,const float*))(image+offsets[5]))((void*)0x12345678,position)==123.25f);
 distance_entry=image+offsets[5];HANDLE worker=CreateThread(0,0,distance_worker,0,0,0);CHECK(worker);
 CHECK(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);DWORD result;CHECK(GetExitCodeThread(worker,&result)&&result==0);CloseHandle(worker);
 for(block=0;block<=1;block++){
  manager[8]=1;manager[9]=0xff;
  ((void(__attribute__((thiscall))*)(void*,float))(image+offsets[0]))(manager,.025f);
  CHECK(manager[8]==1&&manager[9]==(block?0xff:1));
  ((void(__attribute__((thiscall))*)(void*,float))(image+offsets[1]))((void*)0x12345678,.025f);
  ((void(__attribute__((thiscall))*)(void*,float))(image+offsets[4]))((void*)0x12345678,.025f);
  float density[3]={1.25f,2,3};CHECK(((nw_spawn_fn)(image+offsets[2]))(manager,0x12345678,density)==(block?1:7));
  CHECK(((nw_eligible_fn)(image+offsets[3]))(manager,0x12345678)==(block?1:7));
 }
 CHECK(managers==2&&brains==4&&entered==6&&left==6&&spawned==2&&recycled==2);
 puts("PASS traffic hooks: 27 constructor mismatches/live-pool guard; seven hook guards; RX restoration; both native controller updates forward/block; owned manager fully skipped; 2D/3D x87 distance returns; balanced scopes and spawn/recycle interception");
 return 0;
}
