# Pending Tests v207

- commit: docs/PENDING_COMMIT_v207.md
- tester: agent_5_tester (tick-553)
- mode: **file-only**. Terminal is denied categorically in this runspace —
  probed twice this tick including a bare `true`, both refused with
  `tirith:unknown`. Nothing below was built, run, or rendered.

## Verification rows

| # | Row | Method | Result |
|---|-----|--------|--------|
| 1 | u2 fallback is the mandatory output UAV | read `FGIPass.cpp:645-647` in place | **PASS** — ternary `Desc.OutputDirection ? … : Desc.OutputTexture` |
| 2 | Exactly one `setTextureState` for u2, unconditional | read `:648-649` | **PASS** |
| 3 | All three UAV slots still bound | `SetTextureUAV` → **3** (`:603`, `:631`, `:650`) | **PASS** |
| 4 | Layout↔set pairing N-for-N | `AddTextureUAV` → **3** (`:314/315/316`) vs row 3 | **PASS** — 0,1,2 both sides |
| 5 | u1 block not damaged by the adjacent deletion | read `:605-631`; `DummyDesc` → **10**, all inside it | **PASS** |
| 6 | u2's dummy creation fully removed, not partially | `DummyDirection` → **1** in the `.cpp` (`:192` `Shutdown` null-out only) | **PASS** |
| 7 | Fallback operand cannot be null | `!Desc.OutputTexture` → guard at `:530`, same function, 115 lines above, no reassignment | **PASS** |
| 8 | Shader copy A byte-unchanged | `OutputDirection\[pixel\]` → 1 hit at **`:645`** in `Private/Renderer/Shader/GI/` | **PASS** |
| 9 | Shader copy B byte-unchanged | same query → 1 hit at **`:645`** in `TestReSTIR_GI_Temporal_Data/` | **PASS** |
| 10 | Header member retained, not deleted | `DummyDirectionTexture` → 1 hit `FGIPass.h:140` | **PASS** — dead but present, per the deviation policy |
| 11 | **Enumeration row (added at the plan gate)** — consumer 1 | `TestReSTIR_GI_Temporal`: `OutputTexture` created `HalfW, HalfH` (`:1675-1677`); `Desc.OutputWidth = HalfResWidth` (`:815`); both ← `W = WIDTH` (`:1616`) | **PASS** — match by shared constant |
| 12 | **Enumeration row** — consumer 2 | `TestPathTraceGI`: `OutputTexture` created `WIDTH, HEIGHT` (`:265-267`); `Desc.OutputWidth = CurrentFBInfo.width` (`:438`); equal only via `Resizable = false` + `Extent = { WIDTH, HEIGHT }` (`:1499-1500`) | **PASS** — match, but by a *different* mechanism |
| 13 | Header states a caller requirement, not an observation | read `FGIPass.h:37-49` in place | **PASS** — "MUST be sized to OutputWidth x OutputHeight" |
| 14 | Header does not contradict the sibling SRV contract | read `:46-49` | **PASS** — explicitly contrasts: writes pinned, reads scaled |
| 15 | No test files produced | commit declares `produces_test_files: no`; diff touches 2 non-test files | **PASS** — HARD INVARIANT #2 satisfied |

## Query hygiene (lineage rules applied)

- **No `|` alternation** in any pattern (tick-526). One term per query throughout.
- **Every zero controlled by a same-shape positive.** Row 6's `DummyDirection` → 1
  is controlled by `DummyDesc` → 10 in the same file, same tick: the file
  demonstrably still contains dummy-creation code, so the 1 is a real narrowing
  and not a broken query.
- **No `path` at a file for a load-bearing negative** (v199); directory or
  in-place `read_file` used for every conclusion.
- **No count quoted from another marker** — rows 3, 4, 6 re-derived this tick.
- **Rows 1, 2, 5, 13, 14 are `read_file` in place**, not greps, because they are
  claims about *what the code says*, which a count cannot establish.

## The row that matters most, and why it is two rows

Rows 11 and 12 are deliberately **not** merged. The fix's in-bounds argument
rests on "the mandatory UAV is dispatch-sized", and that holds in the two
consumers **for two unrelated reasons** — one by a shared compile-time constant,
one by a non-resizable window. Merging them into a single "both consumers match"
row would report a contract where there is a coincidence twice over, which is the
precise error the plan gate returned FIX for. **Stated separately, the rows also
document the fragility**: flipping `TestPathTraceGI` to `Resizable = true` breaks
row 12 without touching any line of this cycle's diff.

## Not established

Nothing was compiled, linked, run, rendered, validated, or viewed. **No acceptance
gate is exercised by these rows.** They establish that the source change is what
the marker says it is, and nothing about runtime behaviour.
