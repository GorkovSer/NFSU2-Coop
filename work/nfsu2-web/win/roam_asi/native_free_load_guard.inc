/* Persistent per-slot dependency budget survives a failed acquisition.
 * A new CFG or World starts a new attempt; a missing brand is terminal for
 * this attempt, and a pending native header gets at most30 seconds. */
static struct {uint32_t world,peer,gen;int active,failed;int64_t since;} NFL[NSLOT];
static int nfl_attempt(int k,uint32_t w,uint32_t peer,uint32_t gen,int64_t now){
 if(!NFL[k].active||NFL[k].world!=w||NFL[k].peer!=peer||NFL[k].gen!=gen){
  memset(&NFL[k],0,sizeof NFL[k]);NFL[k].active=1;NFL[k].world=w;NFL[k].peer=peer;NFL[k].gen=gen;NFL[k].since=now;
 }
 return !NFL[k].failed;
}
static int nfl_dependency(int k,int ready,int64_t now){
 if(NFL[k].failed)return 0;
 if(ready<0||now<NFL[k].since||(ready==0&&now-NFL[k].since>30000000)){
  NFL[k].failed=1;rlog("[vehicle] free-roam dependency failed/timed out; retaining old model until new configuration or world\n");return 0;
 }
 return ready>0;
}
