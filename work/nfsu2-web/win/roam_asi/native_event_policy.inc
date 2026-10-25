/* Native type zero includes both ordinary Circuit and URL series. */
static int nc_event_mode(uint32_t event){
 unsigned type=MR8(event+0xf);
 if(type==4)return RM_DRAG;if(type==5)return RM_DRIFT;
 return type==0&&MR8(event+0x34)==2?RM_URL:0;
}
static int nc_event_supported(uint32_t event){
 unsigned type=MR8(event+0xf);
 if(type!=0&&type!=1&&type!=2&&type!=4&&type!=5)return 0;
 uint32_t pool=rd32(0x838428);if(!pool)return 0;
 char name[96];cstr(pool+MR16(event),name,sizeof name);if(!name[0])return 0;
 int url=0;for(unsigned i=0;name[i];i++)if(name[i]=='_'&&!strncmp(name+i,"_URL_",5)){url=1;break;}
 unsigned mask=(unsigned)env_int("NFSU2_ROAM_COOP_RACE_MODES",1,1,63);
 if(url||nc_event_mode(event)==RM_URL)
  return url&&nc_event_mode(event)==RM_URL&&(mask&8)&&MR8(event+0x7e)>=1&&MR8(event+0x7e)<=RM_ROUNDS;
 return !!(mask&(1u<<type));
}
