#include "endless.h"

#include "asset_enums.h"
#include "game.h"
#include "macros.h"
#include "math_util.h"
#include "menu.h"
#include "objects.h"
#include "save_layout.h"
#include "structs.h"
#include "thread3_main.h"
#include "types.h"

/**
 * @file Endless Trophy Race mode logic.
 *
 * Holds the mode state and all tuning; menu.c and game.c call into here from
 * small hooks in the existing Trophy Race flow. gTrophyRaceRound is pinned to
 * 0 for the whole run so a cleared race can reuse the vanilla "continue to
 * next round" path; failures branch into the Endless receipt and retry flow.
 * The real, unbounded round counter lives here.
 */

// Rounds are 0-based internally; shown to the player as round + 1.
#define ENDLESS_AI_BASE_TABLE 2        // Behaviour table for round 1; +1 per round up to 9.
#define ENDLESS_AI_TABLE_MAX 9         // Top of the AI behaviour table bank.
// Escalation past the table bank. Three separate things were capping the AI
// long before it got hard, and only the third has real headroom:
//
//   1. The table bank runs out at 9, reached at round 8. Rounds 1-8 still climb
//      it -- that part is hand-authored and genuinely varied, so it stays.
//   2. The action percentages are already 100 across tables 7-9, so the old
//      per-round chance bump was clamped away every time and never once changed
//      a value. Deleted rather than left looking load-bearing.
//   3. An AI's speed comes from its "virtual banana" count, which the engine
//      clamps to 20 -- twice the human cap of 10. That sounds generous until
//      you notice the AI is also hardcoded to the WEAKEST acceleration curve in
//      the game while a human uses their character's own. Multiplying a weak
//      base by a big number still loses: even a fully clamped AI is slower than
//      a ten-banana Drumstick. That is why the ceiling never felt like one.
//
// So the bananas are ramped to their clamp AND the AI is moved off the weak
// curve. Terminal speed goes as the square root of the curve's tail and nothing
// clamps it, which makes the curve the one axis that climbs without bound.
#define ENDLESS_AI_RAMP_START_ROUND 6  // 0-based; the banana ramp bites from displayed round 8.
#define ENDLESS_AI_LEAD_PER_ROUND 0.90f  // unk4 -- the leading AI, the one that decides a win.
#define ENDLESS_AI_PACK_PER_ROUND 1.20f  // unk0 -- the tail; faster, so the field closes up.
#define ENDLESS_AI_LEAD_CAP 21.36f       // Puts the lead AI exactly on the engine's +/-20 clamp.
#define ENDLESS_AI_PACK_CAP 20.34f       // Same for the tail, which carries the racing-line bonus.
#define ENDLESS_AI_PACK_GAP 0.50f        // The tail never catches the lead; keeps some field spread.
// Compounding, not linear: speed goes as sqrt(gain), so a linear gain would
// shrink every round's step forever (+1.2% at round 21 decaying to +0.2% by
// round 200). Compounding holds a constant ~1.7% per round for as long as the
// run lasts, which is what "climbs without bound" has to mean to be felt.
#define ENDLESS_AI_GRIP_PER_ROUND 0.035f
// Not a difficulty ceiling -- the AI is unbeatable decades before here. It
// stops the compounding running away into the engine's velocity clamp of 50,
// where the physics would stop behaving like racing.
#define ENDLESS_AI_GRIP_MAX_ROUNDS 60
#define ENDLESS_SEASON_LADDER_TOP 12   // Ladder round a season's finale lands on.
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
// Every race charges the run clock this share of its own duration, and placement
// refunds against that charge. Without the charge the refunds were pure income:
// first and second grew the clock, third left it alone, and a run could not end
// for anyone who kept podiuming -- a clock that started at 3:00 was routinely
// past 5:00 by round four. Set so that winning is the break-even-plus-a-little
// finish, which is what makes early wins bank time for the rounds where the AI
// has outgrown you.
#define ENDLESS_TA_PCT_COST 25
#define ENDLESS_TA_PCT_FIRST 30
#define ENDLESS_TA_PCT_SECOND 10
#define ENDLESS_TA_PCT_THIRD 0
#define ENDLESS_TA_PCT_FOURTH (-10)
#define ENDLESS_TA_PCT_REST (-25)

// Season: a finite score attack drawn from the shuffle bag, where nothing can
// eliminate the player and the total score is the record. Ten races is about an
// hour, which is a length someone will actually sit down for; a full twenty-race
// bag ran to two. The cost is that a season is now ten of the twenty tracks
// rather than all of them -- so which tracks you get is part of what the seed
// decides, not just their order.
#define ENDLESS_SEASON_RACES 10
#define ENDLESS_SEASON_WIN_POINTS 9 // gTrophyRacePointsArray's first-place award.

// Seeds are four digits so they can be read out loud and typed back in.
#define ENDLESS_SEED_MAX 9999
#define ENDLESS_SEED_DIGITS 4
// Frames of stillness before the intro rebuilds around a changed seed. Any
// rebuild reloads the backdrop, and unloading a level stops the menu music, so
// this is what keeps a burst of digit turns to one interruption instead of one
// per keystroke.
#define ENDLESS_SEED_REFRESH_DELAY 30

// Seeded event rounds use existing, well-tested magic-code rules. They are
// deliberately selected from a stateless seed/round hash instead of the track
// RNG, so enabling events cannot change a seed's track or mirror sequence.
#define ENDLESS_EVENT_INTERVAL 4
// Car, hovercraft and plane -- the three a player can normally pick between.
#define ENDLESS_VEHICLE_CHOICES 3

// Silver coin bounty. The eight coins a track already carries for its adventure
// challenge are switched on as an optional objective: collecting the set buys a
// reward, ignoring them costs nothing. Deliberately not a loss condition, so
// each mode keeps exactly one way to end a run.
#define ENDLESS_BOUNTY_COINS 8
#define ENDLESS_BOUNTY_BANANAS 10      // The human banana cap, so this is the biggest head start available.
#define ENDLESS_BOUNTY_TA_SECONDS 20   // Time Attack pays in clock instead.

