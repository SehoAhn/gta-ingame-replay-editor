# Replay Editor for GTA Vice City (v1.0)

A CLEO mod that works like the GTA IV video editor. Open a replay saved with F2 inside the game,
re-shoot the camera, trim the start and end, and save the result as a project file.

## Installation

| File | Location |
|---|---|
| `ReplayEditor.cs` | `GTA VC\CLEO\` |
| `ReplayEditor.ini` | `GTA VC\CLEO\` |
| `ReplayEditor.dll` | `GTA VC\CLEO\ReplayEditor\` |

Requirements: GTA Vice City **1.0** executable, III.VC.CLEO (2.0 or later).
The mod writes a log to `CLEO\ReplayEditor\ReplayEditor.log`.

## Workflow

1. In game, press **F2** → the recent gameplay is saved as `replay.rep` (stock feature).
   To keep a different scene, rename the file (e.g. `replay2.rep`).
2. In game, press **F6** → menu
   - **OPEN IN REPLAY EDITOR**: pick a file to open in the editor.
     - `.rep` → start editing as a new, empty project
     - `.prj` → continue editing a saved project (keyframes, start/end marks)
   - **PLAY REPLAY**: pick a file to play.
     - `.rep` → plays with the camera recorded in the file
     - `.prj` → plays the source replay with the project (trim, camera, FOV) applied
   - The list shows only the `.rep` / `.prj` files in the user folder
     (`Documents\GTA Vice City User Files`, where F2 saves), newest first.
   - **DELETE PROJECT**: lists only the `.prj` files; pick one and confirm (Enter: delete, Backspace: cancel)
     to delete it from the user folder.
   - In every list, **F** finds files by name and **R** renames the selected file (see below).
3. After editing, save the project to the same folder
   - **F7 (Save)**: saves into the project you are editing (overwrites it).
     If you opened a `.rep` and have not saved yet, it creates a new project file (same as F8).
   - **F8 (Save As)**: always saves as a new project file named with the local date and time,
     e.g. `replay2.edit.2026NOV17-153045.prj` (year, month as JAN–DEC, day, then hour, minute, second);
     after that, F7 saves into this new file. Earlier files are kept (remove old ones with F6 → DELETE PROJECT).
   - The `.prj` contains the keyframes and start/end marks **plus the entire source replay data**,
     so the `.prj` alone can be opened and played even if the source `.rep` is deleted or overwritten by F2
     (this is why a `.prj` is about the same size as the source `.rep`).
   - The source `.rep` is never modified.
4. **F6** → exit the editor (asks once more if there are unsaved changes)

To watch the result, choose F6 → PLAY REPLAY and pick the `.prj` (trim, camera and FOV are applied).

## Menu controls

| Key | Action |
|---|---|
| ↑ ↓ / PgUp PgDn / Home End | Select |
| Enter | Open (DELETE PROJECT: delete after confirmation) |
| F | Find: type part of a file name; the list shows only the files whose name contains it (any letter case). Enter: done (the list stays filtered), Esc: clear |
| R | Rename the selected file (not in the SUB file list of the editor) |
| Backspace | Back (clears the find first, if there is one) |
| F6 | Close menu |

### Find / rename

- **F (find)**: the line under the list shows `FIND: text` and the number of matching files. ↑ ↓ / PgUp PgDn still
  select while typing; Enter keeps the filter so you can open, play, delete or rename a found file. F again edits
  the text, Backspace in the list (or Esc while typing) clears it. Also works in the SUB file list (N / K in the editor).
- **R (rename)**: a box shows the current name without its extension; edit it and press Enter (Esc: cancel).
  - The extension (`.rep` / `.prj`) stays the same. Letters, digits, space, `-`, `_` (Shift + `-`) and `.`
    can be typed; Shift or Caps Lock gives upper case. ← → / Home End move the cursor, Backspace / Delete erase.
  - An existing file is never overwritten (`A FILE WITH THIS NAME ALREADY EXISTS`); Windows device names
    (CON, NUL, COM1 …) are refused. Changing only the letter case is allowed.
  - A name with Korean (or other non-English) characters shows as `?` in the game font, so the box starts empty
    for such a file: type a new name.
  - Renaming a `.rep` does not affect projects made from it (a `.prj` contains the whole source replay).
    `replay.rep` can be renamed to keep it from being overwritten by the next F2.
- While the text box is open, the keys go only to the box: the game does not see them, and Esc does not
  open the pause menu. (Typed letters still count toward PC cheat codes, as in normal play.)

## Editor controls

| Group | Key | Action |
|---|---|---|
| Playback | Space | Play / pause (from the start mark if pressed at the end mark or the last frame; turns the free camera off) |
| | ← → | Step 1 frame (Shift: 1 second) |
| | Home / End | Go to the start / end mark |
| | 1 – 5 | Speed 0.25× / 0.5× / 1× / 2× / 4× |
| Camera | Tab | Free camera on/off (always starts from the camera currently on screen) |
| | W A S D / Q E | Move / down·up (Shift faster, Ctrl slower) |
| | Mouse | Look |
| | Z C / R | Roll / reset roll |
| | - = | Field of view wider / narrower |
| Keyframes | K | Add at the current frame (updates it if one exists) — same type as the previous keyframe |
| | Del | Delete the selected keyframe |
| | [ / ] | Move the selected keyframe one frame back / forward (all tracks; does nothing if that frame already has a key) |
| | PgUp / PgDn | Select the previous / next keyframe |
| | T | Change type: RECORDED CAMERA → FOLLOW PLAYER → LOOK AT PLAYER → FOLLOW & LOOK AT PLAYER → FREE |
| | B / Shift+B | Interpolation to the next keyframe, forward / backward: SMOOTH → LINEAR → EASE IN → EASE OUT → EASE IN-OUT → CONSTANT SPEED → CUT (see below) |
| | P | Preview from the start mark |
| Tracks | ↑ / ↓ | Select the track to edit: CAMERA / WEATHER / TIME (/ SUB 1 … SUB 9 for the sub replays added) — K, Del, PgUp/PgDn and B apply to the selected track |
| | N | Add a sub replay (up to 9): pick a `.rep` from the user folder; its track (SUB 1, SUB 2, …) appears and is selected |
| Weather track | K | Add a weather key with the current weather (updates it if one exists) |
| | T or , / . | Change the selected weather key (7 types: SUNNY, CLOUDY, RAINY, FOGGY, EXTRA SUNNY, HURRICANE, EXTRA COLOURS) |
| Time track | K | Add a time key with the current time (updates it if one exists) |
| | , / . | Selected time key −/+ 10 minutes (Shift: 1 hour) |
| SUB n track | K | Replace this sub replay with another `.rep` |
| | [ / ] | Move the sub replay one frame earlier / later (Shift: 1 second) |
| | Del (twice) | Remove this sub replay (the following ones move up: SUB 3 becomes SUB 2, …) |
| Trim | I / O | Set the current frame as the start / end mark (Shift: clear) |
| Save | F7 | Save: overwrite the project being edited (a new file if not saved yet) |
| | F8 | Save As: new project `<source>.edit.<YYYYMONDD-HHMMSS>.prj` |
| Other | H | Show / hide help (closed at start) |
| | F6 | Exit the editor |

### Interpolation (B)

Each keyframe sets how the camera moves from it to the next keyframe:

| Type | Movement to the next keyframe |
|---|---|
| SMOOTH | Smooth curve through the keyframes (default) |
| LINEAR | Straight line at a steady speed; changes direction sharply at the keyframes |
| EASE IN | Starts from a standstill and speeds up |
| EASE OUT | Slows down and stops at the next keyframe |
| EASE IN-OUT | Starts and stops at a standstill |
| CONSTANT SPEED | The SMOOTH curve at an even speed along the path (no speeding up / slowing down when keyframes are unevenly spaced) |
| CUT | Holds this keyframe, then jumps to the next one |

The easing types also apply to the direction, roll and field of view. On the CAMERA lane, segments other than SMOOTH have a
thin speed bar (bright = fast, dark = slow). Projects from older versions open unchanged (SMOOTH / CUT).

### Weather / time tracks

- The bottom timeline has three lanes — CAMERA · WEATHER · TIME — and the selected track name is orange.
- Weather: with LINEAR it changes gradually to the next key using the game's weather transition; with CUT it switches at the next key.
- Time: with LINEAR the clock runs until the next key's time (time-lapse, always forward — it can pass midnight, e.g. 23:00 → 02:00);
  with CUT it stays at that time until the next key.
- B / Shift+B on these tracks: LINEAR → EASE IN → EASE OUT → EASE IN-OUT → CUT (the easing types make the weather change or
  the time-lapse start slowly and/or end slowly).
- Before the first key, the recorded weather and time are used. The status line shows the current weather and time.
- Weather and time are saved in the `.prj` and applied in the editor and in PLAY REPLAY → `.prj` playback.

### Sub replays (SUB 1 … SUB 9) — up to ten players in one project

- The replay you opened is the **main** replay. <kbd>N</kbd> adds another `.rep` as a **sub** replay (SUB 1, then SUB 2, … up to SUB 9) and shows its track;
  both are played together, so the main player and the sub player (with their vehicles) appear at the same time.
  The camera, weather and time always come from the main replay (and your camera / weather / time keys).
- <kbd>[</kbd> / <kbd>]</kbd> on a SUB track move that sub replay in time. Each SUB lane shows its sub replay (purple) and the main replay (blue line).
- The same `.rep` can be added several times (e.g. with different start times).
- If the sub replay starts before the main replay or ends after it, the project becomes longer at the start / end.
  In those parts the main player is not shown and the camera stays at the main replay's first / last camera
  unless you add camera keys there (the status line shows `NO MAIN HERE: ADD A CAMERA KEY`).
- Keys and the start / end marks stay on the same moment of the main replay when you move the sub replay.
  Keys that fall outside the project are kept and come back when the project gets longer again.
- Outside its range, the sub player and its vehicles are not shown.
- The sub replay is saved inside the `.prj` (like the main replay); F6 → PLAY REPLAY of the `.prj` plays both.
- Both replays should be recorded in the same area: the camera and the streaming of the world follow the main replay.
- Projects with a sub replay are saved in the new project format (v5), which older versions of this mod cannot open.

#### SUB looks — a different appearance for each sub replay

Put files into `GTA VC\CLEO\ReplayEditor\Looks\SUB1\` for SUB 1, `Looks\SUB2\` for SUB 2, … `Looks\SUB9\` (same names as the game's
files). Whatever is there is applied to that sub replay every time a project with sub replays is edited or played; files that
are not there keep the normal look. The folder goes with the number: when SUB 2 is removed and SUB 3 becomes SUB 2, it uses `Looks\SUB2\`.
The files are not saved in the `.prj`, so changing the folder changes the look of existing projects too.

| File | Effect |
|---|---|
| `player.bmp` | Skin of the sub player (like `models\generic\player.bmp`) |
| `<model>.txd` (e.g. `pcj600.txd`) | Textures of the sub replay's vehicles / peds: every texture with the same name as one in the file is replaced |
| `carcols.dat` | Only the `col` section (the colour list) is used: the sub vehicles' recorded colour numbers show the colours of this list |
| `<model>.dff` (e.g. `pcj600.dff`, `player.dff`) | Model of the sub replay's vehicle / player: the sub uses its own copy of the model, the main replay keeps the normal one |
| `particle.cfg` | (experimental) Values of `particle.cfg` for particles created by the sub replay: size, lifetime, movement... and the colour, including its fade from the start colour to the fade colour |

The status line of a SUB track lists its files in use (`LOOKS: ...`). Notes:
- Transparent parts drawn later by the game (e.g. some glass) keep the normal textures.
- Up to about 160 sub colours (all subs together) fit next to the game's colours; the game's colours are restored when the editor closes.
- With `player.dff` the sub player is created as a normal ped using the copied model (the game's player object always uses the standard player model); `player.bmp` still applies.
- `particle.cfg` must be a complete file like the game's `data\particle.cfg` (ending with the `;the end` line).
- `ReplayEditor.log` lists what was loaded (`looks: ...` lines).

### Keyframe types

- **FREE**: exactly the shot position and direction. Catmull-Rom interpolation between keyframes.
- **LOOK AT PLAYER**: position follows the keyframe path; direction looks at the player every frame.
- **FOLLOW PLAYER**: keeps its position relative to the player and follows them; the direction is independent
  (the keyframe's own direction, interpolated like FREE — it does not turn toward the player).
- **FOLLOW & LOOK AT PLAYER**: keeps its position relative to the player and follows them, and also looks at the player every frame.
- **RECORDED CAMERA**: the original recorded camera for this segment.
- Before the first keyframe the original camera is used; after the last keyframe the last keyframe's camera is held.

## Settings (`CLEO\ReplayEditor.ini`)

```
[Playback]
LOOKAROUND_AUTO_RETURN=0   ; 0 = during a replay, a camera turned with the mouse does not return to the original camera (default)
                           ; 1 = stock (returns to the original camera after about 2 seconds)
