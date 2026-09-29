#!/usr/bin/env bash
# Regenerates the README screenshots in docs/img from real Street Culture Engine output.
# Every command runs with a fixed seed, so images only change when the code does.
#
#   ./tools/screenshots.sh [path/to/street]
set -euo pipefail

exe="${1:-build/release/apps/street/street}"
python="${PYTHON:-python3}"
out="docs/img"
mkdir -p "$out"

# shot NAME "TITLE" MAX_LINES ARGS...   (MAX_LINES 0 = keep everything)
shot() {
    local name="$1" title="$2" lines="$3"
    shift 3
    local cut=(cat)
    [ "$lines" -gt 0 ] && cut=(head -n "$lines")
    FORCE_COLOR=1 COLUMNS=100 "$exe" "$@" | "${cut[@]}" |
        "$python" tools/ansi2svg.py --title "$title" > "$out/$name.svg"
    echo "  wrote $out/$name.svg"
}

shot banner  "street"                                   20 --color
shot culture "street culture --seed 7"                  78 culture --seed 7 --weeks 1
shot beat    "street beat --style boombap --seed 42"     0 beat --style boombap --seed 42 --dry-run
shot bars    "street bars --sample tribute"              0 bars --sample tribute
shot game    "street game --home QNS --away BRX --seed 7" 64 game --home QNS --away BRX --seed 7 --shot-chart none
shot odds    "street season --odds 2000 --seed 7"        0 season --odds 2000 --seed 7
shot market  "street market --seed 7"                   62 market --seed 7
shot legends "street legends --from 1984 --to 1998"      0 legends --from 1984 --to 1998
