# Mantis Seadiver: soloed stems

Every sound on its own, rendered through the real firmware's audio code at 24 kHz with its own reverb and echo,
in a place where it really happens. Everything else was playing but silenced, so timing and tails are true.
`stems_spectrograms.png` shows each one (0 to 9 kHz, low at the bottom). Re-render: `tools/host/stems.sh`.

To change a sound: its level and pitch are the `LVL_*` / `PITCH_*` constants at the top of `src/ocean.cpp`
(1 = as designed, 0.5 pitch = an octave down). The score's voices are in `src/music.cpp`.

The Core2's speaker can't reproduce much below ~200 Hz; the motor carries the low end on the device, so the
drone, kick, bass, thunder and eruption are fuller here than from the unit.

| # | Stem | What it is, and when you hear it | Tune with |
|---|---|---|---|
| 01 | sea_breath | the ocean's slow breathing swell: always there, darker with depth | LVL_SEA |
| 02 | surface_wash_storm | waves overhead, louder the shallower you are, much louder in a storm (here: 2 m, storm) | LVL_WASH, PITCH_WASH |
| 03 | rain_on_the_surface | rain heard through the surface: a muffled hiss | LVL_RAIN, PITCH_RAIN |
| 04 | thunder_far_above | a storm overhead, muffled by the water | LVL_THUNDER |
| 05 | vent_rumble | black smokers: rumble and hiss, louder the closer you are | LVL_VENT, PITCH_VENT |
| 06 | volcano_eruption | Fire Beneath, every 40 to 90 s: a rumble, then a column of big bubbles | LVL_ERUPT, PITCH_BUBBLES |
| 07 | crystal_and_ice_chimes | Amethyst Deep and Under the Ice | LVL_CHIME, PITCH_CHIME |
| 08 | abyss_groans_and_pings | the deep: distant groans, pings from nowhere | LVL_EERIE, PITCH_EERIE |
| 09 | screw_stop_cruise_flank | the screw: cruise 7 s, flank (cavitation) 8 s, all stop | LVL_ENGINE, PITCH_ENGINE |
| 10 | bubbles_diving_and_climbing | bubbles off the hull when you pitch, at speed, near vents | LVL_BUBBLES, PITCH_BUBBLES, BUBBLE_RATE |
| 11 | trim_tanks_venting_B | press B: a rush of air and big glugs | LVL_TRIM, PITCH_BUBBLES |
| 12 | hull_creaks_at_depth | past 100 m, now and then | LVL_CREAK, PITCH_CREAK |
| 13 | hull_bump_soft_to_hard | hitting rock or the seabed: soft, medium, hard | LVL_BUMP, PITCH_BUMP |
| 14 | sonar_ping_and_echo | press A: the ping, and its echo off the seabed ahead | LVL_SONAR, PITCH_SONAR |
| 15 | scanner_logged_then_new_entry | tap to scan: ticks, "already logged"; ticks, "new entry" | LVL_SCAN, PITCH_SCAN |
| 16 | ui_lights_card_cruise_detent | lights on, off; a card; C to cruise; the throttle detent (x2) | LVL_UI, PITCH_UI |
| 17 | dive_begins_hatch_and_tanks | power on: the hatch, the tanks flooding | PITCH_BUBBLES |
| 18 | snapping_shrimp_reef_night | the reef's sizzle, densest at night near the bottom | LVL_SHRIMP |
| 19 | humpback_song | a whale passing (near, then far) | LVL_WHALE, PITCH_WHALE |
| 20 | dolphin_whistles_and_clicks | dolphins in the shallows | LVL_DOLPHIN, PITCH_DOLPHIN |
| 21 | narwhal_clicks_and_buzz | narwhals under the ice | LVL_NARWHAL, PITCH_NARWHAL |
| 22 | weddell_seal_trills | under the ice: the falling trill | LVL_SEAL, PITCH_SEAL |
| 23 | leviathan_growl | Leviathan Hall, when the serpent is near | LVL_SERPENT |
| 24 | creature_darting_away | something bolting from a close pass | PITCH_BUBBLES |
| 25-33 | music_* | the score's voices (drone, pad, dub stab, shaker, kick, bass, plucks, bells, choir) | `src/music.cpp` |

25 to 31 are forced: the conductor normally brings music in as occasional passages of one or two voices. For
the stems it holds a passage with every voice at full energy.
