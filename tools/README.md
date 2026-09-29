# `tools` — developer utilities

## `ansi2svg.py` — terminal screenshots as SVG

Every screenshot in the README is real Street Culture Engine output, converted from ANSI escape codes
to SVG by this script. GitHub renders SVG inline, so there's no GIF or video tooling
involved and the images stay small, crisp and diffable.

```bash
FORCE_COLOR=1 street game --seed 7 | python tools/ansi2svg.py --title "street game --seed 7" > docs/img/game.svg
```

How it keeps the output pixel-accurate whatever font the viewer has:

- **Every text chunk is pinned to its column.** Text is split at spaces, and each chunk is
  placed at `x = column × cell width` and stretched to exactly `length × cell width` with
  `textLength`. Font metrics can't make the columns drift.
- **Block elements are geometry, not glyphs.** `█ ▁▂▃▄▅▆▇ ▏▎▍▌▋▊▉ ░▒▓` become rectangles
  that fill the cell exactly, so bars, sparklines and candlesticks have no gaps.
- **Box drawing is vector paths.** Light, heavy, double and rounded borders (`─│╭╮╔═╗…`)
  are drawn as strokes from the cell center to its edges, so tables and panels join
  perfectly between rows.

It supports the SGR codes the engine emits: reset, bold, dim, italic, underline,
24-bit foreground/background, and the basic 8/16 colors.

## `screenshots.sh` — regenerate every README image

```bash
./tools/screenshots.sh build/release/apps/street/street
```

Runs each showcased command with a fixed seed and writes the SVGs into `docs/img/`. Since
every command is deterministic, the images only change when the code does.
