/* Host-owned traffic roster. No engine calls, allocation or threads. */
#ifndef TRAFFIC_SYNC_H
#define TRAFFIC_SYNC_H
#include "roam_core.h"
typedef struct {
    uint32_t self,host,world,context;
    uint16_t seq;
    int seen,active;
    int64_t scene_since,last_rx;
    struct {uint32_t id,type;rc_interp motion;} car[RC_TRAFFIC_CARS];
} ts_client;
void ts_init(ts_client *s,uint32_t self,uint32_t host,int64_t now);
void ts_scene(ts_client *s,uint32_t context,int64_t now);
int ts_receive(ts_client *s,uint32_t from,const rc_traffic_frame *f,int64_t now,int32_t remote_to_local);
int ts_expire(ts_client *s,int64_t now);
int ts_pose(ts_client *s,uint32_t id,int64_t now,int64_t dt,rc_pose *pose);
#endif
