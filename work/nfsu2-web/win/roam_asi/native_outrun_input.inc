/* Called on the game thread by the guarded native Engage hook. */
static int NR_ENGAGE_PRESS;
static uint32_t NR_ENGAGE_WORLD;
int nr_engage_intercept(void){
 if(!nr_outruns_enabled()||!roaming()||!g_in_roam)return 0;
 uint32_t pl=rd32(G_PLAYER1),race=rd32(G_RACE);
 if(!ok(pl,PL_MARKER+4)||rd32(pl+PL_MARKER)||!ok(race,0xc44)||
    MR32(race+0x24)!=1||MR32(G_RACETYPE))return 0;
 int target=-1;
 if(g_du.st==DU_IDLE&&g_du.icon){
  for(int k=0;k<NSLOT;k++)if(g_sl[k].car==g_du.icon&&du_able(k)&&
      !g_sl[k].hidden&&du_dist(g_sl[k].car)<=DU_PROMPT_M){target=k;break;}
 }else if(g_du.st==DU_SENT||g_du.st==DU_GOT||g_du.st==DU_ACCEPTED){
  int k=du_slot(g_du.id);
  if(k>=0&&du_able(k)&&du_dist(g_sl[k].car)<=DU_KEEP_M)target=k;
 }
 if(target<0)return 0;
 NR_ENGAGE_PRESS=1;NR_ENGAGE_WORLD=world();
 return 1;
}
static int nr_take_engage(void){
 int press=NR_ENGAGE_PRESS&&NR_ENGAGE_WORLD==world();
 NR_ENGAGE_PRESS=0;NR_ENGAGE_WORLD=0;return press;
}
