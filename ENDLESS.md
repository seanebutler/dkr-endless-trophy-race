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
before any repeats. **The AI escalates every single round, without a ceiling.**
Rounds 1-8 climb the game's own behaviour tables from 2 to the bank's maximum —
eight hand-authored personalities. Past that, three things keep moving:

- **Speed.** An AI's pace comes from a "virtual banana" count the engine clamps
  at 20, twice the human cap of 10. The lead and tail AI are ramped separately
  toward that clamp, the tail faster, so the field closes up rather than
  stringing out into free positions.
- **Grip.** Vanilla hands every AI the *weakest* acceleration curve in the game
  while a human uses their own character's, which is why raising the banana
  count alone could never make the AI genuinely fast. That curve is now scaled
  up, compounding, every round. Terminal speed goes as its square root and
  nothing clamps it, so this is the axis with no ceiling.
- **Boosts.** The AI is stopped from throwing its boosts away (see below).

Every round from the first moves the AI's top speed by at least 1.7%, and that
floor holds for as long as a run lasts. It crosses a fully-bananaed Drumstick
around round 8, is a third faster by round 19, and does not stop.

**Mirrored tracks** arrive as a coin flip from round 9, and every race from
round 13.

A strong finish buys a **head start** in the next round: five bananas for a
win, three for second, two for third. Bananas raise top speed and are lost on
every hit, so it is a burst of speed the escalating AI immediately starts
taking back rather than an edge that snowballs.

**Gauntlet** adds two seeded rule layers on top of the base run. Every round is
assigned a vehicle, and every fourth race adds a rule from a twelve-card deck:
no weapons, no zippers, four wheel drive, no banana limit, big racers, small
racers, or one of the balloon rules — boost, shield, rockets, traps, magnets,
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

Every map carries **two** complete eight-coin layouts at different positions:
the one Adventure 1 uses, and the one Adventure 2 uses. Adventure 2 is mirror
mode, so its layout is the one authored against a mirrored racing line — a
mirrored round therefore uses that set, and an ordinary round the other. Using
the wrong one puts coins where a lap never goes.

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
to switch between **Gauntlet** and **Classic**, and **C-Up** to turn mirrored
tracks on or off. All three lock once the race starts, so the rules cannot
change halfway through a run.

Mirroring is on by default and shows as **FLIP** on the setup line; turning it
off simply removes the word. It is a genuine matter of taste rather than only
difficulty — some people would rather learn twenty courses than forty — so it
is a real setting with its own records, not an easier route into someone else's
board. Turning it off does **not** change which tracks a seed deals: the mirror
coin flip is still rolled and then discarded, so the course order is identical
either way and a seed stays comparable across the setting.

- **Survival** — meet the round's required finish position or the run ends.
  It tightens as you go: top 4 (rounds 1-3) → top 3 (4-6) → top 2 (7-12) →
  **1st only** (13+). That last step used to arrive at round 10, back when the
  AI plateaued at round 8 and the placement rule had to supply the pressure by
  itself. Now that the AI climbs for real, tightening that fast would stack two
  escalations on top of each other.
- **Season** — a finite, comparable score attack. **Ten races**, drawn from
  the shuffle bag, no track repeated. Nothing can eliminate you: a disastrous
  race costs points and nothing else, so two players who race the same seed
  both finish ten races and can compare a single number. The run ends after
  race ten and the receipt reads **SEASON COMPLETE** rather than GAME OVER.
  Maximum score is 90 — ten wins at nine points each.

  Ten rather than twenty because an hour is a length people actually sit down
  for, where two hours is a commitment. The trade is that a season is ten of
  the twenty tracks rather than all of them, which makes the *selection* part
  of what a seed decides and not merely the order — two seeds are now different
  seasons, not the same season shuffled.

  A season escalates on a schedule sized to its own length. The endless ladder
  climbs forever, so a season compresses it: race 10 lands on the rung an
  endless run reaches at round 13, which is why a season finale bites well above
  where its race number suggests. Mirrored tracks arrive as coin flips from race
  5 and every race from race 7 on the same proportional basis. Change
  `ENDLESS_SEASON_RACES` and every schedule follows it.
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
runs, the game saves a best depth **and score** for each of the twelve rules
categories: Survival, Time Attack or Season, each in Gauntlet or Classic, each
with mirroring on or off.
Deeper always wins; score breaks ties between equally deep runs.

