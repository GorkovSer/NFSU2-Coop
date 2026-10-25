/* Mode-specific results. No game, network, clock or profile access. */
#ifndef RACE_OUTCOME_H
#define RACE_OUTCOME_H
#include <stdint.h>
#define RM_CARS 6
#define RM_ROUNDS 4
#define RM_MAX_SCORE 1000000000u
#define RM_MAX_TICKS 345600000u
#define RC_EXT_RACE_MODES 4u
#define RC_EXT_RANK_TABLE 8u
#define RC_EXT_RANK_CLOCK 16u
#define RM_INBOX 16
enum {RM_URL=3,RM_DRAG=4,RM_DRIFT=5,RM_RANK=6};
enum {RM_LIVE=0,RM_FINISH=1,RM_TOTALLED=8,RM_BLOWN=9,RM_TIMEOUT=11};
enum {RM_TERMS=1,RM_TERMS_ACK,RM_OWNER,RM_BOARD,RM_BOARD_ACK};
typedef struct {uint32_t ticks,score;uint8_t status;uint32_t wall;} rm_value;
typedef struct {
 uint32_t to,source,destination,run,event,layout,serial;
 uint8_t op,mode,round,rounds,racers,final;
 rm_value car[RM_CARS];
 uint32_t elapsed[RM_CARS]; /* final board: shared host pause ledger */
} rm_packet;
typedef struct {
 uint32_t run,event,layout,serial,received_serial;
 int mode,round,rounds,racers,host,initialized,fault,agreed,peer_agreed,seen,final,board_ack;
 rm_value car[RM_CARS];uint8_t rank[RM_CARS];
 uint32_t elapsed[RM_CARS];
 uint32_t points[RM_CARS];uint8_t last_rank[RM_CARS];int rounds_committed;
 int64_t next_tx;
} rm_outcome;
int rm_value_valid(const rm_value *);
int rm_packet_valid(const rm_packet *);
int rm_init(rm_outcome *,uint32_t run,uint32_t event,uint32_t layout,int mode,int round,int rounds,int racers,int host);
int rm_set(rm_outcome *,int slot,const rm_value *);
int rm_receive(rm_outcome *,const rm_packet *);
int rm_next(rm_outcome *,int64_t now,rm_packet *);
int rm_ready(const rm_outcome *);
int rm_resolve(rm_outcome *);
int rm_close_ai(rm_outcome *,uint32_t ticks,uint32_t wall);
int rm_timing(rm_outcome *,const uint32_t elapsed[RM_CARS]);
int rm_team_winner(const rm_outcome *); /* canonical slot+1; 0 if neither human */
int rm_commit_round(rm_outcome *); /* URL only, exactly once */
int rm_advance(rm_outcome *,uint32_t next_layout);
#endif
