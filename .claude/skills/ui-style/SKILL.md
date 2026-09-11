---
name: ui-style
description: >-
  Check and tune MegaDirStat's look by screenshotting the running app on mock
  data. Launches the app at a chosen size, captures it, and runs the
  edit -> build -> relaunch -> capture loop from one command. Use for any
  visual work: treemap colours and spacing, tree columns, layout, light/dark
  theme, and whenever asked to take a screenshot or check how something looks.
  見た目の調整・配色・レイアウト・スクショを撮る、のときに使う。
allowed-tools: Bash(python .claude/skills/ui-style/scripts/ui_shot.py cycle*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py shot*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py launch*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py close*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py build*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py resize*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py move*), Bash(python .claude/skills/ui-style/scripts/ui_shot.py info*)
---

# UI check / style tuning loop

All work goes through one script. Run it from the repo root:

```
python .claude/skills/ui-style/scripts/ui_shot.py <subcommand> [...]
```

PNGs land in `.screenshots/` (gitignored) with an incrementing prefix, e.g.
`.screenshots/007-treemap.png`. Read the PNG the command reports.

The app is **always launched on mock data** — `--mock-generate 20000 --seed 1`
by default, or whatever `--app-args` says (it must contain `--mock...`; the
script refuses otherwise). Never point it at a real account.

## The loop

1. **Baseline** — `... launch --size 1200x800` then `... shot before`.
   Size, theme and app args are remembered for later commands.
2. **Edit** the code.
3. **Iterate** — `... cycle after`: close → build → launch → screenshot in one
   call. Compare, repeat from 2.
4. **Finish** — `... close`. Always close before ending the turn.

Useful data sets:

- `--app-args "--mock tests/fixtures/sample.json"` — 16 hand-written files, easy to reason about
- `--app-args "--mock-generate 20000 --seed 1"` — the default, realistic density
- `--app-args "--mock-generate 300000"` — stress the treemap rendering
- `--app-args "--mock-generate 5000 --mock-delay 5000"` — the loading state

## Rules that keep this cheap

- **Use `cycle`**, not `close` + `build` + `launch` + `shot` as separate calls.
- **Crop instead of scaling** for one component: `... shot tree --crop 0,0,1200,200`.
- **Read each PNG once.**
- Check both themes when colours change: `--theme light|dark` (sets
  `MEGADIRSTAT_COLOR_SCHEME`, no need to touch the OS setting).
- Resize without rebuilding: `... resize 800x600` + `... shot narrow`.

## Driving the UI — ask first

`drive` injects real mouse and keyboard input and takes over the user's machine
for a few seconds. **Ask the user before the first `drive` of a check**; one
answer covers every `drive` call of that check. On a refusal, hand the point to
the user as something to check by hand.

```
python .claude/skills/ui-style/scripts/ui_shot.py drive "click 400,500; wait 300; shot selected"
```

Steps are `;`-separated: `move`, `click`, `drag`, `scroll`, `key`, `type`,
`wait`, `shot`. Coordinates are screenshot pixels from the client area's top
left.

Full option reference and failure modes: [reference.md](reference.md).