That comparator needs no special case for a season: every completed season ties
at the same depth, so the score alone decides, which is exactly the score-attack
semantics. An abandoned season records the races it did clear and loses to any
completed one.

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

The title screen carries the hack's own wordmark under the Diddy Kong Racing
logo: **ENDLESS** at double size with a drop shadow and a slow ember pulse, and
**TROPHY RACE** fading in beneath it once the stamp lands.

There is no new artwork behind that. BIGFONT is the one font in the game with a
separate texture page per glyph, so the word is assembled at runtime from those
pages and handed to the same scaled-texture draw the DKR logo itself uses —
which is the only way to get lettering at a size other than BIGFONT's fixed 28
pixels, since the text renderer has no scale parameter anywhere on its path.

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
- Twelve record categories fit the same 40-byte block that eight did: the
  twelve depths and twelve scores consume exactly the `spare[12]` that version 1
  carried as padding. Widening the board cost no EEPROM at all, and that spare
  was never independently usable anyway since it sat inside this block's
  checksum. Version 1 boards are migrated rather than wiped — every run they
  hold was played mirrored, which is the new index's low bit.
- **The free EEPROM is 80 bytes, not 92, and both its offset and its size are
  load-bearing.** Retiring adventure freed 120 bytes; the records block took 40
  of them. The remaining 80 (byte offsets 40-119, blocks 5-14) are contiguous
  and allocatable. The other 12 unused bytes are `EndlessRecords.spare[12]` at
  offsets 28-39, which sit *inside* the live checksummed block and straddle a
  block boundary — they can only be spent by adding fields to `EndlessRecords`,
  never by starting a new struct there. Nothing else can write to the arena:
  every `osEepromWrite` call site is in `save_data.c`, and the two that could
  reach it are behind the gated adventure functions.

- **`BLOCK_SIZE` truncates silently, so a misaligned save struct fails without
  a word of warning.** It is `(x / sizeof(u64))` with no rounding and no assert.
  A struct starting at byte 68 resolves to block 8 = byte 64 and quietly
  overlaps its neighbour; a 44-byte struct resolves to 5 blocks and only ever
  persists its first 40 bytes, the rest reading back as stale RAM. Both the
  start offset and the size must be multiples of 8 — that is precisely why
  `EndlessRecords` carries `spare[12]`, rounding 28 up to 40. The libultra
  bounds check is `address > EEPROM_MAXBLOCKS`, an off-by-one that would let
  block 64 through, so it will not catch an overrun for you.

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
- **Mirror mode is a rendering flip, not a world transform.** `tracks.c` mirrors
  the track display list and `racer.c` negates the steering to match; object
  world positions are untouched. So objects do move with the track visually, but
  anything authored *for* a particular racing line still has to be chosen per
  layout — which is why the silver coins need the Adventure 2 set when mirrored.
- **The texrect combiner multiplies by the primitive colour, so it can only
  darken.** BIGFONT's letters are natively yellow over a blue outline, and no
  primitive colour can make them brighter or bluer than that — the title's
  colour cycle runs from deep ember up to the font's own yellow because that is
  the whole available range, not a preference. A zero primitive gives a true
  black silhouette, which is how the drop shadow is drawn.
- **A fully opaque alpha selects a different blend mode.** Passing 255 picks the
  opaque table, which ignores the alpha channel and would draw every glyph
  page's transparent padding as a solid block. The vanilla logo passes 254 for
  exactly this reason, and so does the wordmark.
- **Unloading a font NULLs its texture pointers and the next load returns
  different addresses.** Anything borrowing glyph pages must rebuild them on
  every entry to the screen; a build-once array renders correctly on the first
  visit and hands freed memory to the display list on the second.
- **Balloon colours do not match their effects by name.** From
  `obj_init_balloon`: blue is boost, red is missile, **green is the trap**
  (mines, oil slicks, bubbles) and **rainbow is the magnet**. Naming an event
  card after the colour rather than checking the mapping announced one rule and
  ran another for a release.
- **FUNFONT has no slash glyph.** A missing glyph is skipped without advancing
  the pen, so `"7/20"` renders as `720` — silently, with no gap to hint at it.
  Every progress string in this mode spells out `" OF "` for that reason.
