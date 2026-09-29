# Groove files

These are hand-written grooves in the `beats` pattern DSL. Render one with:

```text
street beat --pattern examples/patterns/<file>.groove            # uses the file's own style
street beat --pattern examples/patterns/<file>.groove --style lofi   # same groove, another kit
```

| File | Style | What to listen for |
|---|---|---|
| `sunday-stoop.groove` | boom bap, 90 BPM, 62% swing | Syncopated kick, ghost snares (`g`), an open hat at the end of bar 2, a bass that doubles the kick, and Dm9 → Gm7 → Bbmaj7 → A7 changes |
| `rooftop-trap.groove` | trap, 142 BPM | Clap on beat 3, 1/32 (`r`) and triplet (`t`, `T`) hat rolls, a snare roll into the loop point, and an 808 that swoops up an octave (`^`) and slides home (`~`) |
| `baile-tamborzao.groove` | funk carioca, 130 BPM | The tamborzão: kick and low tom trading the 3-3-2 tresillo, claps on 2 and 4, an accented shaker, a high tom (`^`) answering each bar |
| `bronx-1982.groove` | electro, 128 BPM, straight | Early-80s TR-808 electro-funk: boom kick, snare and handclap together on 2 and 4, a cowbell riff, accented 16th hats, a bass jumping up the octave (`^`) under off-beat saw stabs |
| `iron-charge.groove` | gallop, 176 BPM | An NWOBHM-style gallop (`x.oo`) on kick, bass and palm-muted power chords (`E5 D5 C5`), with an open chord (`*`) ringing over a tom run |

## Syntax at a glance

```text
# '#' at line start or after a space opens a comment
title   My Groove
bpm     92          # optional directives: title, bpm, swing, key, style, chords
swing   60%
key     Am
style   boombap
chords  Am9 Dm7 | G9 Cmaj7

kick    | x..o ..x. ..o. .... |
snare   | .... x..g .... x... |
hat     | o.o. o.o. o.o. orr. |
```

`x` accent · `o` hit · `g` ghost · `.` rest · `r` 1/32 roll · `t` triplet roll ·
`^` an octave up (bass: slide up into it) · `~` bass: slide back to the root ·
`*` guitar: let an open power chord ring (plain guitar hits are palm-muted chugs).
Each track holds whole bars of 16 steps. Shorter tracks loop, and the command
loops the groove to `--bars` (default 8). The full reference is in
[`modules/beats/README.md`](../../modules/beats/README.md).
