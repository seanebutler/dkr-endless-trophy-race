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

- Every round draws a random track from a shuffle bag covering all four main
  worlds; each track appears once before any repeats.
- **Survive** by meeting the round's required finish position. It tightens as
  you go: top 4 (rounds 1-3) → top 3 (4-6) → top 2 (7-9) → **1st only** (10+).
  Miss it once and the run ends.
- **The AI escalates every single round.** The behaviour table climbs from
  table 2 to the bank's maximum (9), one step per round. Past that the loaded
  table itself is scaled: the AI's speed bonus rises +0.125 "virtual bananas"
  per round (capped at +4.0, near human parity so skill still decides races)
  and its boost/item/weapon aggression rises 2% per round.
- **Mirrored tracks** arrive as a coin flip from round 9, and every race from
  round 13.
- Points accumulate across the whole run using the trophy scoring table
  (9/7/5/3/1 for the top five). The rankings screen after each race shows the
  round, your running score, and whether you survived.

Adventure-mode trophy races are untouched and still play the vanilla four-round
format.

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
| `menu.c` `trophyround_render` | Round + goal text on the intro screen |
| `menu.c` `rankings_render_order` | Round / score / GAME OVER overlay |
| `menu.c` `menu_trophy_race_rankings_loop` | Survive → next round, miss → run over |
| `menu.c` `get_filtered_cheats` | Mirrored tracks at high rounds |
| `menu.c` `trophyround_adventure`, pause quit, file select | Clear mode state |
| `game.c` `aitable_init` | AI behaviour table ramp + post-load table scaling |

## Building

Needs a US 1.0 base ROM in `baseroms/` (SHA1
`0cb115d8716dbbc2922fda38e533b9fe63bb9670`) and the toolchain from the decomp's
own [README](README.md) — Linux, WSL2, or macOS.

```bash
make NON_MATCHING=1 -j$(nproc)
```

Output is `build/dkr.us.v77.z64`.

Two things worth knowing:

- **Always build `NON_MATCHING=1`.** The plain matching target no longer links,
  because `endless.o` isn't in the splat-generated linker script. That's
  expected for a code mod — the matching build exists to reproduce the retail
  ROM byte-for-byte, which a hack by definition doesn't.
- **`make clean` when switching between matching and non-matching builds.**
  They share a build directory, and stale matching objects carry Rare's
  anti-tamper checks into the hacked ROM, which booby-traps it (black screen or
  a permanently paused game).

Distribute as an xdelta/BPS patch against the vanilla ROM — never the ROM
itself. The decomp is CC0; the game is not.

## Tuning

Constants at the top of `src/endless.c`:

| Constant | Meaning |
| --- | --- |
| `ENDLESS_AI_BASE_TABLE` | Behaviour table used in round 1 |
| `ENDLESS_HEAT_START_ROUND` | When direct table scaling begins |
| `ENDLESS_SPEED_PER_HEAT` | AI speed bonus added per round past that |
| `ENDLESS_SPEED_BONUS_CAP` | Ceiling on that bonus (human banana cap is 10) |
| `ENDLESS_CHANCE_PER_HEAT` | AI action-chance percent added per round |
| `ENDLESS_MIRROR_CHANCE_ROUND` / `_ALWAYS_ROUND` | Mirroring schedule |

`endless_required_position()` holds the placement schedule.

## Notes on the vanilla code

- The big menu font (`ASSET_FONTS_BIGFONT`) has **no digit glyphs** — every
  number in this mode is drawn with `ASSET_FONTS_FUNFONT` for that reason.
- Lap counts are deliberately left alone. Raising a track above 3 laps
  overruns `Racer.lap_times` (a `u16[3]`) in the end-of-race copy in
  `objects.c`, which corrupts the adjacent racer's `trophy_points` — the very
  field this mode uses for scoring.
