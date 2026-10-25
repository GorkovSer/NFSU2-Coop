#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "race_outcome.h"
#define MR8(a) (*(uint8_t*)(uintptr_t)(a))
#define MR16(a) (*(uint16_t*)(uintptr_t)(a))
static unsigned mode_mask=1;
static char name[96];
static uint32_t rd32(uint32_t a){return a==0x838428?(uint32_t)(uintptr_t)name:0;}
static int env_int(const char *a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;return (int)mode_mask;}
static void cstr(uint32_t p,char *o,unsigned n){strncpy(o,(void*)(uintptr_t)p,n);o[n-1]=0;}
#include "native_event_policy.inc"
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);uint8_t event[0x88]={0};unsigned type,count=0,allowed[4]={0},url=0;
 while(fscanf(f,"%u %95s",&type,name)==2){
  event[15]=(uint8_t)type;event[0x34]=strstr(name,"_URL_")?2:0;event[0x7e]=3;count++;
  for(int variant=0;variant<4;variant++){mode_mask=variant==3?63:variant==2?55:variant?7:1;int accept=nc_event_supported((uint32_t)(uintptr_t)event);allowed[variant]+=accept;
   if(strstr(name,"_URL_")){CHECK(type==0);CHECK(accept==(variant==3));}if(type==3||((type==4||type==5)&&variant<2))CHECK(!accept);
  }
  if(strstr(name,"_URL_"))url++;
 }
 fclose(f);CHECK(count==224&&url==24);CHECK(allowed[0]==56&&allowed[1]==128);CHECK(allowed[2]==198&&allowed[3]==222);
 strcpy(name,"TEST_DRIFT");event[15]=5;*(uint16_t*)(event+0x18)=0x104e;mode_mask=63;event[0x34]=0;CHECK(nc_event_supported((uint32_t)(uintptr_t)event));
 name[0]=0;event[15]=0;CHECK(!nc_event_supported((uint32_t)(uintptr_t)event));
 printf("PASS actual event catalog: %u events; %u URL series excluded from Circuit; %u Circuit and %u rank-based opt-in events; DDAY rejected; URL/Drag/all Drift opt-in\n",count,url,allowed[0],allowed[1]);return 0;
}
