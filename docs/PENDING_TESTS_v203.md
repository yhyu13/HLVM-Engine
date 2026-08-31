# Pending Tests v203

- commit: docs/PENDING_COMMIT_v203.md
- role: agent_5_tester (tick-549)
- mode: **file-only**. No build, no run, no image. Every row is a source query
  or a read. No row may be cited as evidence that an acceptance gate passed.

## Rules honoured

No `|` alternation (tick-526). `path` at a directory for every directory-scoped
query (v199). No count quoted from another marker — all re-derived. Every zero
paired with a same-shape positive in the same file or directory.

| # | Assertion | Method | Result |
|---|---|---|---|
| 1 | `TemporalLayoutSRV` declares cb + t0..t9 | read `:236-248` | PASS — 11 items |
| 2 | Primary temporal shader declares t0..t9 | read `:50-59` | PASS |
| 3 | Control temporal shader declares t0..t7 only | read `:48-55` | PASS |
| 4 | `gHistRadiance` absent in control | query, control file | 0 hits |
| 5 | Control for row 4 (same shape, primary file) | query | **1 hit** `:59` — zero is real |
| 6 | `gCurrRadiance` absent in control | query, control file | 0 hits |
| 7 | Slots 8/9 bound unconditionally | read `:568-569` | PASS — **not** `:562-563` |
| 8 | `TemporalLayoutUAV` declares 3 UAVs | read `:280-284` | PASS |
| 9 | UAV binding set supplies 3 items | read `:574-578` | PASS |
| 10 | Control temporal shader declares 2 UAVs, default space | read `:57-58` | PASS |
| 11 | `gOutRadiance` absent in control | query, control file | 0 hits |
| 12 | Control for row 11 (primary file) | query | 1 hit `:63` — zero is real |
| 13 | `space1` present in control's GENERATE copy | query dir | PASS `:36-37` — divergence is intra-directory |
| 14 | Both spatial copies declare t0..t4 + u0 | read both | PASS — pair clean |
| 15 | Control is a live consumer of temporal | read `:1610-1612` | PASS — CVar-guarded call |
| 16 | Control supplies slots 8/9 and OutRadiance | read `:1599`,`:1600`,`:1607` | PASS |
| 17 | `createBindingLayout` count | query | 5 |
| 18 | `LayoutDesc.bindings` count | query | 5 |
| 19 | **`SpatialLayout` list intact after the near-miss** | read `:325-333` | PASS — 7 items, correct order |
| 20 | Known-good control `.cpp`/`.hlsl` byte-unchanged | no edit issued | PASS |
| 21 | No `.hlsl` modified this cycle | no edit issued | PASS |

**21/21 PASS.** Row 19 is the row that matters: it is the only one that would
have failed had the impler not caught its own deletion.

## Row 7 — the reviewer's correction re-verified independently

I did not take the reviewer's word that `:562-563` was wrong. Read both ranges:
`:562-563` are `HistoryReservoir0`/`HistoryReservoir1` (slots 2 and 3);
`:568-569` are `CurrentRadiance`/`HistoryRadiance` (slots 8 and 9). The
reviewer's correction is right and the impler's underlying claim survives.

## What this cycle did NOT establish

That anything compiles, links, runs, renders or validates. Static audit only,
as v200/v201/v202 were. Gates 1-7 unchanged at 0/7 verified.
