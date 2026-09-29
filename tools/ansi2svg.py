#!/usr/bin/env python3
"""Render ANSI-colored terminal output as an SVG "screenshot".

The README images in docs/img are produced with this script from real Street Culture Engine output:

    FORCE_COLOR=1 street game --seed 7 | python tools/ansi2svg.py \
        --title "street game --seed 7" > docs/img/game.svg

Every styled run is placed at its exact column and stretched to an exact width
(textLength), so tables and box drawings stay aligned whatever monospace font
the viewer happens to have.
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from dataclasses import dataclass, replace

CSI = re.compile(r"\x1b\[([0-9;]*)m")

BACKGROUND = "#0d1117"
FOREGROUND = (201, 209, 217)
BASIC = [
    (1, 4, 9), (239, 68, 68), (34, 197, 94), (255, 196, 0),
    (59, 130, 246), (139, 92, 246), (34, 211, 238), (229, 231, 235),
]

FONT_SIZE = 14
CELL_W = 8.4
LINE_H = 18
PAD_X = 18
PAD_TOP = 46
PAD_BOTTOM = 18
ROW_GAP = 2  # px of air above block elements, so stacked bars stay distinct


@dataclass(frozen=True)
class Style:
    fg: tuple[int, int, int] | None = None
    bg: tuple[int, int, int] | None = None
    bold: bool = False
    dim: bool = False
    italic: bool = False
    underline: bool = False


def apply_sgr(style: Style, params: str) -> Style:
    codes = [int(p) if p else 0 for p in params.split(";")] if params else [0]
    i = 0
    while i < len(codes):
        c = codes[i]
        if c == 0:
            style = Style()
        elif c == 1:
            style = replace(style, bold=True)
        elif c == 2:
            style = replace(style, dim=True)
        elif c == 3:
            style = replace(style, italic=True)
        elif c == 4:
            style = replace(style, underline=True)
        elif c == 22:
            style = replace(style, bold=False, dim=False)
        elif c == 23:
            style = replace(style, italic=False)
        elif c == 24:
            style = replace(style, underline=False)
        elif c == 39:
            style = replace(style, fg=None)
        elif c == 49:
            style = replace(style, bg=None)
        elif c in (38, 48) and i + 4 < len(codes) and codes[i + 1] == 2:
            rgb = tuple(codes[i + 2 : i + 5])
            style = replace(style, fg=rgb) if c == 38 else replace(style, bg=rgb)
            i += 4
        elif 30 <= c <= 37:
            style = replace(style, fg=BASIC[c - 30])
        elif 90 <= c <= 97:
            style = replace(style, fg=BASIC[c - 90])
        elif 40 <= c <= 47:
            style = replace(style, bg=BASIC[c - 40])
        i += 1
    return style


def parse(text: str) -> list[list[tuple[str, Style]]]:
    lines: list[list[tuple[str, Style]]] = []
    style = Style()
    for raw in text.expandtabs(4).splitlines():
        runs: list[tuple[str, Style]] = []
        pos = 0
        for match in CSI.finditer(raw):
            if match.start() > pos:
                runs.append((raw[pos : match.start()], style))
            style = apply_sgr(style, match.group(1))
            pos = match.end()
        if pos < len(raw):
            runs.append((raw[pos:], style))
        lines.append(runs)
    while lines and not any(t.strip() for t, _ in lines[-1]):
        lines.pop()
    return lines


def background(width: int, height: int, theme: str) -> list[str]:
    """Window background. 'retro' is a deep-purple CRT with faint scanlines; 'github' is flat."""
    if theme == "github":
        return [f'<rect width="{width}" height="{height}" rx="10" fill="{BACKGROUND}"/>']
    return [
        "<defs>"
        '<linearGradient id="crt" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#1a0f2e"/><stop offset="1" stop-color="#0b0716"/>'
        "</linearGradient>"
        '<pattern id="scan" width="4" height="3" patternUnits="userSpaceOnUse">'
        '<rect width="4" height="1" fill="#ffffff" opacity="0.025"/>'
        "</pattern>"
        '<linearGradient id="neon" x1="0" y1="0" x2="1" y2="0">'
        '<stop offset="0" stop-color="#ffc400"/><stop offset="0.5" stop-color="#ff4081"/>'
        '<stop offset="1" stop-color="#8b5cf6"/>'
        "</linearGradient>"
        "</defs>",
        f'<rect width="{width}" height="{height}" rx="10" fill="url(#crt)"/>',
        f'<rect width="{width}" height="{height}" rx="10" fill="url(#scan)"/>',
        f'<rect x="10" y="{height - 3}" width="{width - 20}" height="2" rx="1" fill="url(#neon)" opacity="0.8"/>',
    ]


def hex_color(rgb: tuple[int, int, int]) -> str:
    return "#{:02x}{:02x}{:02x}".format(*rgb)


# Block elements are drawn as rectangles: (x0, y0, x1, y1) as fractions of the cell,
# plus an opacity for the shade characters.
BLOCKS: dict[str, tuple[float, float, float, float, float]] = {
    "█": (0, 0, 1, 1, 1.0), "▀": (0, 0, 1, 0.5, 1.0), "▐": (0.5, 0, 1, 1, 1.0),
    "░": (0, 0, 1, 1, 0.25), "▒": (0, 0, 1, 1, 0.5), "▓": (0, 0, 1, 1, 0.75),
}
for _i, _ch in enumerate("▁▂▃▄▅▆▇", start=1):  # lower n/8 blocks
    BLOCKS[_ch] = (0, 1 - _i / 8, 1, 1, 1.0)
for _i, _ch in enumerate("▏▎▍▌▋▊▉", start=1):  # left n/8 blocks
    BLOCKS[_ch] = (0, 0, _i / 8, 1, 1.0)

# Box drawing: arms (up, right, down, left) with weight 1 light, 2 heavy, 3 double.
_BOX_SPEC = {
    "─": "0101", "│": "1010", "┌": "0110", "┐": "0011", "└": "1100", "┘": "1001",
    "├": "1110", "┤": "1011", "┬": "0111", "┴": "1101", "┼": "1111",
    "╭": "0110", "╮": "0011", "╰": "1100", "╯": "1001",
    "╴": "0001", "╵": "1000", "╶": "0100", "╷": "0010",
    "╸": "0002", "╹": "2000", "╺": "0200", "╻": "0020", "╽": "1020", "╿": "2010",
    "━": "0202", "┃": "2020", "┏": "0220", "┓": "0022", "┗": "2200", "┛": "2002",
    "┣": "2220", "┫": "2022", "┳": "0222", "┻": "2202", "╋": "2222",
    "═": "0303", "║": "3030", "╔": "0330", "╗": "0033", "╚": "3300", "╝": "3003",
    "╠": "3330", "╣": "3033", "╦": "0333", "╩": "3303", "╬": "3333",
}
BOX = {ch: tuple(int(c) for c in spec) for ch, spec in _BOX_SPEC.items()}
ROUNDED = set("╭╮╰╯")
DIRECTIONS = [(0, -1), (1, 0), (0, 1), (-1, 0)]  # up, right, down, left


def box_paths(ch: str, x: float, top: float) -> list[tuple[str, float]]:
    """SVG path data (and stroke width) that draws one box-drawing cell."""
    arms = BOX[ch]
    cx, cy = x + CELL_W / 2, top + LINE_H / 2
    half = [LINE_H / 2, CELL_W / 2, LINE_H / 2, CELL_W / 2]
    if ch in ROUNDED:
        (ax, ay), (bx, by) = [DIRECTIONS[i] for i in range(4) if arms[i]]
        r = CELL_W / 2
        ha = half[DIRECTIONS.index((ax, ay))]
        hb = half[DIRECTIONS.index((bx, by))]
        d = (f"M{cx + ax * ha:.2f},{cy + ay * ha:.2f} L{cx + ax * r:.2f},{cy + ay * r:.2f} "
             f"Q{cx:.2f},{cy:.2f} {cx + bx * r:.2f},{cy + by * r:.2f} "
             f"L{cx + bx * hb:.2f},{cy + by * hb:.2f}")
        return [(d, 1.2)]
    paths: list[tuple[str, float]] = []
    gap = 1.6  # half distance between the two strokes of a double line
    for i, weight in enumerate(arms):
        if not weight:
            continue
        ax, ay = DIRECTIONS[i]
        px, py = -ay, ax  # perpendicular
        end = (cx + ax * half[i], cy + ay * half[i])
        if weight == 3:
            for side in (-1, 1):
                perp_dir = (px * side, py * side)
                # Stop short where a perpendicular arm continues on this side, otherwise
                # extend back so the outer strokes of a corner meet.
                t = 1 if (perp_dir in DIRECTIONS and arms[DIRECTIONS.index(perp_dir)]) else -1
                ox, oy = px * side * gap, py * side * gap
                sx, sy = cx + ax * t * gap + ox, cy + ay * t * gap + oy
                paths.append((f"M{sx:.2f},{sy:.2f} L{end[0] + ox:.2f},{end[1] + oy:.2f}", 1.0))
        else:
            width = 1.2 if weight == 1 else 2.4
            sx, sy = cx - ax * width / 2, cy - ay * width / 2
            paths.append((f"M{sx:.2f},{sy:.2f} L{end[0]:.2f},{end[1]:.2f}", width))
    return paths


def text_element(chunk: str, col: int, baseline: int, color: str, style: Style) -> str:
    attrs = [
        f'x="{PAD_X + col * CELL_W:.1f}"',
        f'y="{baseline}"',
        f'fill="{color}"',
        f'textLength="{len(chunk) * CELL_W:.1f}"',
        'lengthAdjust="spacingAndGlyphs"',
    ]
    if style.bold:
        attrs.append('font-weight="bold"')
    if style.dim:
        attrs.append('opacity="0.6"')
    if style.italic:
        attrs.append('font-style="italic"')
    if style.underline:
        attrs.append('text-decoration="underline"')
    return f'<text {" ".join(attrs)}>{html.escape(chunk)}</text>'


def render(lines: list[list[tuple[str, Style]]], title: str, min_cols: int, theme: str = "retro") -> str:
    cols = max([min_cols] + [sum(len(t) for t, _ in runs) for runs in lines])
    width = round(PAD_X * 2 + cols * CELL_W)
    height = PAD_TOP + len(lines) * LINE_H + PAD_BOTTOM
    head = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        *background(width, height, theme),
        '<circle cx="22" cy="22" r="6" fill="#ff5f56"/>',
        '<circle cx="42" cy="22" r="6" fill="#ffbd2e"/>',
        '<circle cx="62" cy="22" r="6" fill="#27c93f"/>',
    ]
    if title:
        head.append(
            f'<text x="{width / 2:.1f}" y="27" text-anchor="middle" fill="#8b949e" '
            f'font-family="ui-sans-serif, -apple-system, Segoe UI, Helvetica, Arial, sans-serif" '
            f'font-size="13">{html.escape(title)}</text>'
        )

    shapes: list[str] = []  # backgrounds, blocks and box lines, drawn under the text
    texts: list[str] = []
    for row, runs in enumerate(lines):
        top = PAD_TOP + row * LINE_H
        baseline = top + FONT_SIZE - 1
        col = 0
        for text, style in runs:
            color = hex_color(style.fg or FOREGROUND)
            opacity = 0.6 if style.dim else 1.0
            if style.bg is not None:
                shapes.append(
                    f'<rect x="{PAD_X + col * CELL_W:.1f}" y="{top}" '
                    f'width="{len(text) * CELL_W:.1f}" height="{LINE_H}" '
                    f'fill="{hex_color(style.bg)}"/>'
                )
            # Plain glyphs are grouped into chunks that break at spaces, so each chunk
            # starts at its exact column; blocks and box lines are drawn as geometry.
            chunk, chunk_col = "", col
            for ch in text + " ":
                if ch in BLOCKS or ch in BOX or ch == " ":
                    if chunk:
                        texts.append(text_element(chunk, chunk_col, baseline, color, style))
                    chunk = ""
                    x = PAD_X + col * CELL_W
                    if ch in BLOCKS:
                        x0, y0, x1, y1, alpha = BLOCKS[ch]
                        # Overlap horizontally by a hair so adjacent cells join without seams,
                        # but keep a thin gap between rows like a real terminal's line spacing.
                        y_top = top + max(y0 * LINE_H, ROW_GAP)
                        shapes.append(
                            f'<rect x="{x + x0 * CELL_W:.2f}" y="{y_top:.2f}" '
                            f'width="{(x1 - x0) * CELL_W + 0.3:.2f}" '
                            f'height="{top + y1 * LINE_H - y_top:.2f}" fill="{color}" '
                            f'opacity="{alpha * opacity:.2f}" shape-rendering="crispEdges"/>'
                        )
                    elif ch in BOX:
                        for d, stroke in box_paths(ch, x, top):
                            shapes.append(
                                f'<path d="{d}" stroke="{color}" stroke-width="{stroke}" '
                                f'fill="none" opacity="{opacity:.2f}"/>'
                            )
                    col += 1
                    chunk_col = col
                else:
                    chunk += ch
                    col += 1
            col -= 1  # the sentinel space appended above is not part of the run

    font = ("ui-monospace, SFMono-Regular, Menlo, Consolas, 'DejaVu Sans Mono', "
            "'Liberation Mono', monospace")
    body = head + shapes + [f'<g font-family="{font}" font-size="{FONT_SIZE}">'] + texts
    return "\n".join(body + ["</g></svg>"]) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("input", nargs="?", help="file with ANSI text (default: stdin)")
    parser.add_argument("--title", default="", help="window title")
    parser.add_argument("--min-cols", type=int, default=80, help="minimum width in columns")
    parser.add_argument("--theme", choices=["retro", "github"], default="retro",
                        help="window style: 80s CRT (default) or flat GitHub dark")
    args = parser.parse_args()

    if args.input:
        with open(args.input, encoding="utf-8") as fh:
            text = fh.read()
    else:
        text = sys.stdin.buffer.read().decode("utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stdout.write(render(parse(text), args.title, args.min_cols, args.theme))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
