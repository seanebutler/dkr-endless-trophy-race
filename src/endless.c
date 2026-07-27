#include "endless.h"

#include "asset_enums.h"
#include "game.h"
#include "macros.h"
#include "math_util.h"
#include "menu.h"
#include "objects.h"
#include "structs.h"
#include "thread3_main.h"
#include "types.h"

/**
 * @file Endless Trophy Race mode logic.
 *
 * Holds the mode state and all tuning; menu.c and game.c call into here from
 * small hooks in the existing Trophy Race flow. gTrophyRaceRound is pinned to
 * 0 for the whole run so the vanilla rankings screen always takes its
 * "continue to next round" path; the real round counter lives here.
 */

// Rounds are 0-based internally; shown to the player as round + 1.
#define ENDLESS_AI_BASE_TABLE 2        // Behaviour table for round 1; +1 per round up to 9.
#define ENDLESS_AI_TABLE_MAX 9         // Top of the AI behaviour table bank.
#define ENDLESS_HEAT_START_ROUND 7     // Rounds past this scale the loaded table directly.
#define ENDLESS_SPEED_PER_HEAT 0.125f  // Extra AI speed bonus (virtual bananas) per heat step.
#define ENDLESS_SPEED_BONUS_CAP 4.0f   // Keep the AI near human banana parity so skill can still win.
#define ENDLESS_CHANCE_PER_HEAT 2      // Extra AI action chance (percent) per heat step.
#define ENDLESS_MIRROR_CHANCE_ROUND 8  // Rounds with a coin-flip mirror.
#define ENDLESS_MIRROR_ALWAYS_ROUND 12 // Every race mirrored from here on.

// Head start for the next round, by how well this one finished. The human
// banana cap is 10, so even a win leaves plenty of room to collect more.
#define ENDLESS_PERK_FIRST 5
#define ENDLESS_PERK_SECOND 3
#define ENDLESS_PERK_THIRD 2

// Time Attack. The run clock is settled after each race as a percentage of the
// time that race took, which keeps the rule identical on a one-minute track and
// a three-minute one -- an absolute bonus would make short tracks free time and
// long ones a death sentence. Winning buys time, trailing bleeds it.
#define ENDLESS_TA_START_SECONDS 180
#define ENDLESS_TICKS_PER_SECOND 60
#define ENDLESS_TA_PCT_FIRST 30
#define ENDLESS_TA_PCT_SECOND 10
#define ENDLESS_TA_PCT_THIRD 0
#define ENDLESS_TA_PCT_FOURTH (-10)
#define ENDLESS_TA_PCT_REST (-25)

// Seeds are four digits so they can be read out loud and typed back in.
#define ENDLESS_SEED_MAX 9999

// Where the run record lives in the EEPROM settings word. Bits 0-25 are the
// vanilla flags (Adventure Two, Drumstick, language, T.T. course times,
// subtitles) and write_eeprom_settings reserves bits 56-63 for its checksum,
// which leaves 26-55 unused. Nothing here can reach these ceilings in practice.
#define ENDLESS_SAVE_ROUNDS_SHIFT 32
#define ENDLESS_SAVE_ROUNDS_MASK ((u64) 0x3FF) // 10 bits, up to 1023 rounds
#define ENDLESS_SAVE_SCORE_SHIFT 42
#define ENDLESS_SAVE_SCORE_MASK ((u64) 0x3FFF) // 14 bits, up to 16383 points

// Layout of ASSET_MISC_TRACKS_MENU_IDS: one row of 6 entries per world, the
// first 4 being races and the last 2 the trophy race and battle arena.
#define ENDLESS_WORLD_COUNT 5
#define ENDLESS_RACES_PER_WORLD 4
#define ENDLESS_MAX_POOL (ENDLESS_WORLD_COUNT * ENDLESS_RACES_PER_WORLD)

/************ .data ************/

