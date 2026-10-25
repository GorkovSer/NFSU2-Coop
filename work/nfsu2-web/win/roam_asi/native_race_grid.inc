/* Game thread only. Capture the replaced AI's native grid BEFORE applying
 * network poses. Move the guest's own physical car, never its controller or
 * equipment. Both peers otherwise put their human in the same vanilla slot. */
static struct {
 uint32_t run,world,race,car,record;
 int captured,placed;
 float origin[3],matrix[16];
} NG;
static int ng_basis(const float *m){
 for(int i=0;i<3;i++){
  float length=0;
  for(int j=0;j<3;j++){float v=m[4*i+j];if(!__builtin_isfinite(v))return 0;length+=v*v;}
  if(length<.99f||length>1.01f)return 0;
  for(int k=0;k<i;k++){float dot=0;for(int j=0;j<3;j++)dot+=m[4*i+j]*m[4*k+j];if(__builtin_fabsf(dot)>.01f)return 0;}
 }
 return 1;
}
static void nc_grid_capture(void){
 memset(&NG,0,sizeof NG);
 if(NC.host)return;
 uint32_t bot=NC.car[0],own=NC.car[1];
 if(!ok(bot,0x9e0)||!ok(own,0x9e0)||bot==own||own!=player_car())return;
 float distance=0;
 for(int j=0;j<3;j++){
  float p=MRF32(bot+0x50+4*j),d=p-MRF32(own+0x50+4*j);
  if(!__builtin_isfinite(p)||!__builtin_isfinite(d))return;
  NG.origin[j]=p;distance+=d*d;
  for(int k=0;k<3;k++)NG.matrix[4*k+j]=MRF32(bot+0x90+16*k+4*j);
 }
 if(distance<1.f||distance>10000.f||!ng_basis(NG.matrix))return;
 NG.matrix[15]=1;NG.run=NC.control.run;NG.world=NC.world;NG.race=NC.race;
 NG.car=own;NG.record=NC.record[1];NG.captured=1;
}
static int nc_grid_frame(CPU *c){
 if(NC.host)return 1;
 if(!nc_same_world()||!NG.captured||NG.run!=NC.control.run||NG.world!=NC.world||
    NG.race!=NC.race||NG.car!=NC.car[1]||NG.record!=NC.record[1]||
    NG.car!=player_car()||rd32(NG.record)!=NG.car)return -1;
 if(NG.placed)return 1;
 if(NC.control.phase!=CC_LOADING||!ncp_bind_ready(NC.race))return -1;
 uint32_t car=NG.car;
 if(!ni_ready(car))return 0;
 /* The own model may have been restored since bind: use its current COM,
  * expressed in the current local basis, rather than the old bot's COM. */
 float basis[16]={0},offset[3]={0},p[3],zero[3]={0};
 for(int i=0;i<3;i++)for(int j=0;j<3;j++)basis[4*i+j]=MRF32(car+0x90+16*i+4*j);
 if(!ng_basis(basis))return -1;
 for(int j=0;j<3;j++){
  float delta=MRF32(car+0x60+4*j)-MRF32(car+0x50+4*j);
  if(!__builtin_isfinite(delta)||__builtin_fabsf(delta)>10.f)return -1;
  for(int i=0;i<3;i++)offset[i]+=delta*basis[4*i+j];
 }
 for(int j=0;j<3;j++){
  p[j]=NG.origin[j];for(int i=0;i<3;i++)p[j]+=offset[i]*NG.matrix[4*i+j];
  if(!__builtin_isfinite(p[j]))return -1;
 }
 ni_set_state(rd32(rd32(car+0x3c)+0x68),p,NG.matrix,zero,zero);
 for(int j=0;j<3;j++){
  MRF32(car+0x60+4*j)=p[j];MRF32(car+0x50+4*j)=NG.origin[j];
  MRF32(car+0x70+4*j)=0;MRF32(car+0x110+4*j)=0;
  for(int i=0;i<3;i++){
   MRF32(car+0x90+16*i+4*j)=NG.matrix[4*i+j];
   MRF32(car+0xd0+16*i+4*j)=NG.matrix[4*i+j];
  }
 }
 gcall1(c,FN_ROADTRACK,car+0x8d8,car+0x60);
 gcall0(c,FN_CAR_SECTION,car);
 NG.placed=1;
 rlog("[coop-grid] guest placed in replaced AI grid; own car/controller retained, run=%08x\n",NG.run);
 return 1;
}
