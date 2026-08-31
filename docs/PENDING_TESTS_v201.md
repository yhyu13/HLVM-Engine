# Pending Tests v201

- commit: docs/PENDING_COMMIT_v201.md
- tester: agent_5_tester (tick-547)
- mode: file-only verification (terminal categorically denied)
- test files produced: **none** (`produces_test_files: no`)

Every row re-run independently. Per tick-526 no `|` alternation; per v199 `path`
never at a directory for a load-bearing negative; per v200 every row whose
subject is a declaration READS the hits rather than counting them.

| # | Claim | Query / read | Result |
|---|---|---|---|
| 1 | `BackBufferResizing` recreates nothing | `read_file :1385-1388` in full | **PASS** — body is exactly `BindingCache.Clear();`, 4 lines total |
| 2 | The override exists (row 1 is not vacuous) | `search_files "void BackBufferResizing"` | **PASS** — 1 hit, so row 1 read a real override, not a missing one |
| 3 | All `CreateTexture2D` sites fixed-extent | `search_files "CreateTexture2D"` → 16 hits, read | **PASS** — 1 decl `:210`, 15 calls `:1655`-`:1717`, all `W`/`H`/`HalfW` |
| 4 | `W`/`H` are the file-scope constants | `read_file :1616` | **PASS** — `const uint32_t W = WIDTH, H = HEIGHT;` |
| 5 | `HalfW` derives from the same constant | `read_file :1671` | **PASS** — `const uint32_t HalfW = W / 2;` |
| 6 | **Reviewer's catch** — raw `createTexture` also enumerated | `search_files "createTexture"` → 8 hits, read | **PASS** — `:570` uses `WIDTH`/`HEIGHT` (`:563-564`); `:315` is 1x1 |
| 7 | No texture is swapchain-sized | `search_files "Desc.width = FB"` | **PASS (0 hits)** |
| 8 | Row 7's zero is controlled | `search_files "Desc.width"` | **PASS** — 11 hits, so the shape matches when it should |
| 9 | `FB.width` union: only 3 live sites | `search_files "FB.width"` → 13 hits, all read | **PASS** — 10 comments, `:754`/`:756` resize-detection, `:1326` blit dest |
| 10 | `FB.height` union agrees | `search_files "FB.height"` → 7 hits, all read | **PASS** — same three live categories |
| 11 | Live dispatch grids are fixed-extent | `search_files "+ 7) / 8"` → 10 hits | **PASS** — `:1178`, `:1312` both `(WIDTH + 7) / 8, (HEIGHT + 7) / 8` |
| 12 | Immunity is load-bearing, not moot | `search_files "Resizable"` | **PASS** — 1 hit, `:3035` `= true`; the window really can resize |
| 13 | Zero source files modified | no `patch`/`write_file` against any `.cpp`/`.hlsl`/`.h` this cycle | **PASS** |
| 14 | Both `GIPathTracing.hlsl` copies untouched (v182 hazard) | not edited this cycle | **PASS** — trivially, no shader touched |

## Row 6 is the row that carries this cycle

Rows 3-5 reproduce the impler's own reasoning and would have passed against an
incomplete set. Row 6 is the reviewer's independent enumeration, and it is the
only row that could have falsified the conclusion. It did not — but it is the
one that makes rows 3-5 mean anything.

## Disagreement with the commit marker: none

I found no arithmetic or classification I would state differently. Noting this
explicitly because v200's audit valued the tester *disagreeing*; agreement is
only informative when disagreement was possible, and here I re-derived rows 6,
9 and 11 from scratch before reading the impler's numbers.

## What these rows do NOT establish

Nothing compiles, runs, renders or validates here. All 14 rows are static reads.
No row may be cited as evidence that any acceptance gate passed.