s32 gEndlessActive = FALSE;
s32 gEndlessRound = 0;
s32 gEndlessTrackId = -1;
s32 gEndlessTrackWorld = 1;
s32 gEndlessMirrorThisRace = FALSE;
s32 gEndlessPerkBananas = 0;
s32 gEndlessTimeAttack = FALSE; // Kept between runs: it is a preference.
s32 gEndlessClock = 0;
s32 gEndlessSeed = 0;

/*******************************/

/************ .bss ************/

static s8 sEndlessPool[ENDLESS_MAX_POOL];
static s8 sEndlessPoolWorld[ENDLESS_MAX_POOL];
static s32 sEndlessPoolSize;
static s32 sEndlessPoolCursor;
static char sEndlessRoundText[16];
static char sEndlessScoreText[24];
static char sEndlessGoalText[24];
static char sEndlessHudText[24];
static char sEndlessBestText[32];
static char sEndlessPerkText[32];
static char sEndlessClockText[24];
static char sEndlessModeText[32];
static s32 sEndlessLastTrack;
static u32 sEndlessRngState;

/******************************/

/**
 * The run's own random stream. The game's shared rand_range is advanced by
 * everything from particles to AI decisions, so a run drawn from it could
 * never be reproduced; this keeps the track order and mirror rolls dependent
 * on nothing but the seed.
 */
static void endless_rng_seed(s32 seed) {
    // Spread the low-entropy display seed across the whole word, or seeds 1
    // and 2 would open with near-identical draws.
    sEndlessRngState = ((u32) seed * 2654435761U) + 1U;
}

static u32 endless_rng_next(void) {
    sEndlessRngState = (sEndlessRngState * 1664525U) + 1013904223U;
    // The high bits of an LCG are far better distributed than the low ones.
    return sEndlessRngState >> 16;
}

/**
 * Random value in [0, max], matching rand_range's inclusive contract.
 */
static s32 endless_rng_range(s32 max) {
    if (max <= 0) {
        return 0;
    }
    return (s32) (endless_rng_next() % (u32) (max + 1));
}

/**
 * Append a decimal number to a string buffer. Returns the new end pointer.
 * (sprintf is not exposed in a header, so keep formatting self-contained.)
 */
