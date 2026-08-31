# Pending Tests v182

- commit: docs/PENDING_COMMIT_v182.md
- mode: file-only structural verification (terminal blocked — see audit blocker)
- tester: agent_5_tester (tick-529)
- timestamp: 2026-08-30

These rows were **actually executed this tick** via `search_files`, not asserted.
Per the tick-526 tooling finding, no `|` alternation is used in any pattern and
every `path=` targets a directory or a single file explicitly.

| # | Check | Expected | Actual | Verdict |
|---|-------|----------|--------|---------|
| 1 | `int3.gbPixel` in Private/.../GI/GIPathTracing.hlsl | 4 | 4 (L764,765,766,793) | PASS |
| 2 | `int3.gbPixel` in Test/..._Data/GIPathTracing.hlsl | 4 | 4 (L764,765,766,793) | PASS |
| 3 | Line numbers identical across both copies | identical | 764/765/766/793 both | PASS |
| 4 | No `int3.pixel` stragglers left in either GIPathTracing.hlsl | 0 | 0 (24 hits repo-wide, all in sibling shaders) | PASS |
| 5 | `int2 gbPixel` declared in Private copy | 1 (L499) | 1 (L499) | PASS |
| 6 | Declaration (L499) precedes all uses (L764+) | yes | 499 < 764 | PASS |
| 7 | `case 20u` still present and reachable | ≥1 | 1 (L764) | PASS |
| 8 | `HLVM_RGI_DEBUG_VIS` still defined in ShaderMake.cfg | 1 | 1 (`ShaderMake.cfg:1`) | PASS |

**8/8 PASS.**

## Supporting evidence gathered this tick (not assertions — direct reads)

- `GIPathTracing.hlsl:496-499` — Phase-D half-res comment + gbScale/gbPixel derivation.
- `GIPathTracing.hlsl:501-503` — production reads use `gbPixel` (contrast with old probes).
- `Binary/Debug/TestReSTIR_GI_Temporal.log:200` — `OutputW=400 OutputH=300`.
- `Binary/Debug/TestReSTIR_GI_Temporal.log:198` — `viewport 800x600`.
- `DeviceManagerVk4_LifeCycle.cpp:118` — validation layer IS hooked up in device
  creation (`:198` nullptr is teardown), so the 0-VUID reading on the current log
  is sound rather than vacuous.

## Rows NOT run (terminal required)

Build; `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` run; `HLVM_PT_DEBUG_MODE=20` re-measure;
`validate_restir_gi.py`; vision inspection of the fresh display PNG. See audit.
