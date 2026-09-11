# `ui_shot.py` reference

Read this only when a command misbehaves or you need a flag `SKILL.md` doesn't
list. Prefix: `python .claude/skills/ui-style/scripts/ui_shot.py`.

## Subcommands

| Command | Purpose |
| --- | --- |
| `launch [--size WxH] [--pos X,Y] [--theme T] [--app-args "..."] [--timeout 45]` | Start the app (size passed as `--window-size`), wait until the scene stops changing. Refuses if a window is already open. |
| `shot [NAME] [capture opts] [--window]` | Capture and save `.screenshots/NNN-NAME.png`. |
| `cycle [NAME] [launch opts] [--target T] [--reconfigure] [--no-build] [capture opts]` | `close` → `build` → `launch` → `shot`. Aborts before relaunching if the build fails. |
| `build [--target MegaDirStat] [--reconfigure]` | Build and print a deduplicated summary. Exit code 1 on failure. |
| `close [--force]` | `WM_CLOSE`, wait up to 12s; `--force` kills. |
| `resize WxH` / `move X,Y` | Adjust the live window (client area gets the requested size). |
| `info` | Window handle, pid, client size, DPI scale, remembered app args. |
| `drive "STEPS"` | Inject input. **Ask the user first.** |

## Capture options (`shot`, `cycle`, `drive`)

| Flag | Meaning |
| --- | --- |
| `--crop x,y,w,h` | Crop in screenshot pixels. |
| `--max-width N` | Downscale if wider than N. Prefer `--crop`. |
| `--grid N` | Overlay a measuring grid every N px. |
| `--method auto\|print\|screen` | `auto` tries `PrintWindow` first and falls back to a desktop blit (window raised) when the result is blank or a popup window (QMenu, combo list) is open. |
| `--delay MS` | Sleep before capturing. |
| `--window` | Include the window frame instead of only the client area. |

## `drive` steps

| Step | Example |
| --- | --- |
| `move X,Y` / `hover X,Y` | `move 400,300` |
| `click X,Y [right\|middle] [double]` | `click 400,300 right` |
| `drag X1,Y1 X2,Y2` | `drag 400,300 120,500` |
| `scroll X,Y [notches]` | `scroll 600,400 -3` (negative = down) |
| `key COMBO` | `key ctrl+shift+n`, `key F5` |
| `type TEXT` | `type "abc"` |
| `wait MS` | `wait 400` |
| `shot [NAME]` | capture mid-sequence |

`--no-countdown` skips the 2s warning; the cursor is restored afterwards.

## Environment

| Item | Value | Override |
| --- | --- | --- |
| Exe | `build/msvc-debug/Debug/MegaDirStat.exe` | — |
| CMake | `C:/Qt/Tools/CMake_64/bin/cmake.exe` | `UI_SHOT_CMAKE` |
| Qt bin | `C:/Qt/6.11.1/msvc2022_64/bin` | `UI_SHOT_QT_BIN` |
| App CWD | repo root, so relative `--mock` paths resolve | — |

State: `.screenshots/.session.json` (pid, hwnd, size, theme, app args, counter).

## Failure modes

| Symptom | Cause / fix |
| --- | --- |
| `app window not found` | Not launched or it exited. `info`, then `launch`. |
| `an app window is already open` | `close` it (or use `cycle`). |
| `app exited immediately` | Bad app args (e.g. a missing fixture shows an error box and exits) or a crash. Run the exe by hand with the same args. |
| `ready=timeout` | The scene kept changing — usually a long `--mock-delay`. The shot is still taken. |
| `app args must select mock data` | By design: this script never launches against a real account. |
| Blank capture even on `screen` | Window minimized; `resize`/`move` restores it, or `close --force` and relaunch. |
