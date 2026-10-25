#include <stdio.h>
#include <stddef.h>
#include "roam_core.h"
int main(void){printf("pause=%u size=%u frame=%u active=%u\n",(unsigned)offsetof(rc_session,pause),(unsigned)sizeof(cp_pause),(unsigned)offsetof(cp_pause,frame_at),(unsigned)offsetof(cp_pause,active));}