// Every card maps to a magic code the game already implements and tests. Two
// obvious candidates are deliberately absent: CHEAT_HIGH_SPEED_RACING is
// declared but read nowhere in the ROM, so it would announce a rule and change
// nothing, and the banana-suppressing codes would quietly void a head start or
// a coin bounty the player had just earned.
// Balloon colours do not map to their effects by name, and getting this wrong
// announces one rule while running another. From obj_init_balloon
// (object_functions.c): blue is boost, red is missile, GREEN IS TRAP, yellow is
// shield, and RAINBOW IS THE MAGNET -- not green, and not merely "rainbow".
typedef enum EndlessEvent {
    ENDLESS_EVENT_NONE,
    ENDLESS_EVENT_NO_WEAPONS,
    ENDLESS_EVENT_NO_ZIPPERS,
    ENDLESS_EVENT_BOOST_BALLOONS,
    ENDLESS_EVENT_SHIELD_BALLOONS,
    ENDLESS_EVENT_MAX_POWER,
    ENDLESS_EVENT_ROCKETS,
    ENDLESS_EVENT_TRAPS,
    ENDLESS_EVENT_MAGNETS,
    ENDLESS_EVENT_BANANA_HOARD,
    ENDLESS_EVENT_FOUR_WHEEL_DRIVE,
    ENDLESS_EVENT_BIG_RACERS,
    ENDLESS_EVENT_SMALL_RACERS,
    ENDLESS_EVENT_COUNT
} EndlessEvent;

// Migration-era layouts in the EEPROM settings word. Records now live in the
// EndlessRecords block (save_layout.h) inside the retired adventure save slot;
// these constants only describe where older builds kept them, so their data
// can be absorbed once and the word's 30 bits freed. v0.3/v0.4: a two-bit
// marker plus four 7-bit depths. v0.2: one shared depth and score.
#define ENDLESS_SAVE_LAYOUT_MARKER_SHIFT 26
#define ENDLESS_SAVE_LAYOUT_MARKER_MASK ((u64) 0x3)
#define ENDLESS_SAVE_LAYOUT_MARKER_VALUE ((u64) 0x3)
#define ENDLESS_SAVE_RECORD_BASE_SHIFT 28
#define ENDLESS_SAVE_RECORD_BITS 7
#define ENDLESS_SAVE_RECORD_MASK ((u64) 0x7F) // 127 rounds per category.
#define ENDLESS_SAVE_LAYOUT_MASK (((u64) 0x3FFFFFFF) << ENDLESS_SAVE_LAYOUT_MARKER_SHIFT)
#define ENDLESS_LEGACY_ROUNDS_SHIFT 32
#define ENDLESS_LEGACY_ROUNDS_MASK ((u64) 0x3FF)
#define ENDLESS_LEGACY_SCORE_SHIFT 42
#define ENDLESS_LEGACY_SCORE_MASK ((u64) 0x3FFF)

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
s32 gEndlessMode = ENDLESS_MODE_SURVIVAL; // Kept between runs: it is a preference.
s32 gEndlessEventsEnabled = TRUE; // Also a preference; OFF is classic v0.2 play.
s32 gEndlessMirrorEnabled = TRUE; // Preference too; ON is how every run played before v0.7.2.
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
static char sEndlessGoalText[56];
static char sEndlessHudText[56];
static char sEndlessBestText[32];
static char sEndlessPerkText[32];
static char sEndlessClockText[24];
static char sEndlessSeasonText[24];
static char sEndlessSeedText[16];
static char sEndlessSpeedText[16];
static char sEndlessModeText[56];
static char sEndlessResultText[40];
static char sEndlessResultDetailText[40];
static char sEndlessSeedPrefix[40];
static char sEndlessSeedDigitText[4];
static s32 sEndlessLastTrack;
static s32 sEndlessSeedDigit;
static s32 sEndlessSeedRefreshTimer;
static s32 sEndlessResultScore;
static s32 sEndlessResultRounds;
static s32 sEndlessResultPosition;
static s32 sEndlessNewBest;
static s32 sEndlessBountyClaimed;
static u32 sEndlessRngState;
static EndlessRecords sEndlessRecords;

/******************************/

static void endless_ensure_records(void);

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
 * Stateless random value reserved for event selection. Keeping it entirely
 * separate from sEndlessRngState is what lets EVENTS OFF reproduce v0.2 track
 * and mirror sequences exactly.
 */
