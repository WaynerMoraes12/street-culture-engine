# Sample lyrics

Original verses written for the Street Culture Engine (no third-party lyrics). Use them to try the
`bars` analyzer:

| file | language | shape | what to look for |
|------|----------|-------|------------------|
| `hardwood_hymn.txt` | English | 16 bars, 4 stanzas | `AABB CCDD EEFF GGGG`, internal rhymes (tight/night/fight), a 4-line "-ary" chain with a 5-syllable multi |
| `coracao_de_quadra.txt` | Portuguese (BR) | 16 bars, 4 stanzas | `AABB CCDD EEBB FFGG` (the "-ão" rhyme comes back), memória / vitória multis |
| `hall_of_fame.txt` | English | 16 bars, 4 stanzas | tribute roll call: Bronx '73, Run-DMC, MJ, AI, Kobe, LeBron, Eminem, Iron Maiden; `AABB CCDD EEFF GGHH`, flu / flew / through |
| `fourth_quarter.txt` | English | `[Verse]` + `[Hook]` | section headers act as stanza breaks; "is ticking down" / "loudest in the town" |

```
street bars examples/lyrics/hardwood_hymn.txt
street bars examples/lyrics/coracao_de_quadra.txt --bpm 95
street bars examples/lyrics/fourth_quarter.txt --no-color
street bars --sample tribute            # hall_of_fame.txt is also built in
```

Files are UTF-8 plain text. Each line is one bar, blank lines separate stanzas, and
bracketed lines such as `[Hook]` are treated as headers. Write your own the same way,
or pipe text in with `street bars -`.
