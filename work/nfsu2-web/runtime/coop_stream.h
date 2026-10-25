/* Authority and interpolation for one fixed race roster. No engine access. */
#ifndef COOP_STREAM_H
#define COOP_STREAM_H
#include "roam_core.h"
typedef struct {
    uint32_t self,peer,run,event;
    int host,racers,seq_init;
    uint16_t seq;
    int64_t last_rx;
    uint8_t position[RC_RACE_CARS],completion[RC_RACE_CARS];
    rc_interp motion[RC_RACE_CARS];
} cs_stream;
void cs_init(cs_stream *s,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,int host,int racers);
int cs_receive(cs_stream *s,uint32_t from,const rc_race_frame *f,int64_t now);
int cs_pose(cs_stream *s,int slot,int64_t now,int64_t dt,rc_pose *pose);
int cs_pose_delay(cs_stream *s,int slot,int64_t now,int64_t dt,int64_t delay_us,rc_pose *pose);
int cs_stale(const cs_stream *s,int64_t now);
void cs_reanchor(cs_stream *s,int64_t now);
#endif
