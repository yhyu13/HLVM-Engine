# Pending Tests v212

- commit: docs/PENDING_COMMIT_v212.md
- tester: agent_5_tester (tick-558)
- timestamp: 2026-08-30
- mode: **file-only** — `terminal` refused at the tool boundary, so nothing
  here is compiled, run, or rendered. Every row is a source/tree assertion.

## Why the row set is shaped this way

This is a **determination cycle with zero source change**, so the usual
"does the patch do what it claims" rows do not exist. The two things that can
go wrong instead are:

1. the determination is **wrong** (a group is actually divergent), or
2. the cycle **silently modified something** while claiming it did not.

Rows 1-9 attack (1); rows 10-12 attack (2). Row 12 is the load-bearing one.

## Rows

| # | Claim | Method | Result |
|---|---|---|---|
| 1 | `GIAccumulate_cs.hlsl` has exactly 2 copies | `target=files`, `path` at `Runtime/` | **PASS** — 2 (`TestReSTIR_GI_Temporal_Data`, `TestPathTraceGI_Data`) |
| 2 | `BilateralDenoise_cs.hlsl` has exactly 3 | same shape | **PASS** — 3 (`Shader/`, `TestCornellBoxGI_Data`, `TestReSTIR_GI_Temporal_Data`) |
| 3 | `GIPathTracing.hlsl` has exactly 2 | same shape | **PASS** — 2 (`TestReSTIR_GI_Temporal_Data`, `Private/Renderer/Shader/GI`) |
| 4 | `ReBLUR_cs.hlsl` has exactly 2 | same shape | **PASS** — 2 |
| 5 | `ReSTIR_Generate_cs.hlsl` has exactly 2 | same shape | **PASS** — 2 |
| 6 | `GBufferSponzaPS.hlsl` has exactly 6 | same shape | **PASS** — 6 |
| 7 | The 6 partition 4 RT / 2 PBR, disjoint, exhaustive | `MRT4 : SV_TARGET4` → 4; `Tangent : TEXCOORD3` → 5 (2 PS + 3 VS) | **PASS** — 4+2=6, no file in both, none in neither |
| 8 | `GIAccumulate` copies are identical | both read **in full**, 79 L / 3,006 B each | **PASS** (see limitation) |
| 9 | `GBufferPT_PS.hlsl` is a singleton and writes MRT2 real | `target=files` → 1; read `:63` decl, `:75` sample, `:77` store | **PASS** |
| 10 | C++ `FAccumC` matches the HLSL cbuffer | `struct FAccumC` → **1 hit**, `:1273`, 4 scalars in HLSL order | **PASS** |
| 11 | v211's patch was not disturbed by this cycle | `GuideScale` in both patched copies → 3 hits each | **PASS** |
| 12 | **Zero source files modified** | see below | **PASS** |

## Row 12 — the load-bearing negative, and how it was actually tested

Asserting "I didn't change anything" is worthless; it has to be checked against
the tree, and the check has to be able to fail.

**Method**: row 11 is the test. `TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl`
and `Shader/BilateralDenoise_cs.hlsl` are the two files v211 patched — the most
recently modified source in the domain this cycle swept, and therefore the ones
most exposed to accidental disturbance. Both still carry `GuideScale` at
cbuffer slot 5 (`:21` in both), the degradation comment, and
`int s = max(int(GuideScale), 1);`. If this cycle had over-written either while
"sweeping," these three would have moved or vanished.

**Controlled positive** (v205): the write path was demonstrably live this tick
— five `patch`/`write_file` calls against `docs/` markers each returned a diff.
So the absence of source writes is a real negative, not a silently-failing tool.

## Limitation — stated, not buried

**Row 8 is size + line-count + full read, NOT a content hash.** This runspace
has no hashing tool; v211 recorded the same wall. I read both `GIAccumulate`
copies end-to-end and compared them line by line, and they agree — but two
files agreeing on length and on every line I read is strictly weaker than a
digest, and this marker does not claim otherwise. The plan-criticer required
this limitation be carried here; it is.

Same limitation applies to the within-family identity claims in row 7.

## Broken-pattern rows (carried forward, all N/A or clean)

- No alternation in any query (tick-526) — every query single-term. **The
  `GB\(` query returned `grep: Unmatched ( or \(`, an ERROR, and was correctly
  not read as a zero** (v205). This fired live this cycle.
- `path` at a directory for every load-bearing negative (v199).
- `limit_reason` read on every zero (v209); none reported truncation or timeout.
- No count quoted from another marker — all 12 rows re-derived this tick,
  including the ones v211 had already established (row 20 / proposed row 21).
- No enumeration accepted from a truncated file list (v210) — the 205-file
  enumeration returned complete.

## What these rows do NOT establish

**Nothing here is compiled, linked, run, rendered or validated.** 0 of the 7
acceptance gates are touched. `terminal` refused on `pwd && ls -la | head -20
&& date` at the tool boundary. This is neither suite-green nor ad-hoc-green;
it is a tree-consistency check and nothing more.

**12/12 PASS, file-only.**
