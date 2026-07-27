#ifndef _ENDLESS_H_
#define _ENDLESS_H_

#include "racer.h"
#include "structs.h"
#include "types.h"

/**
 * @file Endless Trophy Race mode.
 *
 * An arcade-style survival mode built on top of the Trophy Race state machine:
 * an infinite sequence of randomly drawn races where the AI gets faster and
 * more aggressive every round, the required finishing position tightens, and
 * tracks eventually mirror. The run ends the first time the player misses the
 * required position. Entered from the Tracks menu trophy race column, which
 * this hack repurposes.
 */

void endless_start(void);
void endless_stop(void);
s32 endless_is_active(void);
s32 endless_round(void);
void endless_advance_round(void);
s32 endless_pick_track(void);
s32 endless_current_track(void);
s32 endless_current_world(void);
s32 endless_required_position(void);
s32 endless_player_survived(void);
s32 endless_position_is_safe(s32 racePosition);
s32 endless_mirrored(void);
void endless_award_perk(void);
s32 endless_perk_bananas(void);
s32 endless_ai_level(s32 baseLevel);
void endless_scale_ai_table(AIBehaviourTable *table);
s32 endless_score(void);
s32 endless_best_rounds(void);
s32 endless_best_score(void);
void endless_record_run(s32 roundsCleared);
char *endless_round_text(void);
char *endless_score_text(void);
char *endless_goal_text(void);
char *endless_hud_text(void);
char *endless_best_text(void);
char *endless_perk_text(void);

#endif
