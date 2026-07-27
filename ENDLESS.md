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

Your round and what you need are on the HUD the whole race, turning red the
moment you drop out of it.

### Two ways to lose

Press **Z** on the first round's intro to switch; the choice locks once the run
is underway.

- **Survival** — meet the round's required finish position or the run ends.
  It tightens as you go: top 4 (rounds 1-3) → top 3 (4-6) → top 2 (7-9) →
  **1st only** (10+).
- **Time Attack** — placement never ends the run. Instead every race is settled
  against a three-minute run clock, refunded as a share of that race's own
  duration: winning buys 30% back, fourth costs 10%, trailing costs 25%. The
  run ends when the clock empties. The refund is a percentage rather than a
  fixed number of seconds so the rule reads the same on Ancient Lake and on
  Spaceport Alpha.

### Records and seeds

Points accumulate all run using the trophy scoring table (9/7/5/3/1). Your best
run is saved to the cartridge and shown on the first round's intro and on the
game over screen — rounds are the headline record, and score only breaks ties
between equally deep runs.

Every run has a four-digit **seed** that fully determines its track order and
mirror rolls, so a run can be handed to someone else to race. On the first
round's intro, **left/right** picks a digit (the one being edited is drawn in
yellow) and **up/down** turns it, so a seed someone reads out can be dialled in
directly.

The track updates as soon as a digit changes, but the scenery behind the text
does not: loading it stops the menu music, so reloading it per keystroke would
mean cutting the music every time a digit turns. Starting the race loads the
right track regardless — the backdrop is only decoration.

Adventure-mode trophy races are untouched and still play the vanilla four-round
format. T.T. is on the roster from the start.

## How it works

All of the mode's own logic lives in [`src/endless.c`](src/endless.c) /
[`src/endless.h`](src/endless.h). It reuses the existing Trophy Race state
machine rather than building a new one: `gTrophyRaceRound` is pinned at 0 so
the vanilla rankings screen always takes its "continue to the next round" path,
while the real (unbounded) round counter lives in `endless.c`.

Integration points are all marked with an `// ENDLESS` comment:

| Location | Hook |
| --- | --- |
| `menu.c` `menu_track_select_loop` | Trophy column starts a run (`endless_start`) |
| `menu.c` `trackmenu_assets` | Trophy column force-unlocked |
| `menu.c` `menu_trophy_race_round_init` | Draws the next track from the shuffle bag |
| `menu.c` `trophyround_render` | Title, mode, seed, record, goal/clock, round |
| `menu.c` `menu_trophy_race_round_loop` | Z switches mode, L/R walk the seed |
| `menu.c` `menu_trophy_race_rankings_init` | Settles the finished round; sets options |
| `menu.c` `rankings_render_order` | Round / score / clock / GAME OVER overlay |
| `menu.c` `menu_trophy_race_rankings_loop` | Continue to next round, or end the run |
| `menu.c` `get_filtered_cheats` | Mirrored tracks at high rounds |
| `menu.c` `is_tt_unlocked` | T.T. available from the start |
| `menu.c` `trophyround_adventure`, pause quit, file select | Clear mode state |
| `game.c` `aitable_init` | AI behaviour table ramp + post-load table scaling |
| `game_ui.c` `hud_render_general` | In-race status line |
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
schedule, the head start sizes, and the Time Attack clock and refunds.
`endless_required_position()` holds the Survival placement schedule.

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
- The EEPROM settings word has room to spare: bits 0-25 are the vanilla flags
  and `write_eeprom_settings` reserves 56-63 for its checksum, leaving 26-55
  free. The run record lives at 32-55.
- The vanilla rankings screen sets an option count of three while only filling
  two entries, so a stale pointer gets drawn as a third option.

## Not done yet

- The intro screen's rows are full. Anything else shown there will need the
  block re-laid-out again.
- No leaderboard or ghost sharing beyond the seed.
- Multiplayer is untested; the in-race status line is single-player only, since
  it is positioned in screen coordinates.
