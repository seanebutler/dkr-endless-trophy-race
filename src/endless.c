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

#define ENDLESS_MAX_POOL 32

/************ .data ************/

s32 gEndlessActive = FALSE;
s32 gEndlessRound = 0;
s32 gEndlessTrackId = -1;
s32 gEndlessTrackWorld = 1;
s32 gEndlessMirrorThisRace = FALSE;

/*******************************/

/************ .bss ************/

static s8 sEndlessPool[ENDLESS_MAX_POOL];
static s8 sEndlessPoolWorld[ENDLESS_MAX_POOL];
static s32 sEndlessPoolSize;
static s32 sEndlessPoolCursor;
static char sEndlessRoundText[16];
static char sEndlessScoreText[24];
static char sEndlessGoalText[24];
static s32 sEndlessLastTrack;

/******************************/

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
 * Build the pool of candidate tracks from the tracks-menu level table:
 * the first four entries of each of the four main worlds, filtered to
 * standard races only.
 */
static void endless_build_pool(void) {
    s8 *levelIds = (s8 *) get_misc_asset(ASSET_MISC_TRACKS_MENU_IDS);
    s32 world;
    s32 slot;
    s32 id;

    sEndlessPoolSize = 0;
    for (world = 1; world <= 4; world++) {
        for (slot = 0; slot < 4; slot++) {
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
        j = rand_range(0, i);
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
    sEndlessLastTrack = -1;
    endless_build_pool();
    endless_shuffle_pool();
    for (i = 0; i < 8; i++) {
        settings->racers[i].trophy_points = 0;
    }
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
        gEndlessMirrorThisRace = rand_range(0, 1);
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
 * TRUE if the best-placed human met this round's required position.
 * starting_position holds the finish position of the last race.
 */
s32 endless_player_survived(void) {
    Settings *settings = get_settings();
    s32 best = 99;
    s32 i;

    for (i = 0; i < get_number_of_active_players(); i++) {
        if (settings->racers[i].starting_position < best) {
            best = settings->racers[i].starting_position;
        }
    }
    return best <= endless_required_position();
}

s32 endless_mirrored(void) {
    return gEndlessMirrorThisRace;
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
 * Total points held by human players, shown on the rankings screen.
 */
char *endless_score_text(void) {
    Settings *settings = get_settings();
    s32 score = 0;
    s32 i;
    char *end;

    for (i = 0; i < get_number_of_active_players(); i++) {
        score += settings->racers[i].trophy_points;
    }
    end = endless_append_string(sEndlessScoreText, "SCORE ");
    endless_append_number(end, score);
    return sEndlessScoreText;
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
