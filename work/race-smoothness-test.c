/* Reproduces the old scripted guest's held-pose/new-timestamp pattern.
 * Real render interpolator, 20 Hz packets, 120 Hz frames, 288 km/h car. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "coop_stream.h"
typedef struct {float min_step,max_step,velocity_rmse;unsigned backwards,frames;} metrics;
static metrics simulate(int fresh_only){
 cs_stream s;cs_init(&s,100,200,1,2,1,4);
 metrics out={1000,-1000,0,0,0};float previous=0;int had=0;double errors=0;
 int64_t next_send=10000,last_sample=-1;uint16_t seq=0;
 for(int64_t now=0;now<12000000;now+=8333){
  while(next_send<=now){
   int64_t sample=(next_send-5000)/60000*60000;
   if(!fresh_only||sample!=last_sample){
    rc_race_frame f={0};f.to=100;f.run=1;f.event=2;f.racers=4;f.seq=seq++;f.n=1;f.car[0].slot=1;
    rc_state *st=&f.car[0].state;st->q[3]=1;st->v[0]=80;st->speed=80;st->p[0]=80.f*(float)sample/1000000.f;
    st->t_us=(uint32_t)(fresh_only?sample:next_send);
    if(!cs_receive(&s,200,&f,next_send))abort();last_sample=sample;
   }
   next_send+=50000;
  }
  rc_pose pose;if(cs_pose(&s,1,now,8333,&pose)){
   if(now>2000000&&had){float step=pose.p[0]-previous;
    if(step<out.min_step)out.min_step=step;if(step>out.max_step)out.max_step=step;
    if(step<-.001f)out.backwards++;
    double error=step/0.008333-80;errors+=error*error;out.frames++;
   }
   previous=pose.p[0];had=1;
  }
 }
 out.velocity_rmse=(float)sqrt(errors/out.frames);return out;
}
int main(void){
 metrics old=simulate(0),fixed=simulate(1);
 printf("old helper: frame step %.4f..%.4f m; backwards=%u; speed RMSE %.3f m/s\n",old.min_step,old.max_step,old.backwards,old.velocity_rmse);
 printf("timestamp-preserving helper: frame step %.4f..%.4f m; backwards=%u; speed RMSE %.3f m/s\n",fixed.min_step,fixed.max_step,fixed.backwards,fixed.velocity_rmse);
 if(!old.backwards||fixed.backwards||fixed.velocity_rmse>old.velocity_rmse*.15f)return 1;
 puts("PASS: actual race interpolator reproduces the scripted-peer stutter; retaining source sample times and skipping repeated samples removes it");return 0;
}