- **AI racers are hardcoded to the game's weakest acceleration curve.**
  `update_AI_racer` always loads `ASSET_MISC_RACERACCELERATION_UNKNOWN0` (tail
  0.33) where a human loads their character's own (T.T. 0.43, Drumstick 0.41).
  Terminal speed is `sqrt(425 * tail * (1 + 0.025 * bananas))`, so a weak base
  cannot be rescued by bananas — this is why the AI felt slow no matter what
  the behaviour table said.
- **All three vehicles share one speed law**, so difficulty scales identically
  across them. Car (`update_car_velocity_ground`), hovercraft (`func_80046524`)
  and plane (`func_80049794`) all apply quadratic drag at 0.004 against a thrust
  of curve x banana-multiplier x 1.7, 1.7 and 1.8 respectively, giving
  `v = sqrt(425 * C * M)` for car and hovercraft and `sqrt(450 * C * M)` for the
  plane. The plane is therefore 2.9% faster outright — but that applies to every
  racer in the flight, so it cancels out of any AI-versus-human comparison. The
  `velocity *` drag branch in the hovercraft and plane code is a low-speed case
  that only fires below `v = 1` while coasting; it never applies while racing.
  The one real asymmetry is that the car's drag comes from
  `gSurfaceTractionTable` and varies by surface, where the other two hardcode
  0.004 — so car rounds vary more track to track.
- **An AI's speed is its `unk124`, which the engine substitutes for its banana
  count.** Giving an AI real bananas does nothing. It is clamped to 20 against
  the human's 10, and `unk4` is the *leading* AI's value while `unk0` is the
  *last-placed* one's, blended across the field by position — so `unk4` is the
  lever that decides whether the player can win, and in a solo race the blend
  never reaches `unk0`'s end at all.
- The behaviour table's action percentages are already 100 across tables 7-9,
  and `roll_percent_chance` is `rand_range(0, 99) < chance`, so 100 is a real
  "always" with no headroom. A per-round chance bump is clamped away every
  time.
- **At 100, `AI_EMPOWERED_BOOST` makes the AI *worse*.** A boosting AI lifts off
  the accelerator for the whole boost and coasts afterwards while its throttle
  bleeds off, so the tables that always fire it are throwing the boost away.

## Fixed

- **The AI stopped getting harder at round 8, and its ceiling was below a good
  human.** Table 9 was reached at round 8 and never changed again; the only
  thing left was a speed drip that needed round 40 to finish arriving. Even
  fully maxed, that AI was ~8% *slower* than a Drumstick at full bananas — so
  no endless run could ever become genuinely hard, and Survival leaned on its
  placement schedule to create pressure instead. Fixed in v0.7.0. Depths set
  before it were earned against an AI that stopped climbing and are not
  comparable; erase Game Pak Times if you want a clean board.

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
- **A standing record does not remember its own seed.** The intro prints the
  seed of the run in front of you, and the BEST line prints a score with no
  provenance, so the record you are chasing cannot be re-raced. This is a hole
  in Season rather than a missing extra, and the fix is free: `bestSeeds[6]` as
  `u16` replaces `spare[12]` in place, leaving the struct 40 bytes, its
  alignment, and its checksum coverage all unchanged.

## Ruled out

Verified against the code and rejected, so they are not re-proposed:

- **Suspending a run between races.** Needs about 40 bytes rather than the 16 it
  looks like, because the run's real state is larger than it appears:
  `racers[].character` is a derived copy that `init_racer_headers` overwrites
  from `gCharacterIdSlots`, and all eight racers' `trophy_points` are live —
  endless skips the vanilla per-round reset, so the standings table is built
  from every racer and a naive resume lands on a screen with seven AI racers on
  zero. Replaying the RNG must also step the round rather than just re-seed,
  since whether a pick consumes a draw depends on it. Beyond the cost, a resume
  makes save-scumming unfixable while records are written after every round —
  against the point of a comparable mode. Ten-race seasons run about an hour,
  which is what this was for.
- **Per-track best boards.** A season visits 10 of the 20 bag tracks, so half
  the board is unreachable in any given season, and a best-per-track number over
  a difficulty-ramped draw order measures *when* you drew the track rather than
  the track: race 1 is always the weakest AI table, never mirrored, never an
  event. The optimal way to fill such a board is to win race 1 and take a new
  seed, which rewards quitting out of runs.
- **Anything dated.** There is no real-time clock, so "daily seed" and any
  date-stamped record cannot exist on this hardware.
- Interface strings added by the hack are English-only until they move into the
  localized menu asset pipeline.
