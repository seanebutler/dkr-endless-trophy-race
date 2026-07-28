# Endless Trophy Race

An arcade-style survival mode for Diddy Kong Racing, built on the
[DKR decompilation](https://github.com/DavidSM64/Diddy-Kong-Racing).
Inspired by *Super Metroid Arcade: Endless Mode* — an infinite run of races
that never stops getting harder, ending the first time you fall short.

## Getting it

Grab `EndlessTrophyRace.bps` from the
[Releases page](https://github.com/seanebutler/dkr-endless-trophy-race/releases)
and apply it to your own copy of the retail **US 1.0** ROM (SHA1
`0cb115d8716dbbc2922fda38e533b9fe63bb9670`). Any BPS patcher works —
[RomPatcher.js](https://www.marcrobledo.com/RomPatcher.js/) runs in a browser
and needs no install.

The ROM is not distributed here and never will be: the patch is this hack's own
changes, and the game data has to come from your own cartridge dump.

## Playing it

Choose a character and you are in — the run's setup screen opens straight from
character select. **B** there backs out to ordinary Tracks racing.

Every round draws a random track from a shuffle bag covering all 20 races
across all five worlds, Future Fun Land included; each track appears once
before any repeats. **The AI escalates every single round** — its behaviour
table climbs from table 2 to the bank's maximum, one step per round, and past
that the table itself is scaled: speed rises +0.125 "virtual bananas" per round
(capped at +4.0, near human parity so skill still decides races) and
boost/item/weapon aggression rises 2% per round. **Mirrored tracks** arrive as
a coin flip from round 9, and every race from round 13.

A strong finish buys a **head start** in the next round: five bananas for a
win, three for second, two for third. Bananas raise top speed and are lost on
every hit, so it is a burst of speed the escalating AI immediately starts
taking back rather than an edge that snowballs.

**Gauntlet** adds two seeded rule layers on top of the base run. Every round is
assigned a vehicle, and every fourth race adds a rule from a twelve-card deck:
no weapons, no zippers, four wheel drive, no banana limit, big racers, small
racers, or one of the balloon rules — boost, shield, rockets, magnets, rainbow,
or maximum power.

Every card maps to a magic code the game already implements, and each one
displaces its whole family for that race, the way the codes menu treats them,
so an event never stacks with a code the player set into a combination neither
chose. Two obvious candidates are deliberately missing: `CHEAT_HIGH_SPEED_RACING`
is declared but read nowhere in the ROM, so it would announce a rule and change
nothing, and the banana-suppressing codes would quietly void a head start or
coin bounty the player had just earned.

The vehicle is drawn from the game's own per-track vehicle mask — the same data
the track select menu greys its icons with — so an illegal pairing is not
possible to express: a track that cannot be flown never offers the plane in the
first place. It also means an ordinary track can become a genuinely different
race, since a hovercraft round on a car circuit takes different lines.

Both layers use their own stateless hashes rather than the track-order RNG, so
neither consumes it. **Classic** turns both off and keeps the exact track and
mirror sequence that the same seed produced in v0.2.

Gauntlet also switches on the **silver coin bounty**. The eight coins a track
already carries for its adventure challenge appear on the course, and the
in-race tally comes with them. Collecting the set is worth a ten-banana head
start next round in Survival, or twenty seconds of clock in Time Attack —
enough that a full set can be what keeps a Time Attack run alive.

Coins are strictly optional and never a loss condition: ignoring them costs
nothing, so each mode keeps exactly one way to end a run. What they add is a
second racing line worth weighing against position, since the detour to a coin
is usually the slow way round.

The bounty only runs when the gauntlet's vehicle draw matches the track's own
default — always, on single-vehicle tracks. The coins are placed along the
route that vehicle takes, so flying the same course can leave some of them
somewhere a plane never passes, and an objective that cannot be completed is
worse than no objective. The tally appearing at the start of a race is the
tell that this round has a bounty.

These share one switch to keep runs comparable: one bit of rules is easy to
say out loud with a seed, and four record categories stay legible on screen.

Your round and what you need are on the HUD the whole race, turning red the
moment you drop out of it, with a digital speed readout above them. Both sit
centred along the bottom, in the gap between the weapon icon and the minimap.
Speed is solo only — the split-screen viewports have no room to spare.

### Three modes

Press **Z** on the first round's intro to cycle modes. Press **C-Down** there
to switch between **Gauntlet** and **Classic**. Both choices lock once the race
starts, so the rules cannot change halfway through a run.

- **Survival** — meet the round's required finish position or the run ends.
  It tightens as you go: top 4 (rounds 1-3) → top 3 (4-6) → top 2 (7-9) →
  **1st only** (10+).
- **Season** — a finite, comparable score attack. Exactly twenty races, one
  full shuffle bag, every track once. Nothing can eliminate you: a disastrous
  race costs points and nothing else, so two players who race the same seed
  both finish twenty races and can compare a single number. The run ends after
  race twenty and the receipt reads **SEASON COMPLETE** rather than GAME OVER.
  Maximum score is 180 — twenty wins at nine points each.

  A season escalates on its own schedule. The endless ramp is built for a run
  with no end and spends little of its range inside twenty races: the AI
  behaviour table saturates at race 8, and the speed bonus needs race 40 to
  reach its cap. A season instead scales that bonus so the final race lands
  exactly on the cap, which turns a flat back two-thirds into a real arc.
- **Time Attack** — placement never ends the run. Instead every race is settled
  against a three-minute run clock, refunded as a share of that race's own
  duration: winning buys 30% back, fourth costs 10%, trailing costs 25%. The
  run ends when the clock empties. The refund is a percentage rather than a
  fixed number of seconds so the rule reads the same on Ancient Lake and on
  Spaceport Alpha.

### Multiplayer co-op

Multiplayer Tracks mode treats two to four humans as a team. The best human
finish decides whether Survival continues; in Time Attack, that same racer's
finish and race time determine the clock adjustment. A head start earned by
any player is granted to everyone on the next grid, trophy points are combined
on the receipt, and every viewport shows the shared **TEAM** status. The
quarter-screen layouts use a shorter version of the same line.

Co-op runs are deliberately unranked: a team result is not comparable with a
solo one, and letting an easier team run overwrite a solo best would make the
saved records meaningless.

### Records and seeds

Points accumulate all run using the trophy scoring table (9/7/5/3/1). For solo
runs, the game saves a best depth **and score** for each of the six rules
categories: Survival, Time Attack or Season, each in Gauntlet or Classic.
Deeper always wins; score breaks ties between equally deep runs.

That comparator needs no special case for a season: every completed season ties
at the same depth of twenty, so the score alone decides, which is exactly the
score-attack semantics. An abandoned season records the races it did clear and
loses to any completed one.

One honest limit of the scoring table in a mode with no elimination: positions
6th through 8th all award zero, so the very bottom of a season's range is flat.
It separates competitive runs cleanly and stops distinguishing bad ones. Records live in a
checksummed block in the retired adventure save region (caps 255 rounds and
65535 points) and are cleared by erasing Game Pak Times. Records from older
versions are migrated in automatically the first time this build runs.

The game-over screen is a run receipt with the seed and rules, cleared rounds,
full score, final placement, remaining Time Attack clock, and a new-best flag.
From there, **Retry Seed** reopens round one with the same seed and rules,
**New Seed** starts a guaranteed-different one, and **Quit** returns to track
select. The reopened setup can still be edited before pressing A.

Every run has a four-digit **seed** that fully determines its track order,
mirror rolls, and event rules when events are enabled, so a run can be handed
to someone else to race. On the first round's intro, **left/right** picks a
digit (the one being edited is drawn in yellow) and **up/down** turns it, so a
seed someone reads out can be dialled in directly.

The screen rebuilds around the new seed about half a second after you stop
turning digits — the track, the scenery behind it and the music all refresh
together. That pause is deliberate: swapping the backdrop means unloading the
previous level, which stops the menu music (that is why the code that opens
this screen replays it straight after loading a track). Waiting for you to
settle keeps a burst of edits to one interruption instead of one per keystroke.

Adventure mode is retired outright: this hack is the endless mode, and its
save slots now hold the endless records. Everything adventure used to gate is
simply open — the full track grid, the battle arenas, the mirror option, and
the complete roster including T.T. and Drumstick. Backing out of the run setup
with **B** lands on ordinary Tracks racing, the one other thing left to play.

## How it works

All of the mode's own logic lives in [`src/endless.c`](src/endless.c) /
[`src/endless.h`](src/endless.h). It reuses the existing Trophy Race state
machine rather than building a new one: `gTrophyRaceRound` is pinned at 0 so
cleared races can reuse the vanilla "continue to the next round" path. Failed
runs branch into the custom receipt and retry flow, while the real (unbounded)
round counter lives in `endless.c`.

Integration points are all marked with an `// ENDLESS` comment:

| Location | Hook |
| --- | --- |
| `menu.c` `menu_track_select_loop` | Trophy column starts a run (`endless_start`) |
| `menu.c` `trackmenu_assets` | Trophy column force-unlocked |
| `menu.c` `menu_trophy_race_round_init` | Draws the next track from the shuffle bag |
| `menu.c` `trophyround_render` | Title, mode, seed, record, goal/clock, round |
| `menu.c` `menu_trophy_race_round_loop` | Z switches mode, C-Down toggles events, stick edits the seed |
| `menu.c` `menu_trophy_race_rankings_init` | Settles the finished round; sets options |
| `menu.c` `rankings_render_order` | Live result overlay or final run receipt |
| `menu.c` `menu_trophy_race_rankings_loop` | Continue, retry seed, choose a new seed, or quit |
| `menu.c` `get_filtered_cheats` | Mirrored tracks and isolated event rules |
| `menu.c` `is_tt_unlocked` | T.T. available from the start |
| `menu.c` `trophyround_adventure`, pause quit, file select | Clear mode state |
| `game.c` `aitable_init` | AI behaviour table ramp + post-load table scaling |
| `game_ui.c` `hud_render_general` | Solo / multiplayer team status line |
| `objects.c` `track_setup_racers` | Applies the banana head start |

## Building

Needs a US 1.0 base ROM in `baseroms/` (SHA1
`0cb115d8716dbbc2922fda38e533b9fe63bb9670`) and the toolchain from the decomp's
own [README](README.md) — Linux, WSL2, or macOS.

```bash
make NON_MATCHING=1 -j$(nproc)
```

Output is `build/dkr.us.v77.z64`. `./make-patch.sh` turns that into a
distributable BPS patch, and refuses to emit one it cannot verify.

Two things worth knowing:

- **Always build `NON_MATCHING=1`.** The plain matching target no longer links,
  because `endless.o` isn't in the splat-generated linker script. That's
  expected for a code mod — the matching build exists to reproduce the retail
  ROM byte-for-byte, which a hack by definition doesn't.
- **`make clean` when switching between matching and non-matching builds.**
  They share a build directory, and stale matching objects carry Rare's
  anti-tamper checks into the hacked ROM, which booby-traps it (black screen or
  a permanently paused game).

## Tuning

Constants at the top of `src/endless.c` cover the AI ramp, the mirroring
schedule, the event cadence, the head start sizes, and the Time Attack clock
and refunds. `endless_required_position()` holds the Survival placement
schedule.

## Notes on the vanilla code

These cost real time to find, so they are written down rather than rediscovered:

- The big menu font (`ASSET_FONTS_BIGFONT`) has **no digit glyphs** — every
  number in this mode is drawn with `ASSET_FONTS_FUNFONT` for that reason.
- FUNFONT's glyphs carry their own colours, so tinting text needs the fourth
  argument of `set_text_colour` (how much the flat colour replaces the texture)
  to read at all.
- Lap counts are deliberately left alone. Raising a track above 3 laps overruns
  `Racer.lap_times` (a `u16[3]`) in the end-of-race copy in `objects.c`, which
  corrupts the adjacent racer's `trophy_points` — the very field this mode uses
  for scoring.
- The EEPROM is measured full: 512 of 512 bytes between three adventure saves,
  the settings word, and two time-trial record blocks. Retiring adventure is
  what created room — the endless records live in retired save slot A as a
  checksummed, versioned 40-byte block (rounds and scores for four rules
  categories, caps 255 and 65535, four more categories reserved). The
  adventure save IO is gated off so its checksum self-heal cannot "repair"
  the block back into a blank save, and older records in the settings word
  are migrated in once and the word's 30 bits freed.
- The vanilla rankings screen sets an option count of three while only filling
  two entries, so a stale pointer gets drawn as a third option.
- **FUNFONT has no slash glyph.** A missing glyph is skipped without advancing
  the pen, so `"7/20"` renders as `720` — silently, with no gap to hint at it.
  Every progress string in this mode spells out `" OF "` for that reason.
- `endless_ai_level` pins the behaviour table to its maximum from round 8, and
  that table's action chances are already 100, so `ENDLESS_CHANCE_PER_HEAT` is
  clamped away at every round of every run and has never changed a value. The
  speed bonus is the only escalation that actually does anything past round 8.

## Fixed

- Time Attack settled its clock **twice per race** from the silver coin bounty
  onwards: the bounty commit added a settle above the existing one without
  removing it, so every refund and penalty landed at double rate (a win bought
  back 60% of the race duration instead of 30%, a trailing finish cost 50%
  instead of 25%). Fixed in v0.6.0. Time Attack records set before that were
  earned under the doubled rates and are not comparable with new ones.

## Not done yet

- The intro screen has room again, but not much. It carries four rows of text
  between the title and the track name, spaced so they read as separate facts;
  a fifth would start crowding them back together.
- No leaderboard or ghost sharing beyond the seed and run receipt.
- Interface strings added by the hack are English-only until they move into the
  localized menu asset pipeline.
