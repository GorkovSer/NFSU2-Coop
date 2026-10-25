#include <stdio.h>
#include <stdint.h>
int roam_selftest(uint8_t *,int);
int main(void){static uint8_t b[131072];int n=roam_selftest(b,sizeof b);uint32_t h=2166136261u;for(int i=0;i<n;i++)h=(h^b[i])*16777619u;printf("core golden: %d bytes %08x\n",n,h);return n!=37585||h!=0x8095f1c1u;}
