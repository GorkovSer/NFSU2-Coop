/* Session-only frame liveness. Caller holds the platform lock. No engine
 * reads, invented snapshots or background game simulation are permitted. */
#ifndef FRAME_WATCH_H
#define FRAME_WATCH_H
#include "roam_core.h"
typedef struct { int64_t last_frame; int seen, parked; } frame_watch;
static void fw_frame(frame_watch *w, rc_session *s, int64_t now){
    if(w->parked){
        /* Source timestamps stop with the game. Start a fresh interpolation
         * clock instead of joining samples across a background interval. */
        rc_bump_clock(s);
        w->parked=0;
        /* The game frame publishes its actual status and fresh state next. */
    }
    w->last_frame=now;w->seen=1;
}
static int fw_tick(frame_watch *w, rc_session *s, int64_t now){
    if(!w->seen||w->parked||s->closed||s->status!=RC_ST_ROAM||
       now-w->last_frame<750000)return 0;
    w->parked=1;
    rc_set_status(s,RC_ST_PAUSED);
    return 1;
}
#endif