LOOKAROUND=1               ; 0 = disable the mouse look-around camera during replays

[Display]
FONT_SCALE=60              ; editor text size in % (100 = original size, 90/80/70/60..., 40–150)

[Keys]                     ; each one of F4, F5, F6, F7, F8, F9, F10, F11, F12
MENU=F6                    ; open / close the menu, exit the editor
SAVE=F7                    ; save the project being edited
SAVE_AS=F8                 ; save as a new project file
```

The three keys must all be different. If the same key is set twice, pressing the menu key
only shows an error message and nothing opens; fix the INI and restart the game.
An invalid value (e.g. `F3`) uses that key's default. The help panel and the menu show the
keys you set; this README uses the defaults F6 / F7 / F8.

The `[Playback]` options apply to all replay playback (including stock F1/F3).
With `LOOKAROUND_AUTO_RETURN=0`, moving the mouse even slightly during playback keeps the
look-around camera until that playback ends, so leave the mouse alone or set `LOOKAROUND=0`
when checking an edited camera.

## Notes

- Opening a replay file overwrites the gameplay recorded so far (for F1), just like stock F3.
- You can open a replay during a mission, but only after a warning and a second Enter press.
  The game state is restored when the replay ends, but mission scripts may break.
- Large replays with more than 8 buffers saved by mods such as "replay longer" (e.g. 6.4 MB / 64 buffers) can also be opened and played.
  The game has only 8 replay buffers, so the mod keeps the whole file in memory, fills the game buffers with the section around
  the current position, and keeps swapping sections during playback and seeking (such files are also played this way from F6 → PLAY).
  The replay longer mod itself does not need to be installed.
- The list shows only the file name, size and date.
  A `.prj` made by an earlier version (without the embedded source) opens only if the source `.rep` exists;
  saving it once with F7 converts it to the new format with the source included.
- A project whose start was trimmed opens at the start mark frame.
- Keys pressed during a replay (including the editor) are not passed to the game (e.g. no crouching after you exit).
- When opening a project, the mod briefly creates `ReplayEditor.tmp` in the user folder and deletes it right away.
- File names with Korean characters show as `?` in the list because the game font has no such glyphs (opening/playing works normally).

## Build (development)

`build.py` — MSVC 14.29 (freestanding DLL without the Windows SDK); copies the output to the `CLEO` folder.
The DLL is locked while the game runs, so close the game before building.
`tools\emu_editor.py <file.rep>` — verifies the real DLL on fake game memory with the Unicorn emulator.
