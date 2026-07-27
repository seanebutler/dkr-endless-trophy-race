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

Pick **Tracks mode → the Trophy Race column** (always unlocked in this hack).

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

These share one switch because the save has room for four record categories,
not eight — see below.

Your round and what you need are on the HUD the whole race, turning red the
moment you drop out of it, with a digital speed readout above them. Both sit
centred along the bottom, in the gap between the weapon icon and the minimap.
Speed is solo only — the split-screen viewports have no room to spare.

### Two ways to lose

Press **Z** on the first round's intro to switch modes. Press **C-Down** there
to switch between **Gauntlet** and **Classic**. Both choices lock once the race
starts, so the rules cannot change halfway through a run.

- **Survival** — meet the round's required finish position or the run ends.
  It tightens as you go: top 4 (rounds 1-3) → top 3 (4-6) → top 2 (7-9) →
  **1st only** (10+).
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

Co-op runs are deliberately unranked. The Game Pak has room for the four solo
rules categories but not another four co-op records, and letting an easier team
run overwrite a solo best would make the saved depths incomparable.

### Records and seeds

Points accumulate all run using the trophy scoring table (9/7/5/3/1). For solo
runs, the game saves a separate best depth for each of the four rules
categories: Survival or Time Attack, each with Events On or Off. The compact
EEPROM fields top out at 127 cleared rounds and are cleared by erasing Game Pak
Times. A v0.2 save's shared record is migrated to Survival with Events Off
because the old save data did not identify which mode earned it.

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

Adventure-mode trophy races are untouched and still play the vanilla four-round
format. T.T. is on the roster from the start.

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
- The EEPROM settings word has exactly 30 spare data bits: bits 0-25 are the
  vanilla flags and `write_eeprom_settings` reserves 56-63 for its checksum.
  Bits 26-27 mark the v0.3 layout and 28-55 hold four 7-bit category records.
  There is not enough room for four persistent scores, so the receipt keeps the
  full score while the saved personal best is depth-only.
- The vanilla rankings screen sets an option count of three while only filling
  two entries, so a stale pointer gets drawn as a third option.

## Not done yet

- The intro screen has room again, but not much. It carries four rows of text
  between the title and the track name, spaced so they read as separate facts;
  a fifth would start crowding them back together.
- No leaderboard or ghost sharing beyond the seed and run receipt.
- Interface strings added by the hack are English-only until they move into the
  localized menu asset pipeline.
