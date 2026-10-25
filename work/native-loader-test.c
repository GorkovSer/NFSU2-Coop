/* Loads the actual ASI against a mapped, non-executing game image.
 * Exercises initialization and hook placement; it does not simulate gameplay. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static HMODULE WINAPI game_module(LPCSTR name){ return name ? GetModuleHandleA(name) : (HMODULE)0x400000; }
typedef BOOL (WINAPI *dllentry)(HINSTANCE,DWORD,LPVOID);
int main(int argc,char **argv){
 if(argc!=4)return 2;
 if(!strcmp(argv[3],"wrong-host")){
  uint8_t *host=(void*)GetModuleHandleA(NULL);IMAGE_DOS_HEADER *dos=(void*)host;
  IMAGE_NT_HEADERS32 *nt=(void*)(host+dos->e_lfanew);IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);
  unsigned i=0;for(;i<nt->FileHeader.NumberOfSections;i++)if(!memcmp(sec[i].Name,".text",5))break;
  if(i==nt->FileHeader.NumberOfSections)return 10;
  uint32_t size=sec[i].Misc.VirtualSize;uint8_t *text=host+sec[i].VirtualAddress,*copy=malloc(size);memcpy(copy,text,size);
  SetEnvironmentVariableA("NFSU2_ROAM","1");SetEnvironmentVariableA("NFSU2_ROAM_LOG","1");
  HMODULE dll=LoadLibraryA(argv[2]);if(!dll){printf("DLL load failed %lu\n",GetLastError());return 11;}
  int pass=!memcmp(text,copy,size);FreeLibrary(dll);free(copy);
  printf("actual ASI wrong-host load/unload: %s; harness .text unchanged, no game frame invoked\n",pass?"PASS":"FAIL");return pass?0:1;
 }
 uint8_t *img=VirtualAlloc((void*)0x400000,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);if(img!=(void*)0x400000){
  printf("reserve failed %lu; harness module=%p\n",GetLastError(),GetModuleHandleA(NULL));
  for(uintptr_t at=0x400000;at<0xa00000;){MEMORY_BASIC_INFORMATION mi;if(!VirtualQuery((void*)at,&mi,sizeof mi))break;
   printf("region %p size=%lx state=%lx allocation=%p\n",mi.BaseAddress,(unsigned long)mi.RegionSize,mi.State,mi.AllocationBase);
   at=(uintptr_t)mi.BaseAddress+mi.RegionSize;
  }return 5;}
 FILE *f=fopen(argv[1],"rb");if(!f)return 3;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 uint8_t *raw=malloc(n);if(fread(raw,1,n,f)!=(size_t)n)return 4;fclose(f);
 IMAGE_DOS_HEADER *dos=(void*)raw;IMAGE_NT_HEADERS32 *nt=(void*)(raw+dos->e_lfanew);
 uint32_t size=nt->OptionalHeader.SizeOfImage;
 if(size>0x600000)return 5;
 memcpy(img,raw,nt->OptionalHeader.SizeOfHeaders);
 IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)memcpy(img+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
 int negative=strcmp(argv[3],"good")!=0;
 if(!strcmp(argv[3],"bad-entry")){IMAGE_NT_HEADERS32 *e=(void*)(img+dos->e_lfanew);e->OptionalHeader.AddressOfEntryPoint^=1;}
 if(!strcmp(argv[3],"bad-code"))img[0x1813da]^=1;
 uint8_t *before=malloc(size);memcpy(before,img,size);
 HMODULE dll=LoadLibraryExA(argv[2],NULL,DONT_RESOLVE_DLL_REFERENCES);
 if(!dll){printf("load failed %lu\n",GetLastError());return 6;}
 IMAGE_DOS_HEADER *dd=(void*)dll;IMAGE_NT_HEADERS32 *dn=(void*)((uint8_t*)dll+dd->e_lfanew);
 IMAGE_IMPORT_DESCRIPTOR *imp=(void*)((uint8_t*)dll+dn->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
 for(;imp->Name;imp++){
   HMODULE dep=LoadLibraryA((char*)dll+imp->Name);if(!dep)return 7;
   IMAGE_THUNK_DATA32 *names=(void*)((uint8_t*)dll+imp->OriginalFirstThunk),*iat=(void*)((uint8_t*)dll+imp->FirstThunk);
   for(;names->u1.AddressOfData;names++,iat++){
     FARPROC proc;
     if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))proc=GetProcAddress(dep,(LPCSTR)(uintptr_t)IMAGE_ORDINAL32(names->u1.Ordinal));
     else {IMAGE_IMPORT_BY_NAME *name=(void*)((uint8_t*)dll+names->u1.AddressOfData);proc=!strcmp((char*)name->Name,"GetModuleHandleA")?(FARPROC)game_module:GetProcAddress(dep,(char*)name->Name);}
     if(!proc)return 8;DWORD old;VirtualProtect(iat,sizeof(*iat),PAGE_READWRITE,&old);iat->u1.Function=(DWORD)(uintptr_t)proc;VirtualProtect(iat,sizeof(*iat),old,&old);
   }
 }
 SetEnvironmentVariableA("NFSU2_ROAM_LOG","1");
 SetEnvironmentVariableA("NFSU2_ROAM","1");
 dllentry entry=(dllentry)((uint8_t*)dll+dn->OptionalHeader.AddressOfEntryPoint);
 if(!entry((HINSTANCE)dll,DLL_PROCESS_ATTACH,NULL))return 9;
 unsigned changed=0,outside=0;
 for(uint32_t i=0;i<size;i++)if(img[i]!=before[i]){changed++;if(i<0x1813d5||i>=0x1813da)outside++;}
 int32_t rel;memcpy(&rel,img+0x1813d6,4);uintptr_t target=0x5813da+(uint32_t)rel;
 int pass=negative?changed==0:(changed>0&&!outside&&img[0x1813d5]==0xe8&&target>=(uintptr_t)dll&&target<(uintptr_t)dll+dn->OptionalHeader.SizeOfImage);
 printf("loader %s: %s; changed=%u outside-hook=%u target=%08lx\n",argv[3],pass?"PASS":"FAIL",changed,outside,(unsigned long)target);
 entry((HINSTANCE)dll,DLL_PROCESS_DETACH,NULL);
 return pass?0:1;
}
