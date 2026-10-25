/* Frozen host AI equipment, independent of either human's CFG channel. */
#ifndef RACE_ROSTER_H
#define RACE_ROSTER_H
#include <stdint.h>
#define AR_CFG_MAX 2048
#define AR_FRAGMENT 1000
#define AR_SLOTS 6
enum {AR_MANIFEST=22,AR_ACK=23};
typedef struct {
 uint32_t to,run,event,digest,physics;
 uint16_t total,dlen;
 uint8_t slot,racers,fragment,fragments;
 const uint8_t *data;
} ar_fragment;
typedef struct {
 uint32_t self,peer,run,event,manifest;
 int active,host,racers,sealed,manifest_seen,peer_ack,fault,cursor;
 int64_t next_control,next_fragment;
 struct {uint32_t digest,physics;uint16_t length;uint8_t mask,complete,loaded;uint8_t bytes[AR_CFG_MAX];} car[AR_SLOTS-2];
} ar_roster;
void ar_init(ar_roster *,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,int host,int racers);
int ar_fragment_valid(const ar_fragment *);
int ar_host_set(ar_roster *,int slot,const uint8_t *,int n,uint32_t physics);
int ar_seal(ar_roster *);
int ar_receive(ar_roster *,uint32_t from,const ar_fragment *);
int ar_control(ar_roster *,uint32_t from,uint8_t op,uint32_t to,uint32_t run,uint32_t event,uint32_t value);
int ar_loaded(ar_roster *,int slot,uint32_t digest,uint32_t physics);
int ar_ready(const ar_roster *);
int ar_next_fragment(ar_roster *,int64_t now,ar_fragment *);
int ar_next_control(ar_roster *,int64_t now,uint8_t *op,uint32_t *value);
#endif
