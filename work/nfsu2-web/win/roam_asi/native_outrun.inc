/* Windows outrun adapter: preserve brains, scope native goals and intercept
 * the native Engage eligibility query before it queues a solo Outrun. */
int nr_outruns_enabled(void){
 static int checked,enabled;
 if(!checked){checked=1;enabled=env_int("NFSU2_ROAM_OUTRUNS",0,0,1)&&nc_hooks_install();
  if(enabled)rlog("[outrun] experimental player challenges enabled; native Engage action, 30m proximity\n");}
 return enabled;
}
int nr_free_car(uint32_t car){
 if(!nr_outruns_enabled()||!car||rd32(G_FLOW)!=6)return 0;
 for(int k=0;k<NSLOT;k++)if(g_sl[k].car==car&&g_sl[k].world==world()&&
    (g_sl[k].st==SL_ACTIVE||g_sl[k].st==SL_LOADING))return 1;
 return 0;
}
int nr_free_brain(uint32_t brain){
 if(!brain)return 0;
 for(int k=0;k<NSLOT;k++)if(nr_free_car(g_sl[k].car)&&rd32(g_sl[k].car+0x2c)==brain)return 1;
 return 0;
}
void nr_goals(void(*original)(void)){
 uint32_t w=world();
 if(!nr_outruns_enabled()||!ok(w,0x80)){original();return;}
 uint32_t first=MR32(w+0x14),end=MR32(w+0x18),saved[22],count=first;
 if(first>end||end>22){original();return;}
 for(uint32_t i=first;i<end;i++){
  uint32_t car=saved[i-first]=MR32(w+0x1c+4*i);
  if(!nr_free_car(car))MR32(w+0x1c+4*count++)=car;
 }
 MR32(w+0x18)=count;original();
 for(uint32_t i=first;i<end;i++)MR32(w+0x1c+4*i)=saved[i-first];
 MR32(w+0x18)=end;
}