static u32 endless_event_hash(void) {
    u32 value = (u32) gEndlessSeed ^ (((u32) gEndlessRound + 1U) * 0x9E3779B9U) ^ 0xE71E5EEDU;

    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static EndlessEvent endless_event(void) {
    if (!gEndlessEventsEnabled || ((gEndlessRound + 1) % ENDLESS_EVENT_INTERVAL) != 0) {
        return ENDLESS_EVENT_NONE;
    }
    return (EndlessEvent) (ENDLESS_EVENT_NO_WEAPONS + (endless_event_hash() % (ENDLESS_EVENT_COUNT - 1)));
}

/**
 * The vehicle stream, kept separate from both the track RNG and the event hash
 * so that a track's assigned vehicle is stable no matter what else changes.
 */
static u32 endless_vehicle_hash(void) {
    u32 value = (u32) gEndlessSeed ^ (((u32) gEndlessRound + 1U) * 0x85EBCA6BU) ^ 0x2545F491U;

    value ^= value >> 15;
    value *= 0x2C1B3C6DU;
    value ^= value >> 12;
    value *= 0x297A2D39U;
    value ^= value >> 15;
    return value;
}

/**
 * Which vehicle this round is raced in. With the gauntlet off this is simply
 * the track's own default, so classic runs are untouched.
 *
 * The candidates come from the game's per-track vehicle mask -- the same data
 * the track select menu greys its icons with -- rather than a hand-written
 * compatibility table. That is what makes an illegal pairing impossible: a
 * track that cannot be flown never offers the plane in the first place.
 */
s32 endless_vehicle(void) {
    s32 usable;
    s32 choices[ENDLESS_VEHICLE_CHOICES];
    s32 count = 0;
    s32 i;

    if (gEndlessTrackId < 0) {
        return VEHICLE_CAR;
    }
    if (!gEndlessEventsEnabled) {
        return leveltable_vehicle_default(gEndlessTrackId);
    }
    usable = leveltable_vehicle_usable(gEndlessTrackId);
    // Only the three the player can normally choose between; the mask has room
    // for special vehicles that no ordinary race should hand out.
    for (i = VEHICLE_CAR; i <= VEHICLE_PLANE; i++) {
        if (usable & (1 << i)) {
            choices[count++] = i;
        }
    }
    if (count == 0) {
        return leveltable_vehicle_default(gEndlessTrackId);
    }
    return choices[endless_vehicle_hash() % (u32) count];
}

/**
 * Names the vehicle on the round intro. Only worth saying when the gauntlet
 * picked it; otherwise it is just the track's usual vehicle.
 */
char *endless_vehicle_text(void) {
    if (!gEndlessEventsEnabled) {
        return "";
    }
    switch (endless_vehicle()) {
        case VEHICLE_HOVERCRAFT:
            return "HOVERCRAFT";
        case VEHICLE_PLANE:
            return "PLANE";
        default:
            return "CAR";
    }
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

/**
 * How many races a season is. Clamped to the bag: asking for more races than
 * there are tracks would reshuffle mid-season and repeat one, which would break
 * the promise that a season never shows the same track twice.
 */
static s32 endless_season_length(void) {
    if (sEndlessPoolSize <= 0) {
        endless_build_pool();
    }
    if (ENDLESS_SEASON_RACES < sEndlessPoolSize) {
        return ENDLESS_SEASON_RACES;
    }
    return sEndlessPoolSize;
}

/**
 * Which rung of the difficulty ladder this race sits on. Endless climbs it one
 * rung per round forever; a season compresses the whole ladder into its own
 * length so a ten-race season still passes through the same arc and finishes
 * on ENDLESS_SEASON_LADDER_TOP. Same idiom as the mirror schedule.
 */
static s32 endless_ai_ladder_round(void) {
    s32 length;

    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        length = endless_season_length();
        if (length <= 1) {
            return 0;
        }
        return (gEndlessRound * ENDLESS_SEASON_LADDER_TOP) / (length - 1);
    }
    return gEndlessRound;
}

/**
 * The AI's acceleration curve, scaled up as the run climbs.
 *
 * Vanilla hands every AI racer ASSET_MISC_RACERACCELERATION_UNKNOWN0 -- tail
 * 0.33, the weakest curve in the game -- while a human racer loads their own
 * character's (T.T. 0.43, Drumstick 0.41). Terminal speed is proportional to
 * the square root of the tail and nothing clamps it, unlike the banana count,
 * so this is the only axis that can escalate indefinitely.
 *
 * Gain 1.0 reproduces the vanilla asset byte for byte, so round 1 is untouched.
 * The result is memoised on the gain rather than the round, because this is
 * called from the per-frame racer update.
 */
static const f32 sEndlessAiAccelBase[16] = {
    0.10f, 0.12f, 0.15f, 0.17f, 0.19f, 0.21f, 0.25f, 0.28f,
    0.32f, 0.33f, 0.33f, 0.33f, 0.33f, 0.33f, 0.33f, 0.00f,
};
static f32 sEndlessAiAccel[16];
static f32 sEndlessAiAccelGain = 0.0f;

f32 *endless_ai_accel_curve(void) {
    s32 n = endless_ai_ladder_round();
    f32 gain = 1.0f;
    s32 i;

    if (n > ENDLESS_AI_GRIP_MAX_ROUNDS) {
        n = ENDLESS_AI_GRIP_MAX_ROUNDS;
    }
    for (i = 0; i < n; i++) {
        gain *= 1.0f + ENDLESS_AI_GRIP_PER_ROUND;
    }
    if (gain != sEndlessAiAccelGain) {
        for (i = 0; i < 15; i++) {
            sEndlessAiAccel[i] = sEndlessAiAccelBase[i] * gain;
        }
        // The final entry terminates the curve; scaling it would change shape.
        sEndlessAiAccel[15] = 0.0f;
        sEndlessAiAccelGain = gain;
    }
    return sEndlessAiAccel;
}

/**
 * When this race mirrors. The endless schedule is absolute -- a coin flip from
 * round 9, always from round 13 -- which a short season would end before ever
 * reaching. A season scales those points to its own length instead, so it still
 * passes through clean, uncertain and mirrored phases. At twenty races this
 * reproduces the endless schedule exactly.
 */
static void endless_mirror_schedule(s32 *chanceFrom, s32 *alwaysFrom) {
    s32 length;

    if (gEndlessMode != ENDLESS_MODE_SEASON) {
        *chanceFrom = ENDLESS_MIRROR_CHANCE_ROUND;
        *alwaysFrom = ENDLESS_MIRROR_ALWAYS_ROUND;
        return;
    }
    length = endless_season_length();
    *chanceFrom = (length * ENDLESS_MIRROR_CHANCE_ROUND) / ENDLESS_MAX_POOL;
    *alwaysFrom = (length * ENDLESS_MIRROR_ALWAYS_ROUND) / ENDLESS_MAX_POOL;
}

/**
 * "RACE 7 OF 20". Spelled out because FUNFONT has no slash glyph: a missing
 * glyph is skipped without advancing, so "7/20" would silently render "720".
 */
static char *endless_append_season_progress(char *dst) {
    char *end = endless_append_string(dst, "RACE ");

    end = endless_append_number(end, gEndlessRound + 1);
    end = endless_append_string(end, " OF ");
    return endless_append_number(end, endless_season_length());
}

/**
 * Public "RACE 7 OF 20" for the screens outside this file.
 */
char *endless_season_text(void) {
    endless_append_season_progress(sEndlessSeasonText);
    return sEndlessSeasonText;
}

static void endless_begin_with_seed(s32 seed) {
    Settings *settings;
    s32 i;

    endless_ensure_records();
    // Results overwrite starting_position and per-race timing fields. Starting
    // a new attempt must restore the same clean grid as entering Tracks mode,
    // while init_racer_headers preserves the selected character slots.
    init_racer_headers();
    settings = get_settings();
    gEndlessActive = TRUE;
    gEndlessRound = 0;
    gEndlessTrackId = -1;
    gEndlessTrackWorld = 1;
    gEndlessMirrorThisRace = FALSE;
    gEndlessPerkBananas = 0;
    gEndlessClock = normalise_time(ENDLESS_TA_START_SECONDS * ENDLESS_TICKS_PER_SECOND);
    sEndlessSeedDigit = 0;
    sEndlessSeedRefreshTimer = 0;
    sEndlessResultScore = 0;
    sEndlessResultRounds = 0;
    sEndlessResultPosition = 0;
    sEndlessNewBest = FALSE;
    sEndlessBountyClaimed = FALSE;
    for (i = 0; i < 8; i++) {
        settings->racers[i].trophy_points = 0;
    }
    endless_set_seed(seed);
}

void endless_start(void) {
    // A fresh run gets a random seed; the first intro still lets the player
    // replace it before starting.
    endless_begin_with_seed(rand_range(0, ENDLESS_SEED_MAX));
}

void endless_retry_seed(void) {
    s32 seed = gEndlessSeed;

    endless_begin_with_seed(seed);
}

void endless_new_seed(void) {
    s32 oldSeed = gEndlessSeed;
    s32 seed = rand_range(0, ENDLESS_SEED_MAX);

    // NEW SEED should never silently hand back the run the player just left.
    if (seed == oldSeed) {
        seed++;
        if (seed > ENDLESS_SEED_MAX) {
            seed = 0;
        }
    }
    endless_begin_with_seed(seed);
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
    s32 mirrorChanceFrom;
    s32 mirrorAlwaysFrom;

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

    endless_mirror_schedule(&mirrorChanceFrom, &mirrorAlwaysFrom);
    if (gEndlessRound >= mirrorAlwaysFrom) {
        gEndlessMirrorThisRace = TRUE;
    } else if (gEndlessRound >= mirrorChanceFrom) {
        gEndlessMirrorThisRace = endless_rng_range(1);
    } else {
        gEndlessMirrorThisRace = FALSE;
    }
    // The coin flip above is rolled and then discarded when mirroring is off,
    // rather than skipped. Skipping it would leave the track RNG one draw
    // ahead, so the same seed would deal a different course order with
    // mirroring disabled -- and comparing a seed across the setting is the
    // whole reason someone turns it off.
    if (!gEndlessMirrorEnabled) {
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
 * Top 4 early, tightening to 1st place only from round 13 on.
 *
 * That used to be round 10. The schedule was doing the escalation's job while
 * the AI plateaued at round 8; now that the AI actually climbs, tightening this
 * fast would stack two difficulty curves on top of each other.
 */
s32 endless_required_position(void) {
    if (gEndlessRound >= 12) {
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
 * Settings index of the best-placed human in the race just run.
 * starting_position holds the finish position of the last race.
 */
static s32 endless_best_human_index(void) {
    Settings *settings = get_settings();
    s32 bestIndex = 0;
    s32 best = 99;
    s32 i;

    for (i = 0; i < get_number_of_active_players(); i++) {
        if (settings->racers[i].starting_position < best) {
            best = settings->racers[i].starting_position;
            bestIndex = i;
        }
    }
    return bestIndex;
}

/**
 * Finishing position of the best-placed human, 0 = first. In co-op this is the
 * shared team result: either player can keep the run alive.
 */
static s32 endless_best_finish(void) {
    Settings *settings = get_settings();

    return settings->racers[endless_best_human_index()].starting_position;
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

/**
 * Decimal weight of a seed digit, counting from the left.
 */
static s32 endless_digit_place(s32 index) {
    switch (index) {
        case 0:
            return 1000;
        case 1:
            return 100;
        case 2:
            return 10;
        default:
            return 1;
    }
}

void endless_seed_move_digit(s32 delta) {
    sEndlessSeedDigit += delta;
    while (sEndlessSeedDigit < 0) {
        sEndlessSeedDigit += ENDLESS_SEED_DIGITS;
    }
    while (sEndlessSeedDigit >= ENDLESS_SEED_DIGITS) {
        sEndlessSeedDigit -= ENDLESS_SEED_DIGITS;
    }
}

/**
 * Turn the selected digit, leaving the other three alone, so any seed can be
 * dialled in directly instead of counted up to.
 */
void endless_seed_change_digit(s32 delta) {
    s32 place = endless_digit_place(sEndlessSeedDigit);
    s32 digit = (gEndlessSeed / place) % 10;
    s32 wanted = digit + delta;

    while (wanted < 0) {
        wanted += 10;
    }
    while (wanted > 9) {
        wanted -= 10;
    }
    endless_set_seed(gEndlessSeed + ((wanted - digit) * place));
}

/**
 * Note that the seed changed and the intro needs rebuilding around it, once the
 * player stops turning digits.
 */
void endless_seed_mark_dirty(void) {
    sEndlessSeedRefreshTimer = ENDLESS_SEED_REFRESH_DELAY;
}

void endless_seed_clear_dirty(void) {
    sEndlessSeedRefreshTimer = 0;
}

s32 endless_seed_refresh_pending(void) {
    return sEndlessSeedRefreshTimer > 0;
}

/**
 * TRUE on the single frame the wait expires.
 */
s32 endless_seed_refresh_due(s32 updateRate) {
    if (sEndlessSeedRefreshTimer <= 0) {
        return FALSE;
    }
    sEndlessSeedRefreshTimer -= updateRate;
    if (sEndlessSeedRefreshTimer <= 0) {
        sEndlessSeedRefreshTimer = 0;
        return TRUE;
    }
    return FALSE;
}

/**
 * Whether this round is running the silver coin bounty. Tied to the gauntlet so
 * that Classic keeps racing exactly the track v0.2 raced, with nothing extra on
 * it, and restricted to ordinary races because that is where the coins live.
 */
s32 endless_bounty_active(void) {
    if (!gEndlessActive || !gEndlessEventsEnabled) {
        return FALSE;
    }
    if (gEndlessTrackId < 0 || leveltable_type(gEndlessTrackId) != RACETYPE_DEFAULT) {
        return FALSE;
    }
    // The eight coins are placed along the route the track's own vehicle takes.
    // Fly the same course and some of them sit somewhere a plane never passes,
    // which would put an objective on screen that cannot be completed. So the
    // bounty only runs when the gauntlet's draw happens to match the vehicle
    // the coins were laid out for -- always, on single-vehicle tracks.
    return endless_vehicle() == leveltable_vehicle_default(gEndlessTrackId);
}

/**
 * Record how many coins a human finished with. Called as the results are
 * written, while the racer objects still exist -- by the time the rankings
 * screen opens they are gone. Any human completing the set claims it, so co-op
 * can split the work.
 */
void endless_note_coins(s32 coins) {
    if (gEndlessActive && coins >= ENDLESS_BOUNTY_COINS) {
        sEndlessBountyClaimed = TRUE;
    }
}

/**
 * Digital speed readout for the in-race HUD, in the same units the vanilla
 * speedometer needle sweeps, so a full gauge and a full number agree.
 */
char *endless_speed_text(s32 speed) {
    char *end = endless_append_string(sEndlessSpeedText, "SPEED ");

    endless_append_number(end, speed);
    return sEndlessSpeedText;
}

s32 endless_bounty_claimed(void) {
    return sEndlessBountyClaimed;
}

s32 endless_time_attack(void) {
    return gEndlessMode == ENDLESS_MODE_TIME_ATTACK;
}

s32 endless_season(void) {
    return gEndlessMode == ENDLESS_MODE_SEASON;
}

/**
 * Survival -> Time Attack -> Season -> Survival. The clock is refilled on every
 * step rather than only on the way into Time Attack, so a mode cycled past and
 * come back to is still a full clock. A season needs no reset of its own: its
 * progress is gEndlessRound and its length is the bag, both rebuilt per run.
 */
void endless_cycle_mode(void) {
    gEndlessMode++;
    if (gEndlessMode > ENDLESS_MODE_SEASON) {
        gEndlessMode = ENDLESS_MODE_SURVIVAL;
    }
    gEndlessClock = normalise_time(ENDLESS_TA_START_SECONDS * ENDLESS_TICKS_PER_SECOND);
}

s32 endless_events_enabled(void) {
    return gEndlessEventsEnabled;
}

s32 endless_mirror_enabled(void) {
    return gEndlessMirrorEnabled;
}

void endless_toggle_mirror(void) {
    gEndlessMirrorEnabled = !gEndlessMirrorEnabled;
}

void endless_toggle_events(void) {
    gEndlessEventsEnabled = !gEndlessEventsEnabled;
}

s32 endless_event_active(void) {
    return endless_event() != ENDLESS_EVENT_NONE;
}

s32 endless_event_cheats(void) {
    switch (endless_event()) {
        case ENDLESS_EVENT_NO_WEAPONS:
            return CHEAT_DISABLE_WEAPONS;
        case ENDLESS_EVENT_NO_ZIPPERS:
            return CHEAT_TURN_OFF_ZIPPERS;
        case ENDLESS_EVENT_BOOST_BALLOONS:
            return CHEAT_ALL_BALLOONS_ARE_BLUE;
        case ENDLESS_EVENT_SHIELD_BALLOONS:
            return CHEAT_ALL_BALLOONS_ARE_YELLOW;
        case ENDLESS_EVENT_MAX_POWER:
            return CHEAT_MAXIMUM_POWER_UP;
        case ENDLESS_EVENT_ROCKETS:
            return CHEAT_ALL_BALLOONS_ARE_RED;
        case ENDLESS_EVENT_TRAPS:
            return CHEAT_ALL_BALLOONS_ARE_GREEN;
        case ENDLESS_EVENT_MAGNETS:
            return CHEAT_ALL_BALLOONS_ARE_RAINBOW;
        case ENDLESS_EVENT_BANANA_HOARD:
            return CHEAT_NO_LIMIT_TO_BANANAS;
        case ENDLESS_EVENT_FOUR_WHEEL_DRIVE:
            return CHEAT_FOUR_WHEEL_DRIVER;
        case ENDLESS_EVENT_BIG_RACERS:
            return CHEAT_BIG_CHARACTERS;
        case ENDLESS_EVENT_SMALL_RACERS:
            return CHEAT_SMALL_CHARACTERS;
        default:
            return 0;
    }
}

char *endless_event_text(void) {
    switch (endless_event()) {
        case ENDLESS_EVENT_NO_WEAPONS:
            return "EVENT  NO WEAPONS";
        case ENDLESS_EVENT_NO_ZIPPERS:
            return "EVENT  NO ZIPPERS";
        case ENDLESS_EVENT_BOOST_BALLOONS:
            return "EVENT  BOOST BALLOONS";
        case ENDLESS_EVENT_SHIELD_BALLOONS:
            return "EVENT  SHIELD BALLOONS";
        case ENDLESS_EVENT_MAX_POWER:
            return "EVENT  MAX POWER";
        case ENDLESS_EVENT_ROCKETS:
            return "EVENT  ROCKETS ONLY";
        case ENDLESS_EVENT_TRAPS:
            return "EVENT  TRAPS ONLY";
        case ENDLESS_EVENT_MAGNETS:
            return "EVENT  MAGNETS ONLY";
        case ENDLESS_EVENT_BANANA_HOARD:
            return "EVENT  NO BANANA LIMIT";
        case ENDLESS_EVENT_FOUR_WHEEL_DRIVE:
            return "EVENT  FOUR WHEEL DRIVE";
        case ENDLESS_EVENT_BIG_RACERS:
            return "EVENT  BIG RACERS";
        case ENDLESS_EVENT_SMALL_RACERS:
            return "EVENT  SMALL RACERS";
        default:
            return "";
    }
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
    s32 bestHuman = endless_best_human_index();
    s32 raceTime = settings->racers[bestHuman].course_time;
    s32 percent;

    // The placement and duration must come from the same human. Otherwise a
    // P2 win could be settled using P1's slower finishing time.
    switch (settings->racers[bestHuman].starting_position) {
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
    // Charge the race, then refund by placement. A win nets +5% of the race's
    // duration, second -15%, third -25%, and it falls away from there.
    gEndlessClock += (raceTime * (percent - ENDLESS_TA_PCT_COST)) / 100;
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
    // A season ends by running out of races, not by failing one. Answering the
    // bag question here is what keeps every existing caller correct without
    // being re-pointed: for a season "the run is over" and "there is no next
    // round" are the same fact, which is only untrue for modes that end early.
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        return (gEndlessRound + 1) < endless_season_length();
    }
    if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
        return gEndlessClock > 0;
    }
    return endless_player_survived();
}

/**
 * Whether the run ended badly, as opposed to merely ending. Only two things
 * need to tell those apart: the depth written to the save, and whether the
 * receipt reads GAME OVER or SEASON COMPLETE.
 */
s32 endless_run_failed(void) {
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        return FALSE;
    }
    return !endless_run_continues();
}

/**
 * Settle everything the finished round owes: the run clock, the save record,
 * and the head start for next time. Called once, as the rankings screen opens,
 * so that screen and the exit branch agree on the outcome.
 */
void endless_round_finished(s32 roundPoints) {
    s32 survived;

    // The bounty is paid before the clock is judged, so a full coin set can be
    // what keeps a Time Attack run alive rather than a consolation for losing.
    if (sEndlessBountyClaimed && gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
        gEndlessClock += normalise_time(ENDLESS_BOUNTY_TA_SECONDS * ENDLESS_TICKS_PER_SECOND);
    }
    if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
        endless_settle_clock();
    }
    survived = endless_run_continues();
    // Depth counts races cleared, so it keys off failing rather than off
    // continuing: a completed season stops without failing and has cleared its
    // last race, where a lost run has not cleared the one that ended it.
    sEndlessResultRounds = endless_run_failed() ? gEndlessRound : gEndlessRound + 1;
    sEndlessResultScore = endless_score() + roundPoints;
    sEndlessResultPosition = endless_best_finish() + 1;
    if (endless_record_run(sEndlessResultRounds, sEndlessResultScore)) {
        sEndlessNewBest = TRUE;
    }
    if (survived) {
        endless_award_perk();
        // Survival pays the bounty as the largest head start there is, which
        // beats anything placement alone can earn.
        if (sEndlessBountyClaimed && gEndlessMode != ENDLESS_MODE_TIME_ATTACK) {
            gEndlessPerkBananas = ENDLESS_BOUNTY_BANANAS;
        }
    }
    sEndlessBountyClaimed = FALSE;
}

char *endless_clock_text(void) {
    s32 seconds = gEndlessClock / normalise_time(ENDLESS_TICKS_PER_SECOND);
    // "LEFT" rather than "TIME": this is what remains of the whole run's budget,
    // and sitting next to a round number the word TIME read as a target for this
    // one race. Same width as TIME, so no line grew.
    char *end = endless_append_string(sEndlessClockText, "LEFT ");

    end = endless_append_number(end, seconds / 60);
    *end++ = ':';
    // Seconds are always two digits, or "2:5" would read as two minutes five.
    *end++ = (char) ('0' + ((seconds % 60) / 10));
    *end++ = (char) ('0' + ((seconds % 60) % 10));
    *end = '\0';
    return sEndlessClockText;
}

/**
 * The run's rules on one line: what ends it, whether the gauntlet is on, and
 * whether tracks mirror. The seed used to share this line, which is what forced
 * every word to be abbreviated -- spelled out with the seed present, the
 * worst combination measured 319px against a 320px screen. The seed now has its
 * own row directly below, which also makes the cursor arithmetic trivial.
 */
char *endless_mode_text(void) {
    char *end;

    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_string(sEndlessModeText, "SEASON   ");
    } else if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
        end = endless_append_string(sEndlessModeText, "TIME ATTACK   ");
    } else {
        end = endless_append_string(sEndlessModeText, "SURVIVAL   ");
    }
    // Naming both states beats "EVENTS ON/OFF": the switch now decides vehicles
    // as well as event rounds, and it is shorter than spelling either out.
    end = endless_append_string(end, gEndlessEventsEnabled ? "GAUNTLET" : "CLASSIC");
    // Named as a property of the run when on and simply absent when off, the
    // same way the vehicle is only named when the gauntlet chose it.
    if (gEndlessMirrorEnabled) {
        endless_append_string(end, "   MIRRORED");
    }
    return sEndlessModeText;
}

/**
 * The seed on its own row. Leading zeroes are kept so the digits never shift
 * under the cursor.
 */
char *endless_seed_text(void) {
    char *end = endless_append_string(sEndlessSeedText, "SEED ");
    s32 digit;
    s32 i;

    for (i = 0; i < ENDLESS_SEED_DIGITS; i++) {
        digit = (gEndlessSeed / endless_digit_place(i)) % 10;
        *end++ = (char) ('0' + digit);
    }
    *end = '\0';
    return sEndlessSeedText;
}

/**
 * The seed row truncated just before the digit being edited, and that digit on
 * its own. Measuring these two gives the digit's position within the centred
 * row, which is how the cursor gets drawn: there is no glyph for it, because
 * the only bracket-like characters in this font live on a page that is not
 * loaded here and come out blank.
 */
char *endless_seed_prefix_text(void) {
    char *full = endless_seed_text();
    s32 length = 0;
    s32 i;

    while (full[length] != '\0') {
        length++;
    }
    // The seed is the last ENDLESS_SEED_DIGITS characters of the line, so
    // dropping the digits at and after the cursor leaves the prefix.
    length -= ENDLESS_SEED_DIGITS - sEndlessSeedDigit;
    if (length < 0) {
        length = 0;
    }
    for (i = 0; i < length; i++) {
        sEndlessSeedPrefix[i] = full[i];
    }
    sEndlessSeedPrefix[i] = '\0';
    return sEndlessSeedPrefix;
}

char *endless_seed_digit_text(void) {
    s32 digit = (gEndlessSeed / endless_digit_place(sEndlessSeedDigit)) % 10;

    sEndlessSeedDigitText[0] = (char) ('0' + digit);
    sEndlessSeedDigitText[1] = '\0';
    return sEndlessSeedDigitText;
}

/**
 * Names the head start on the round intro so it does not look like a glitch
 * when the player starts a race already holding bananas.
 */
char *endless_perk_text(void) {
    char *end;

    // Naming the bounty payout is the only place the coins explain what they
    // were worth. Placement alone tops out well below the bounty's award, so
    // the size of the head start identifies where it came from.
    if (gEndlessPerkBananas >= ENDLESS_BOUNTY_BANANAS) {
        end = endless_append_string(sEndlessPerkText, "COIN BOUNTY  ");
    } else {
        end = endless_append_string(sEndlessPerkText, "HEAD START  ");
    }
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
    // A season has no loss condition, so nothing on its HUD should ever read as
    // danger; a mid-pack race costs points, which the score line already says.
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        return TRUE;
    }
    if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
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
    s32 level = ENDLESS_AI_BASE_TABLE + endless_ai_ladder_round();

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
    s32 n = endless_ai_ladder_round() - ENDLESS_AI_RAMP_START_ROUND;
    f32 lead;
    f32 pack;
    s32 i;

    if (n <= 0) {
        return;
    }
    // unk4 is the leading AI's speed and unk0 the last-placed one's, blended
    // across the field by position. The tail climbs faster on purpose: an equal
    // bonus would leave backmarkers as free positions while only the leader
    // mattered, and in a solo race the blend never reaches unk0's end anyway.
    lead = table->unk4 + ((f32) n * ENDLESS_AI_LEAD_PER_ROUND);
    pack = table->unk0 + ((f32) n * ENDLESS_AI_PACK_PER_ROUND);
    if (lead > ENDLESS_AI_LEAD_CAP) {
        lead = ENDLESS_AI_LEAD_CAP;
    }
    if (pack > ENDLESS_AI_PACK_CAP) {
        pack = ENDLESS_AI_PACK_CAP;
    }
    if (pack > lead - ENDLESS_AI_PACK_GAP) {
        pack = lead - ENDLESS_AI_PACK_GAP;
    }
    table->unk4 = lead;
    table->unk0 = pack;

    // Stop the AI throwing its boosts away. At 100 -- which every table from 7
    // up sets -- a boosting AI lifts off the accelerator for the whole boost
    // and coasts afterwards while its throttle bleeds off. All four slots are
    // cleared, not just the two the old code touched: the step slots would
    // otherwise walk it straight back up mid-race.
    for (i = 0; i < 4; i++) {
        table->percentages[AI_EMPOWERED_BOOST][i] = 0;
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

/**
 * mode * 2 + gauntlet. Survival and Time Attack keep indices 0-3 exactly where
 * they already were, so existing records survive untouched, and Season lands on
 * the slots 4 and 5 that the block reserved.
 */
static s32 endless_record_category(void) {
    s32 category = gEndlessMode * 4;

    if (gEndlessEventsEnabled) {
        category += 2;
    }
    if (gEndlessMirrorEnabled) {
        category++;
    }
    return category;
}

/**
 * The persistent board ranks solo attempts; a team result is not comparable
 * with one. Co-op still gets the complete end-of-run receipt.
 */
static s32 endless_records_enabled(void) {
    return get_number_of_active_players() == 1;
}

EndlessRecords *endless_records_pointer(void) {
    return &sEndlessRecords;
}

/**
 * Bring the records block up to date with anything older builds left in the
 * settings word: v0.3/v0.4 kept four 7-bit depths behind a marker there, and
 * v0.2 a single depth+score pair. Whatever is found is absorbed (taking the
 * larger where both exist) and the word's bits are freed for good -- the block
 * in the retired save slot has room the word never had.
 *
 * The block itself was read by the boot-time settings read; an invalid or
 * never-written block arrives here zeroed with version 0.
 */
static void endless_ensure_records(void) {
    u64 word = get_eeprom_settings();
    s32 marker = (s32) ((word >> ENDLESS_SAVE_LAYOUT_MARKER_SHIFT) & ENDLESS_SAVE_LAYOUT_MARKER_MASK);
    s32 changed = FALSE;
    s32 depth;
    s32 score;
    s32 i;

    if (sEndlessRecords.version != ENDLESS_RECORDS_VERSION) {
        sEndlessRecords.version = ENDLESS_RECORDS_VERSION;
        changed = TRUE;
    }
    if ((word & ENDLESS_SAVE_LAYOUT_MASK) != 0) {
        if (marker == (s32) ENDLESS_SAVE_LAYOUT_MARKER_VALUE) {
            // v0.3/v0.4 layout: four categorized depths, no scores.
            for (i = 0; i < 4; i++) {
                depth = (s32) ((word >> (ENDLESS_SAVE_RECORD_BASE_SHIFT + (i * ENDLESS_SAVE_RECORD_BITS))) &
                               ENDLESS_SAVE_RECORD_MASK);
                if (depth > sEndlessRecords.rounds[i]) {
                    sEndlessRecords.rounds[i] = depth;
                }
            }
        } else {
            // v0.2 layout: one shared depth+score; events and modes were not
            // recorded, so Survival/Classic is its only unambiguous home.
            depth = (s32) ((word >> ENDLESS_LEGACY_ROUNDS_SHIFT) & ENDLESS_LEGACY_ROUNDS_MASK);
            score = (s32) ((word >> ENDLESS_LEGACY_SCORE_SHIFT) & ENDLESS_LEGACY_SCORE_MASK);
            if (depth > 255) {
                depth = 255;
            }
            if (depth > sEndlessRecords.rounds[0]) {
                sEndlessRecords.rounds[0] = depth;
                sEndlessRecords.scores[0] = score;
            }
        }
        *get_eeprom_settings_pointer() = word & ~ENDLESS_SAVE_LAYOUT_MASK;
        changed = TRUE;
    }
    if (changed) {
        mark_write_eeprom_settings();
    }
}

s32 endless_best_rounds(void) {
    endless_ensure_records();
    return sEndlessRecords.rounds[endless_record_category()];
}

static s32 endless_best_score(void) {
    endless_ensure_records();
    return sEndlessRecords.scores[endless_record_category()];
}

/**
 * Store this run if it beats this rules category's saved one: deeper always
 * wins, and score breaks ties between equally deep runs. Scores fit again now
 * that records live in the reclaimed save slot rather than the settings word.
 *
 * Called after every cleared round rather than only at the end, so a run still
 * counts if the console is reset or the player quits out mid-run.
 */
s32 endless_record_run(s32 roundsCleared, s32 score) {
    s32 category;

    if (!endless_records_enabled()) {
        return FALSE;
    }
    endless_ensure_records();
    if (roundsCleared > 255) {
        roundsCleared = 255;
    }
    if (score > 65535) {
        score = 65535;
    }
    category = endless_record_category();
    if (roundsCleared <= 0 || roundsCleared < sEndlessRecords.rounds[category]) {
        return FALSE;
    }
    if (roundsCleared == sEndlessRecords.rounds[category] && score <= sEndlessRecords.scores[category]) {
        return FALSE;
    }
    sEndlessRecords.rounds[category] = roundsCleared;
    sEndlessRecords.scores[category] = score;
    mark_write_eeprom_settings();
    return TRUE;
}

/**
 * Treat Endless records as part of Game Pak TIMES so the existing erase option
 * really returns every record category to a clean state.
 */
void endless_clear_records(void) {
    s32 i;

    for (i = 0; i < ENDLESS_RECORDS_CATEGORIES; i++) {
        sEndlessRecords.rounds[i] = 0;
        sEndlessRecords.scores[i] = 0;
    }
    *get_eeprom_settings_pointer() = get_eeprom_settings() & ~ENDLESS_SAVE_LAYOUT_MASK;
    mark_write_eeprom_settings();
}

s32 endless_new_best(void) {
    return sEndlessNewBest;
}

/**
 * The record to beat, for the first round's intro and the game over screen.
 */
char *endless_best_text(void) {
    char *end;

    if (!endless_records_enabled()) {
        endless_append_string(sEndlessBestText, "CO-OP  UNRANKED");
        return sEndlessBestText;
    }
    if (endless_best_rounds() == 0) {
        endless_append_string(sEndlessBestText, "NO RECORD YET");
        return sEndlessBestText;
    }
    end = endless_append_string(sEndlessBestText, "BEST ");
    // Every completed season ties at the same depth, so the depth is not news
    // there and the score is the whole record.
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_number(end, endless_best_score());
        end = endless_append_string(end, " OF ");
        end = endless_append_number(end, endless_season_length() * ENDLESS_SEASON_WIN_POINTS);
        endless_append_string(end, " PTS");
        return sEndlessBestText;
    }
    end = endless_append_number(end, endless_best_rounds());
    end = endless_append_string(end, endless_best_rounds() == 1 ? " ROUND" : " ROUNDS");
    // Migrated v0.3 records carry no score; only show one that exists.
    if (endless_best_score() > 0) {
        end = endless_append_string(end, "  ");
        end = endless_append_number(end, endless_best_score());
        endless_append_string(end, " PTS");
    }
    return sEndlessBestText;
}

char *endless_result_text(void) {
    char *end;

    // A season's headline fact is the score against its fixed maximum; the
    // race count is constant for every completed season and moves to the
    // detail line, where it reports how much of the season was actually run.
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_string(sEndlessResultText, "SCORE ");
        end = endless_append_number(end, sEndlessResultScore);
        end = endless_append_string(end, " OF ");
        endless_append_number(end, endless_season_length() * ENDLESS_SEASON_WIN_POINTS);
        return sEndlessResultText;
    }
    end = endless_append_string(sEndlessResultText, "ROUNDS ");
    end = endless_append_number(end, sEndlessResultRounds);
    end = endless_append_string(end, "   SCORE ");
    endless_append_number(end, sEndlessResultScore);
    return sEndlessResultText;
}

