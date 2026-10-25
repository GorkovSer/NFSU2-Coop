/* Included by roam_core.c, preserving single-source platform builds. */
void ar_init(ar_roster *a,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,int host,int racers){
 memset(a,0,sizeof *a);
 if(!self||!peer||self==peer||!run||!event||racers<2||racers>AR_SLOTS)return;
 a->self=self;a->peer=peer;a->run=run;a->event=event;a->host=!!host;a->racers=racers;a->active=1;
}
int ar_fragment_valid(const ar_fragment *f){
 if(!f||!f->to||!f->run||!f->event||!f->physics||f->racers<2||f->racers>AR_SLOTS||
    f->slot<2||f->slot>=f->racers||!f->total||f->total>AR_CFG_MAX)return 0;
 unsigned n=(f->total+AR_FRAGMENT-1)/AR_FRAGMENT;
 if(f->fragments!=n||f->fragment>=n)return 0;
 unsigned offset=f->fragment*AR_FRAGMENT,len=f->total-offset;
 if(len>AR_FRAGMENT)len=AR_FRAGMENT;
 return f->dlen==len&&f->data!=0;
}
static int ar_complete(const ar_roster *a){
 for(int k=0;k<a->racers-2;k++)if(!a->car[k].complete)return 0;
 return 1;
}
static uint32_t ar_digest(const ar_roster *a){
 uint8_t bytes[80];rc_w w;rc_w_init(&w,bytes,sizeof bytes);
 rc_put8(&w,1);rc_put8(&w,(uint8_t)a->racers);
 for(int k=0;k<a->racers-2;k++){
  rc_put8(&w,(uint8_t)(k+2));rc_put16(&w,a->car[k].length);
  rc_put32(&w,a->car[k].digest);rc_put32(&w,a->car[k].physics);
 }
 return rc_crc32(bytes,w.n);
}
static int ar_verify(ar_roster *a){
 if(!a->host&&a->manifest_seen&&ar_complete(a)&&ar_digest(a)!=a->manifest){a->fault=1;return 0;}
 return 1;
}
int ar_host_set(ar_roster *a,int slot,const uint8_t *bytes,int n,uint32_t physics){
 if(!a->active||!a->host||a->sealed||slot<2||slot>=a->racers||!bytes||n<1||n>AR_CFG_MAX||!physics)return 0;
 int k=slot-2;
 if(a->car[k].complete)return a->car[k].length==n&&a->car[k].physics==physics&&!memcmp(a->car[k].bytes,bytes,(unsigned)n);
 a->car[k].length=(uint16_t)n;a->car[k].physics=physics;a->car[k].digest=rc_fnv1a_bytes(bytes,(uint32_t)n);
 memcpy(a->car[k].bytes,bytes,(unsigned)n);a->car[k].complete=1;return 1;
}
int ar_seal(ar_roster *a){
 if(!a->active||!a->host||a->fault||!ar_complete(a))return 0;
 if(!a->sealed){a->manifest=ar_digest(a);a->sealed=1;}return 1;
}
int ar_receive(ar_roster *a,uint32_t from,const ar_fragment *f){
 if(!a->active||a->host||a->fault||from!=a->peer||!ar_fragment_valid(f)||f->to!=a->self||
    f->run!=a->run||f->event!=a->event||f->racers!=a->racers)return 0;
 int k=f->slot-2;
 if(a->car[k].length&&(a->car[k].length!=f->total||a->car[k].digest!=f->digest||a->car[k].physics!=f->physics)){a->fault=1;return -1;}
 unsigned offset=f->fragment*AR_FRAGMENT,bit=1u<<f->fragment;
 if(a->car[k].mask&bit){
  if(memcmp(a->car[k].bytes+offset,f->data,f->dlen)){a->fault=1;return -1;}
  return 1;
 }
 a->car[k].length=f->total;a->car[k].digest=f->digest;a->car[k].physics=f->physics;
 memcpy(a->car[k].bytes+offset,f->data,f->dlen);a->car[k].mask|=(uint8_t)bit;
 if(a->car[k].mask==(1u<<f->fragments)-1u){
  if(rc_fnv1a_bytes(a->car[k].bytes,a->car[k].length)!=a->car[k].digest){a->fault=1;return -1;}
  a->car[k].complete=1;
 }
 return ar_verify(a)?1:-1;
}
int ar_control(ar_roster *a,uint32_t from,uint8_t op,uint32_t to,uint32_t run,uint32_t event,uint32_t value){
 if(!a->active||a->fault||from!=a->peer||to!=a->self||run!=a->run||event!=a->event)return 0;
 if(a->host){
  if(op!=AR_ACK||!a->sealed||value!=a->manifest)return 0;
  a->peer_ack=1;return 1;
 }
 if(op!=AR_MANIFEST)return 0;
 if(a->manifest_seen&&value!=a->manifest){a->fault=1;return -1;}
 a->manifest=value;a->manifest_seen=1;
 return ar_verify(a)?1:-1;
}
int ar_loaded(ar_roster *a,int slot,uint32_t digest,uint32_t physics){
 if(!a->active||a->host||a->fault||!a->manifest_seen||slot<2||slot>=a->racers)return 0;
 int k=slot-2;if(!a->car[k].complete)return 0;
 if(a->car[k].digest!=digest||a->car[k].physics!=physics){a->fault=1;return 0;}
 a->car[k].loaded=1;return 1;
}
int ar_ready(const ar_roster *a){
 if(!a->active||a->fault)return 0;
 if(a->host)return a->sealed&&a->peer_ack;
 if(!a->manifest_seen||!ar_complete(a))return 0;
 for(int k=0;k<a->racers-2;k++)if(!a->car[k].loaded)return 0;
 return 1;
}
int ar_next_fragment(ar_roster *a,int64_t now,ar_fragment *f){
 if(!a->active||!a->host||!a->sealed||a->fault||a->peer_ack||now<a->next_fragment||a->racers<=2)return 0;
 int fragments=0;
 for(int k=0;k<a->racers-2;k++)fragments+=(a->car[k].length+AR_FRAGMENT-1)/AR_FRAGMENT;
 int cursor=a->cursor,slot=0;a->cursor=(a->cursor+1)%fragments;
 for(;slot<a->racers-2;slot++){
  int n=(a->car[slot].length+AR_FRAGMENT-1)/AR_FRAGMENT;if(cursor<n)break;cursor-=n;
 }
 memset(f,0,sizeof *f);f->to=a->peer;f->run=a->run;f->event=a->event;f->slot=(uint8_t)(slot+2);f->racers=(uint8_t)a->racers;
 f->digest=a->car[slot].digest;f->physics=a->car[slot].physics;f->total=a->car[slot].length;
 f->fragments=(uint8_t)((f->total+AR_FRAGMENT-1)/AR_FRAGMENT);f->fragment=(uint8_t)cursor;
 unsigned offset=(unsigned)cursor*AR_FRAGMENT;f->dlen=(uint16_t)(f->total-offset);if(f->dlen>AR_FRAGMENT)f->dlen=AR_FRAGMENT;
 f->data=a->car[slot].bytes+offset;a->next_fragment=now+100000;return 1;
}
int ar_next_control(ar_roster *a,int64_t now,uint8_t *op,uint32_t *value){
 if(!a->active||a->fault||now<a->next_control)return 0;
 if(a->host){if(!a->sealed||a->peer_ack)return 0;*op=AR_MANIFEST;}
 else {if(!ar_ready(a))return 0;*op=AR_ACK;}
 *value=a->manifest;a->next_control=now+250000;return 1;
}
