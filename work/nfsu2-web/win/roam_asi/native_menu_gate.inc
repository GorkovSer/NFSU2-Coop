/* Race invitation UI retains live flow6/World. Shared traffic must not
 * mutate assets/controllers while the native free-roam scene is paused. */
static int ng_city_suspended(void){
 float dt=MRF32(G_WORLDDT);
 return roaming()&&(nc_pause_raw(0)||!__builtin_isfinite(dt)||dt<=0.f);
}
static int nw_frame_allowed(void){
 return !nc_pause_held()&&!ng_city_suspended();
}
