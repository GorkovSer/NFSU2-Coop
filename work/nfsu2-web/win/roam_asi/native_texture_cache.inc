/* Exact EXE: CarRenderInfo owns three 73-entry texture-remap tables at
 * +638/+ac8/+f58 (638190). Entry {source hash,replacement hash,cached TI,
 * active swap address}. 490560 resolves cached TI only when it is -1;
 * 488dd0 restores the geometry table after rendering. CarLoader may replace
 * a DUMMY_SKIN TextureInfo while another instance of that model survives.
 * Refresh the native lookup cache between frames, never during a swap.
 * No TextureInfo/platform objects are copied or fabricated. */
static unsigned ntc_refresh_car(uint32_t car){
 if(!ok(car,0x9e0))return 0;
 uint32_t render=rd32(car+0x554);
 if(!ok(render,0x13e8)||rd32(render)!=car||rd32(render+4)!=rd32(car+0x10)||
    rd32(render+8)!=rd32(car+0xc))return 0;
 unsigned changed=0;
 for(unsigned table=0;table<3;table++){
  uint32_t entries=render+0x638+table*0x490;int swapping=0;
  for(unsigned i=0;i<73;i++)if(rd32(entries+16*i+12)){swapping=1;break;}
  if(swapping)continue;
  for(unsigned i=0;i<73;i++){
   uint32_t entry=entries+16*i;
   if(rd32(entry)&&rd32(entry+4)&&rd32(entry+8)!=0xffffffffu){
    MR32(entry+8)=0xffffffffu;changed++;
   }
  }
 }
 return changed;
}
static void ntc_frame(void){
 if(rd32(G_FLOW)!=6||(!NC.enabled&&!NW.enabled))return;
 uint32_t w=world();if(!ok(w,0x80))return;
 unsigned count=rd32(w+0x10);if(count>22)return;
 for(unsigned i=0;i<count;i++)ntc_refresh_car(rd32(w+0x1c+4*i));
}