static char *endless_append_number(char *dst, s32 value) {
    char digits[12];
    s32 count = 0;

    if (value < 0) {
        *dst++ = '-';
        value = -value;
    }
    do {
        digits[count++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);
    while (count > 0) {
        *dst++ = digits[--count];
    }
    *dst = '\0';
    return dst;
}

static char *endless_append_string(char *dst, const char *src) {
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
    return dst;
}

/**
 * Build the pool of candidate tracks from the tracks-menu level table: the
 * four race slots of every world, Future Fun Land included, filtered to
 * standard races only. The table's remaining slots per world are the trophy
 * race and the battle arena, which are not raceable rounds.
 *
 * Future Fun Land is drawn from the table directly rather than through the
 * menu, so its tracks are in the rotation from round one without needing the
 * adventure-mode unlock -- this mode is separate from that progression.
 */
static void endless_build_pool(void) {
    s8 *levelIds = (s8 *) get_misc_asset(ASSET_MISC_TRACKS_MENU_IDS);
    s32 world;
    s32 slot;
    s32 id;

    sEndlessPoolSize = 0;
    for (world = 1; world <= ENDLESS_WORLD_COUNT; world++) {
        for (slot = 0; slot < ENDLESS_RACES_PER_WORLD; slot++) {
            id = levelIds[((world - 1) * 6) + slot];
            if (id < 0) {
                continue;
            }
            if (leveltable_type(id) != RACETYPE_DEFAULT) {
                continue;
            }
            if (sEndlessPoolSize < ENDLESS_MAX_POOL) {
                sEndlessPool[sEndlessPoolSize] = id;
                sEndlessPoolWorld[sEndlessPoolSize] = world;
                sEndlessPoolSize++;
            }
        }
    }
}

/**
 * Fisher-Yates shuffle of the track pool, so every track appears once
 * before any repeats.
 */
static void endless_shuffle_pool(void) {
    s32 i;
    s32 j;
    s8 temp;

    for (i = sEndlessPoolSize - 1; i > 0; i--) {
        j = endless_rng_range(i);
        temp = sEndlessPool[i];
        sEndlessPool[i] = sEndlessPool[j];
        sEndlessPool[j] = temp;
        temp = sEndlessPoolWorld[i];
        sEndlessPoolWorld[i] = sEndlessPoolWorld[j];
        sEndlessPoolWorld[j] = temp;
    }
    sEndlessPoolCursor = 0;
}

void endless_start(void) {
    Settings *settings = get_settings();
    s32 i;

    gEndlessActive = TRUE;
    gEndlessRound = 0;
    gEndlessTrackId = -1;
    gEndlessTrackWorld = 1;
    gEndlessMirrorThisRace = FALSE;
    gEndlessPerkBananas = 0;
    gEndlessClock = normalise_time(ENDLESS_TA_START_SECONDS * ENDLESS_TICKS_PER_SECOND);
    for (i = 0; i < 8; i++) {
        settings->racers[i].trophy_points = 0;
    }
    // A fresh run gets a random seed; this also builds and shuffles the bag.
    // The player can still change it on the first round's intro.
    endless_set_seed(rand_range(0, ENDLESS_SEED_MAX));
}

void endless_stop(void) {
    gEndlessActive = FALSE;
}

s32 endless_is_active(void) {
    return gEndlessActive;
}

s32 endless_round(void) {
    return gEndlessRound;
}

void endless_advance_round(void) {
    gEndlessRound++;
}

/**
 * Draw the next track from the shuffle bag and roll this race's mirroring.
 */
s32 endless_pick_track(void) {
    s8 temp;

    if (sEndlessPoolSize <= 0) {
        endless_build_pool();
    }
    if (sEndlessPoolCursor >= sEndlessPoolSize) {
        endless_shuffle_pool();
        // Avoid a back-to-back repeat across the bag boundary.
        if (sEndlessPoolSize > 1 && sEndlessPool[0] == sEndlessLastTrack) {
            temp = sEndlessPool[0];
            sEndlessPool[0] = sEndlessPool[sEndlessPoolSize - 1];
            sEndlessPool[sEndlessPoolSize - 1] = temp;
            temp = sEndlessPoolWorld[0];
            sEndlessPoolWorld[0] = sEndlessPoolWorld[sEndlessPoolSize - 1];
            sEndlessPoolWorld[sEndlessPoolSize - 1] = temp;
        }
    }
    gEndlessTrackId = sEndlessPool[sEndlessPoolCursor];
    gEndlessTrackWorld = sEndlessPoolWorld[sEndlessPoolCursor];
    sEndlessPoolCursor++;
    sEndlessLastTrack = gEndlessTrackId;

    if (gEndlessRound >= ENDLESS_MIRROR_ALWAYS_ROUND) {
        gEndlessMirrorThisRace = TRUE;
    } else if (gEndlessRound >= ENDLESS_MIRROR_CHANCE_ROUND) {
        gEndlessMirrorThisRace = endless_rng_range(1);
    } else {
        gEndlessMirrorThisRace = FALSE;
    }
    return gEndlessTrackId;
}

s32 endless_current_track(void) {
    return gEndlessTrackId;
}

s32 endless_current_world(void) {
    return gEndlessTrackWorld;
}

/**
 * Worst allowed finishing position (0-based) for the current round.
 * Top 4 early, tightening to 1st place only from round 10 on.
 */
s32 endless_required_position(void) {
    if (gEndlessRound >= 9) {
        return 0;
    }
    if (gEndlessRound >= 6) {
        return 1;
    }
    if (gEndlessRound >= 3) {
        return 2;
    }
    return 3;
}

/**
 * Finishing position of the best-placed human in the race just run, 0 = first.
 * starting_position holds the finish position of the last race.
 */
static s32 endless_best_finish(void) {
    Settings *settings = get_settings();
    s32 best = 99;
    s32 i;

    for (i = 0; i < get_number_of_active_players(); i++) {
        if (settings->racers[i].starting_position < best) {
            best = settings->racers[i].starting_position;
        }
    }
    return best;
}

/**
 * TRUE if the best-placed human met this round's required position.
 */
s32 endless_player_survived(void) {
    return endless_best_finish() <= endless_required_position();
}

/**
 * Turn the finish just achieved into a head start for the next round.
 *
 * Bananas are the right currency for this: they raise top speed, they are
 * lost on every hit, and they do not carry past the race. So a strong finish
 * buys a burst of speed that the escalating AI immediately starts taking back,
 * rather than a permanent advantage that would snowball.
 */
void endless_award_perk(void) {
    switch (endless_best_finish()) {
        case 0:
            gEndlessPerkBananas = ENDLESS_PERK_FIRST;
            break;
        case 1:
            gEndlessPerkBananas = ENDLESS_PERK_SECOND;
            break;
        case 2:
            gEndlessPerkBananas = ENDLESS_PERK_THIRD;
            break;
        default:
            gEndlessPerkBananas = 0;
            break;
    }
}

s32 endless_perk_bananas(void) {
    return gEndlessPerkBananas;
}

s32 endless_seed(void) {
    return gEndlessSeed;
}

/**
 * Point the run at a seed and rebuild the draw order from it. Safe to call
 * while the player is still on the first round's intro, which is the only
 * place the seed can be changed -- the bag is rebuilt from scratch each time,
 * so the same seed always produces the same run.
 */
void endless_set_seed(s32 seed) {
    while (seed < 0) {
        seed += ENDLESS_SEED_MAX + 1;
    }
    while (seed > ENDLESS_SEED_MAX) {
        seed -= ENDLESS_SEED_MAX + 1;
    }
    gEndlessSeed = seed;
    endless_rng_seed(seed);
    endless_build_pool();
    endless_shuffle_pool();
    sEndlessLastTrack = -1;
}

s32 endless_time_attack(void) {
    return gEndlessTimeAttack;
}

void endless_toggle_time_attack(void) {
    gEndlessTimeAttack = !gEndlessTimeAttack;
    gEndlessClock = normalise_time(ENDLESS_TA_START_SECONDS * ENDLESS_TICKS_PER_SECOND);
}

s32 endless_clock(void) {
    return gEndlessClock;
}

/**
 * Settle the run clock against the race just finished. The refund is a share
 * of that race's own duration, so the rule reads the same on every track.
 */
static void endless_settle_clock(void) {
    Settings *settings = get_settings();
    s32 raceTime = settings->racers[0].course_time;
    s32 percent;

    switch (endless_best_finish()) {
        case 0:
            percent = ENDLESS_TA_PCT_FIRST;
            break;
        case 1:
            percent = ENDLESS_TA_PCT_SECOND;
            break;
        case 2:
            percent = ENDLESS_TA_PCT_THIRD;
            break;
        case 3:
            percent = ENDLESS_TA_PCT_FOURTH;
            break;
        default:
            percent = ENDLESS_TA_PCT_REST;
            break;
    }
    gEndlessClock += (raceTime * percent) / 100;
    if (gEndlessClock < 0) {
        gEndlessClock = 0;
    }
}

/**
 * Whether the run survives the round that just finished. Survival mode ends on
 * a missed placement; Time Attack ignores placement entirely and ends only when
 * the clock runs dry, so a bad race costs time rather than the whole run.
 */
s32 endless_run_continues(void) {
    if (gEndlessTimeAttack) {
        return gEndlessClock > 0;
    }
    return endless_player_survived();
}

/**
 * Settle everything the finished round owes: the run clock, the save record,
 * and the head start for next time. Called once, as the rankings screen opens,
 * so that screen and the exit branch agree on the outcome.
 */
void endless_round_finished(void) {
    if (gEndlessTimeAttack) {
        endless_settle_clock();
    }
    if (endless_run_continues()) {
        endless_record_run(gEndlessRound + 1);
        endless_award_perk();
    } else {
        endless_record_run(gEndlessRound);
    }
}

char *endless_clock_text(void) {
    s32 seconds = gEndlessClock / normalise_time(ENDLESS_TICKS_PER_SECOND);
    char *end = endless_append_string(sEndlessClockText, "TIME ");

    end = endless_append_number(end, seconds / 60);
    *end++ = ':';
    // Seconds are always two digits, or "2:5" would read as two minutes five.
    *end++ = (char) ('0' + ((seconds % 60) / 10));
    *end++ = (char) ('0' + ((seconds % 60) % 10));
    *end = '\0';
    return sEndlessClockText;
}

/**
 * Mode and seed share a line on the first round's intro. They are the two
 * things chosen there, and the intro has to fit them between the title and the
 * track name without crowding either.
 */
char *endless_mode_text(void) {
    char *end;

    if (gEndlessTimeAttack) {
        end = endless_append_string(sEndlessModeText, "TIME ATTACK   SEED ");
    } else {
        end = endless_append_string(sEndlessModeText, "SURVIVAL   SEED ");
    }
    endless_append_number(end, gEndlessSeed);
    return sEndlessModeText;
}

/**
 * Names the head start on the round intro so it does not look like a glitch
 * when the player starts a race already holding bananas.
 */
char *endless_perk_text(void) {
    char *end = endless_append_string(sEndlessPerkText, "HEAD START  ");

    end = endless_append_number(end, gEndlessPerkBananas);
    endless_append_string(end, " BANANAS");
    return sEndlessPerkText;
}

s32 endless_mirrored(void) {
    return gEndlessMirrorThisRace;
}

/**
 * Whether the in-race status line should read as safe. In Survival that is
 * holding the required position (1 = first); in Time Attack placement is
 * irrelevant, so it is whether the clock still has comfortable room.
 */
s32 endless_position_is_safe(s32 racePosition) {
    if (gEndlessTimeAttack) {
        return gEndlessClock > normalise_time(60 * ENDLESS_TICKS_PER_SECOND);
    }
    return (racePosition - 1) <= endless_required_position();
}

/**
 * Round-based AI behaviour table: start low and climb one table per round so
 * the run passes through genuinely different AI personalities. (The vanilla
 * trophy-race tables sit at 6-8 already, so basing the ramp on them would
 * saturate immediately.) The ramp continues past the bank's top table via
 * endless_scale_ai_table.
 */
s32 endless_ai_level(UNUSED s32 baseLevel) {
    s32 level = ENDLESS_AI_BASE_TABLE + gEndlessRound;

    if (level > ENDLESS_AI_TABLE_MAX) {
        level = ENDLESS_AI_TABLE_MAX;
    }
    return level;
}

/**
 * Unbounded escalation past the table bank: push the loaded table's
 * position-interpolated speed bonus and action chances up with each round.
 */
void endless_scale_ai_table(AIBehaviourTable *table) {
    s32 heat = gEndlessRound - ENDLESS_HEAT_START_ROUND;
    f32 bonus;
    s32 value;
    s32 i;

    if (heat <= 0) {
        return;
    }
    bonus = heat * ENDLESS_SPEED_PER_HEAT;
    if (bonus > ENDLESS_SPEED_BONUS_CAP) {
        bonus = ENDLESS_SPEED_BONUS_CAP;
    }
    table->unk0 += bonus;
    table->unk4 += bonus;
    for (i = 0; i < 4; i++) {
        value = table->percentages[i][AI_MIN] + (heat * ENDLESS_CHANCE_PER_HEAT);
        if (value > 100) {
            value = 100;
        }
        table->percentages[i][AI_MIN] = value;
        value = table->percentages[i][AI_MAX] + (heat * ENDLESS_CHANCE_PER_HEAT);
        if (value > 100) {
            value = 100;
        }
        table->percentages[i][AI_MAX] = value;
    }
}

char *endless_round_text(void) {
    char *end = endless_append_string(sEndlessRoundText, "ROUND ");

    endless_append_number(end, gEndlessRound + 1);
    return sEndlessRoundText;
}

/**
 * Total points held by human players.
 */
s32 endless_score(void) {
    Settings *settings = get_settings();
    s32 score = 0;
    s32 i;

    for (i = 0; i < get_number_of_active_players(); i++) {
        score += settings->racers[i].trophy_points;
    }
    return score;
}

char *endless_score_text(void) {
    char *end = endless_append_string(sEndlessScoreText, "SCORE ");

    endless_append_number(end, endless_score());
    return sEndlessScoreText;
}

s32 endless_best_rounds(void) {
    return (s32) ((get_eeprom_settings() >> ENDLESS_SAVE_ROUNDS_SHIFT) & ENDLESS_SAVE_ROUNDS_MASK);
}

s32 endless_best_score(void) {
    return (s32) ((get_eeprom_settings() >> ENDLESS_SAVE_SCORE_SHIFT) & ENDLESS_SAVE_SCORE_MASK);
}

/**
 * Store this run if it beat the saved one. Rounds are the headline record, so
 * a deeper run always wins; the score only breaks ties between equally deep
 * runs, which stops a high-scoring short run from masking a longer one.
 *
 * Called after every cleared round rather than only at the end, so a run still
 * counts if the console is reset or the player quits out mid-run.
 */
void endless_record_run(s32 roundsCleared) {
    s32 score = endless_score();
    s32 bestRounds = endless_best_rounds();
    s32 bestScore = endless_best_score();

    if (roundsCleared < bestRounds || (roundsCleared == bestRounds && score <= bestScore)) {
        return;
    }
    if (roundsCleared > (s32) ENDLESS_SAVE_ROUNDS_MASK) {
        roundsCleared = (s32) ENDLESS_SAVE_ROUNDS_MASK;
    }
    if (score > (s32) ENDLESS_SAVE_SCORE_MASK) {
        score = (s32) ENDLESS_SAVE_SCORE_MASK;
    }

    // The setter only ORs bits, so each field is cleared before being written.
    unset_eeprom_settings_value(ENDLESS_SAVE_ROUNDS_MASK << ENDLESS_SAVE_ROUNDS_SHIFT);
    unset_eeprom_settings_value(ENDLESS_SAVE_SCORE_MASK << ENDLESS_SAVE_SCORE_SHIFT);
    set_eeprom_settings_value(((u64) roundsCleared) << ENDLESS_SAVE_ROUNDS_SHIFT);
    set_eeprom_settings_value(((u64) score) << ENDLESS_SAVE_SCORE_SHIFT);
}

/**
 * The record to beat, for the first round's intro and the game over screen.
 */
char *endless_best_text(void) {
    char *end;

    if (endless_best_rounds() == 0) {
        endless_append_string(sEndlessBestText, "NO RECORD YET");
        return sEndlessBestText;
    }
    end = endless_append_string(sEndlessBestText, "BEST ");
    end = endless_append_number(end, endless_best_rounds());
    end = endless_append_string(end, " ROUNDS  ");
    end = endless_append_number(end, endless_best_score());
    endless_append_string(end, " PTS");
    return sEndlessBestText;
}

char *endless_goal_text(void) {
    char *end;

    if (endless_required_position() == 0) {
        endless_append_string(sEndlessGoalText, "FINISH 1ST");
    } else {
        end = endless_append_string(sEndlessGoalText, "FINISH TOP ");
        endless_append_number(end, endless_required_position() + 1);
    }
    return sEndlessGoalText;
}

/**
 * Compact one-line status for the in-race HUD: which round this is and the
 * position that has to be held to survive it.
 */
char *endless_hud_text(void) {
    char *end = endless_append_string(sEndlessHudText, "ROUND ");

    end = endless_append_number(end, gEndlessRound + 1);
    if (gEndlessTimeAttack) {
        end = endless_append_string(end, "  ");
        endless_append_string(end, endless_clock_text());
    } else if (endless_required_position() == 0) {
        endless_append_string(end, "  1ST");
    } else {
        end = endless_append_string(end, "  TOP ");
        endless_append_number(end, endless_required_position() + 1);
    }
    return sEndlessHudText;
}