char *endless_result_detail_text(void) {
    char *end = endless_append_string(sEndlessResultDetailText, "FINAL ");

    end = endless_append_number(end, sEndlessResultPosition);
    if (sEndlessResultPosition == 1) {
        end = endless_append_string(end, "ST");
    } else if (sEndlessResultPosition == 2) {
        end = endless_append_string(end, "ND");
    } else if (sEndlessResultPosition == 3) {
        end = endless_append_string(end, "RD");
    } else {
        end = endless_append_string(end, "TH");
    }
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_string(end, "   ");
        end = endless_append_number(end, sEndlessResultRounds);
        end = endless_append_string(end, " OF ");
        end = endless_append_number(end, endless_season_length());
        endless_append_string(end, " RACES");
    } else if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
        end = endless_append_string(end, "   ");
        endless_append_string(end, endless_clock_text());
    }
    return sEndlessResultDetailText;
}

/**
 * Round and what it takes to survive it, on one line for the round intro.
 * Separate from the in-race version because that one compacts itself to fit a
 * quarter-screen viewport, while the intro always has the whole screen and can
 * afford to spell the requirement out.
 */
char *endless_intro_status_text(void) {
    char *end;

    // A season counts down to a known end, so it says where it is in the season
    // rather than which round this is.
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_season_progress(sEndlessGoalText);
    } else {
        end = endless_append_string(sEndlessGoalText, "ROUND ");
        end = endless_append_number(end, gEndlessRound + 1);
    }
    // The gauntlet's vehicle belongs next to the round it applies to. With the
    // gauntlet off it is the track's usual vehicle and not worth a word.
    if (gEndlessEventsEnabled) {
        end = endless_append_string(end, "   ");
        end = endless_append_string(end, endless_vehicle_text());
    }
    end = endless_append_string(end, "    ");
    // A season has no requirement to state, so the slot that names the goal
    // carries the running total instead -- the only number that decides it.
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_string(end, "SCORE ");
        endless_append_number(end, endless_score());
    } else if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
        endless_append_string(end, endless_clock_text());
    } else if (endless_required_position() == 0) {
        endless_append_string(end, "FINISH 1ST");
    } else {
        end = endless_append_string(end, "FINISH TOP ");
        endless_append_number(end, endless_required_position() + 1);
    }
    return sEndlessGoalText;
}

