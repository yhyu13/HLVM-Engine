# Pending Tests v198

- commit: docs/PENDING_COMMIT_v198.md
- role: agent_5_tester (tick-544)
- mode: **file-only**. Terminal denied (`tirith:unknown`) on 2 probes this tick. No build,
  no run, no image. Every row below is a `search_files` or `read_file` I executed this tick.

Query discipline in force: single-term patterns only (tick-526 alternation); `path` at a
directory, never a file; every zero paired with a same-shape positive (v192/v196); widen
before believing a zero (v197 assumption-encoding).

| # | Row | Query / read | Result | Verdict |
|---|---|---|---|---|
| 1 | No source file was modified | `v198` in `Engine/Source/Runtime/Test` | **0** | PASS |
| 2 | Control for row 1 | `v198` in `docs` | **4** markers | PASS |
| 3 | Control target untouched | `v198` in `TestPathTraceGI.cpp` | **0** | PASS |
| 4 | Generation dispatch is swapchain-sized | `GenDesc.OutputWidth` | 1 hit `:1531` `= CurrentFBInfo.width` | PASS |
| 5 | Temporal dispatch is swapchain-sized | read `:1608-1609` | `TempDesc.OutputWidth/Height = CurrentFBInfo.*` | PASS |
| 6 | Spatial dispatch is swapchain-sized | read `:1654-1655` | `SpatDesc.OutputWidth/Height = CurrentFBInfo.*` | PASS |
| 7 | Reservoir targets created once | `Reservoir0Texture` | 9 hits, one `createTexture` `:968` | PASS |
| 8 | Merged targets created once | `Reservoir0MergedTexture` | 7 hits, one `createTexture` `:976` | PASS |
| 9 | Output target created once | `ReSTIROutputTexture = NvrhiDevice` | 1 hit `:983` | PASS |
| 10 | Radiance target created once | `TemporalRadianceTexture` | 6 hits, one `createTexture` `:985` | PASS |
| 11 | Prev-normal created once | `PrevNormalTexture = NvrhiDevice` | 1 hit `:1008` | PASS |
| 12 | Resize block membership | `createTexture` | 32 hits; 9 in `:1160-1256`, none a reservoir/radiance/prev/ReBLUR | PASS |
| 13 | Window is resizable | `Resizable` | 1 hit `:1917` `= true` | PASS |
| 14 | Guard is tautological (temporal) | read `ReSTIR_Temporal_cs.hlsl:63-64` | `>= gConstants.OutputSize` | PASS |
| 15 | Guard is tautological (spatial) | read `ReSTIR_Spatial_cs.hlsl:58-60` | `>= outputSize` | PASS |
| 16 | Primary target enumeration still clean | `FB\.width` in `Test` | 16 hits, 3 live, all intentional | PASS |
| 17 | Neither shader copy touched | `v198` in `TestReSTIR_GI_Temporal_Data` | **0** (control: row 2) | PASS |

**17/17 PASS.**

## Row 12 — the row that carries the cycle, and how I avoided fooling myself

Row 12 is a claim about an **absence**, and absences are exactly what a grep cannot assert.
`search_files` returning "no reservoir createTexture inside the resize block" is not a query
I can write; what I actually did was take all 32 `createTexture` hits, read the line numbers,
and partition them against the resize block's range, which I established by reading
`:1160` (the `if`) and `:1256` (the closing brace) rather than assuming.

I state this because the v197 audit's lesson was that a query encoding a false assumption
returns a clean zero. **The defence against that here was not a better query — it was
refusing to use a query at all for the load-bearing step.** Rows 7-11 are the queries; row 12
is a read. Anyone re-running this should re-read the block bounds, because if the block's
extent is wrong the partition is wrong and every row above still passes.

## Row 16 — deliberately included

Not about this cycle's change. It re-establishes that v197's closure of card I for the
primary target still holds after this cycle touched nothing, which is the only way to
distinguish "we changed nothing" from "we changed nothing *and* nothing else moved."

## What these rows do NOT establish

That anything compiles, links, runs, renders or validates. **0/7 acceptance gates.** These
rows establish a source-level determination about a defect in a target that was not patched.
No runtime claim is made anywhere in this marker.
