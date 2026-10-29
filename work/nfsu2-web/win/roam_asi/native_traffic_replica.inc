/* Activate before nv_begin freezes the controller pointer. Activate always
 * creates the stock record-kind3 controller, even when another was installed.
 * Activating after model attachment invalidated nv_slot.brain next frame. */
static struct {uint32_t car,world,body,render,ride,type;int64_t at;} NW_VISIBLE[RC_TRAFFIC_CARS];
static int nw_render_attached(uint32_t car){
 uint32_t render=rd32(car+0x554);
 return ok(car,0x9e0)&&MR8(car+0x550)&&MR8(car+0x5c0)&&ok(render,12)&&
  rd32(render)==car&&rd32(render+4)==rd32(car+0x10)&&rd32(render+8)==rd32(car+0xc);
}
static int nw_contact_visible(uint32_t car,int64_t now){
 if(!NW.enabled||NW.host||!NW.owned||!nw_same())return 1;
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_pool(pool);
 /* A partially rebuilt pool must not classify a native traffic record as
  * an unrelated object and fail open. Record kind is independent of brain. */
 uint32_t record=rd32(car+0x14);
 int traffic=ok(car,0x9e0)&&ok(record,0x940)&&rd32(record+4)==3;
 for(unsigned k=0;k<n;k++)if(pool[k]==car)traffic=1;
 for(unsigned k=0;k<RC_TRAFFIC_CARS;k++)if(NW.replica[k].car==car)traffic=1;
 if(!traffic)return 1;
 for(unsigned k=0;k<RC_TRAFFIC_CARS;k++)if(NW.replica[k].car==car){
  const __typeof__(NW_VISIBLE[0]) *v=&NW_VISIBLE[k];
  return v->car==car&&v->world==world()&&now>=v->at&&now-v->at<300000&&
   now>=NW.client.last_rx&&now-NW.client.last_rx<300000&&
   v->body==rd32(car+0x3c)&&v->render==rd32(car+0x554)&&
   v->ride==rd32(car+0x10)&&v->type==nw_type(car)&&nw_render_attached(car);
 }
 return 0; /* The client's unassigned native traffic is not authoritative. */
}
static int nw_contact_body_visible(uint32_t body,int64_t now){
 if(!body||!NW.enabled||NW.host||!nw_same())return 1;
 uint32_t w=world();if(!ok(w,0x118))return 1;
 unsigned end=rd32(w+0x10);if(end>22)return 1;
 /* Identity lookup only: do not assume a dynamic-body vtable or +130 car
  * backlink. Kinematic/native replacement bodies need the same visibility
  * gate, without changing their flags or treating scenery as a car. */
 for(unsigned i=0;i<end;i++){
  uint32_t car=rd32(w+0x1c+4*i);
  if(ok(car,0x9e0)&&rd32(car+0x3c)==body)return nw_contact_visible(car,now);
 }
 return 1;
}
static void nw_withdraw_pool(CPU *c,const uint32_t *pool,unsigned n){
 memset(NW_VISIBLE,0,sizeof NW_VISIBLE);
 for(unsigned i=0;i<n;i++)if(ok(pool[i],0x9e0))body_set(c,pool[i],0);
}
static int nw_prepare_replica(CPU *c,nv_slot *model,uint32_t car){
 if(!ok(car,0x9e0))return 0;
 int activate=!MR8(car+0x550),replace=!nw_traffic_driver(rd32(car+0x24));
 if(activate||replace){
  if(model->started)nv_reset(c,model);
  if(model->started)return 0; /* Live model pinned after ownership change. */
  body_set(c,car,0);
  if(activate)gcall0(c,FN_CAR_ACTIVATE,car);
  if(!nw_traffic_driver(rd32(car+0x24)))gcall1(c,FN_CAR_SETDRIVER,car,4);
  body_set(c,car,0);
 }
 uint32_t brain=rd32(car+0x2c);
 return ok(brain,0x79c)&&nw_traffic_controller(rd32(car+0x24),rd32(brain));
}
static void nw_park_replica(CPU *c,uint32_t car){
 if(!ok(car,0x9e0))return;
 body_set(c,car,0);gcall0(c,FN_CAR_DEACTIVATE,car);
 /* Deactivate installs NullController, unsuitable for native pool queries.
  * Restore a stock controller while leaving the car inactive/body disabled. */
 if(!nw_traffic_driver(rd32(car+0x24)))gcall1(c,FN_CAR_SETDRIVER,car,4);
 body_set(c,car,0);
}
static int nw_prepare_physics(CPU *c,uint32_t car){
 if(!ok(car,0x9e0))return 0;
 uint32_t mode=rd32(car+0x28);
 /* SetPhysics(2) falls back to mode4 while the native cell is unavailable.
  * That nonzero fallback must not permanently block the ordinary traffic
  * mode after streaming catches up. Keep native dynamic impact modes. */
 if(!mode||(mode==4&&MR8(car+0x5c0))){
  body_set(c,car,0);
  gcall1(c,FN_CAR_SETPHYS,car,2);
 }
 return rd32(car+0x28)!=0&&ok(rd32(car+0x34),0x10);
}
static int nw_apply_visible(CPU *c,Slot *slot,const rc_pose *pose,float dt){
 if(!slot||!ok(slot->car,0x9e0))return 0;
 body_set(c,slot->car,0);
 if(!pose){soft_park(c,slot->car);return 0;}
 if(!apply_pose_mode(c,slot,pose,dt,1))return 0;
 /* Section updates may withdraw the native cell or replace physics. Only
  * expose a body after the final pose/section update and native render attach.
  * Never force the streaming flag to make an unloaded car collidable. */
 if(!nw_render_attached(slot->car)){
  body_set(c,slot->car,0);return 0;
 }
 body_set(c,slot->car,1);
 for(unsigned k=0;k<RC_TRAFFIC_CARS;k++)if(NW.replica[k].car==slot->car){
  __typeof__(NW_VISIBLE[0]) *v=&NW_VISIBLE[k];uint32_t car=slot->car;
  v->car=car;v->world=world();v->body=rd32(car+0x3c);v->render=rd32(car+0x554);
  v->ride=rd32(car+0x10);v->type=nw_type(car);v->at=roam_plat_now_us();break;
 }
 return 1;
}
