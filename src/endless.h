#ifndef _ENDLESS_H_
#define _ENDLESS_H_

#include "racer.h"
#include "save_layout.h"
#include "structs.h"
#include "types.h"

/**
 * @file Endless Trophy Race mode.
 *
 * An arcade-style mode built on top of the Trophy Race state machine: an
 * infinite sequence of seeded races where the AI escalates, tracks eventually
 * mirror, and optional seeded event rules appear. Survival ends on a missed
 * placement; Time Attack ends when the run clock expires. Entered from the
 * Tracks menu trophy race column, which this hack repurposes.
 */

void endless_start(void);
void endless_retry_seed(void);
void endless_new_seed(void);
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
s32 endless_seed(void);
void endless_set_seed(s32 seed);
void endless_seed_move_digit(s32 delta);
void endless_seed_change_digit(s32 delta);
void endless_seed_mark_dirty(void);
void endless_seed_clear_dirty(void);
s32 endless_seed_refresh_pending(void);
s32 endless_seed_refresh_due(s32 updateRate);
s32 endless_time_attack(void);
void endless_toggle_time_attack(void);
s32 endless_events_enabled(void);
void endless_toggle_events(void);
s32 endless_event_active(void);
s32 endless_event_cheats(void);
s32 endless_bounty_active(void);
void endless_note_coins(s32 coins);
s32 endless_bounty_claimed(void);
s32 endless_vehicle(void);
char *endless_vehicle_text(void);
s32 endless_clock(void);
s32 endless_run_continues(void);
void endless_round_finished(s32 roundPoints);
s32 endless_ai_level(s32 baseLevel);
void endless_scale_ai_table(AIBehaviourTable *table);
s32 endless_score(void);
s32 endless_best_rounds(void);
EndlessRecords *endless_records_pointer(void);
s32 endless_record_run(s32 roundsCleared, s32 score);
void endless_clear_records(void);
s32 endless_new_best(void);
char *endless_round_text(void);
char *endless_score_text(void);
char *endless_result_text(void);
char *endless_result_detail_text(void);
char *endless_intro_status_text(void);
char *endless_hud_text(void);
char *endless_best_text(void);
char *endless_perk_text(void);
char *endless_clock_text(void);
char *endless_speed_text(s32 speed);
char *endless_mode_text(void);
char *endless_event_text(void);
char *endless_seed_prefix_text(void);
char *endless_seed_digit_text(void);

#endif