/**
 * Compact one-line status for the in-race HUD: which round this is and the
 * position that has to be held to survive it.
 */
char *endless_hud_text(void) {
    char *end;
    s32 players = get_number_of_active_players();

    // Quarter-screen viewports need a compact form that still names the team
    // rule. Horizontal two-player and solo layouts keep the full wording.
    if (players > 2) {
        end = endless_append_string(sEndlessHudText, "R");
        end = endless_append_number(end, gEndlessRound + 1);
        end = endless_append_string(end, " TEAM ");
        // This branch exists because a quarter-screen viewport has no room, so
        // the season drops "OF 20" here and keeps only the number that changes.
        if (gEndlessMode == ENDLESS_MODE_SEASON) {
            endless_append_number(end, endless_score());
        } else if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
            endless_append_string(end, endless_clock_text() + 5); // Skip "LEFT ".
        } else if (endless_required_position() == 0) {
            endless_append_string(end, "1ST");
        } else {
            end = endless_append_string(end, "TOP ");
            endless_append_number(end, endless_required_position() + 1);
        }
        return sEndlessHudText;
    }

    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_season_progress(sEndlessHudText);
    } else {
        end = endless_append_string(sEndlessHudText, "ROUND ");
        end = endless_append_number(end, gEndlessRound + 1);
    }
    if (players > 1) {
        end = endless_append_string(end, "  TEAM");
    }
    if (gEndlessMode == ENDLESS_MODE_SEASON) {
        end = endless_append_string(end, "  SCORE ");
        endless_append_number(end, endless_score());
    } else if (gEndlessMode == ENDLESS_MODE_TIME_ATTACK) {
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
