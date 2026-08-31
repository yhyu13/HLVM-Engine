# Pending Tests v183

- commit: docs/PENDING_COMMIT_v183.md
- tester: agent_5_tester (tick-530)
- timestamp: 2026-08-30
- test type: file-only structural verifier (no test FILES produced —
  `produces_test_files: no`; this runspace cannot execute anything)

## Why a file-only verifier

Terminal is denied by tirith for every command including `pwd`, and there is
no vision tool (tick-528). A build, a slangc compile, a GPU run, and an image
check are all unreachable. Every row below is a static check executed with
`read_file` / `search_files` this tick. Per tick-526's rule, no `|`
alternation is used in any pattern and every `path` points at a directory.

## Rows (10) — all executed this tick

| # | Check | Method | Result |
|---|-------|--------|--------|
| 1 | Temporal converts all 4 full-res loads | `Load` enumeration in `ReSTIR_Temporal_cs.hlsl` | **PASS** — `:129` `gDepth`, `:172` `gPrevDepth`, `:173` `gPrevNormals`, `:174` `gNormals` all wrapped in `GB(...)` |
| 2 | Spatial converts all 4 full-res loads | same, `ReSTIR_Spatial_cs.hlsl` | **PASS** — `:80`/`:116` `gNormals`, `:81`/`:117` `gDepth` all wrapped |
| 3 | **Negative control** — no half-res load converted | same enumeration, inverted | **PASS** — temporal `:117`,`:118`,`:195`,`:196` and spatial `:83`,`:84`,`:128`,`:129` are reservoir loads, all still raw `pixel`/`prevPixel`/`nPixel` |
| 4 | Exactly 8 loads per shader (no site missed, none invented) | count | **PASS** — 8 and 8; 4 wrapped + 4 unwrapped in each |
| 5 | `uv`/`prevPixel`/bounds stay half-res | `read_file` temporal `:130`,`:164`,`:170`; spatial `:113` | **PASS** — all unchanged, still against `RcpOutputSize`/`outputSize` |
| 6 | Guard present in BOTH copies | `search_files pattern="GBufferScale"` | **PASS** — 4 hits: decl `Temporal:36`, guard `Temporal:74`, decl `Spatial:25`, guard `Spatial:54`; both guards are `max(int(...), 1)` |
| 7 | Helper declared before first use | `search_files pattern="int2 GB"` vs first call line | **PASS** — `Temporal:72` < first use `:129`; `Spatial:52` < first use `:80` |
| 8 | Struct size unchanged, field order matches C++↔HLSL | `read_file` `FReSTIRPass.h:31-59` vs `Temporal:22-37` / `Spatial:16-27` | **PASS** — temporal `…PrevSceneYaw, Pad[2], GBufferScale` on both sides; spatial `…DebugVis, GBufferScale, Pad` on both sides; 3-float and 2-float tails preserved |
| 9 | **Scope control** — CornellBox copies untouched | `search_files path=TestCornellBoxGI_Data pattern="GBufferScale"` | **PASS** — 0 hits; that test has no Phase-D half-res path |
| 10 | Both shaders are actually compiled | `read_file ShaderMake.cfg` | **PASS** — `:6` `ReSTIR_Temporal_cs.hlsl -T cs`, `:7` `ReSTIR_Spatial_cs.hlsl -T cs`; the patch is not dead code |

10/10 PASS.

## Rows 3, 4 and 9 are the real discriminators

Rows 1-2 only confirm the intended edits landed. The rows that could actually
have failed:

- **Row 3** would fail if the impler had over-applied `GB()` to reservoir
  loads. That is the damaging mistake — it corrupts reprojection while
  superficially resembling the fix.
- **Row 4** would fail if a full-res load existed outside the plan's list, or
  if the helper had been applied somewhere invented.
- **Row 9** would fail on scope creep into a currently-passing sibling test.

### Rows 11-13 added post-report (cbuffer marshalling — the row set that was MISSING)

The original 10 rows verified the shaders and the header, but **never checked
that the new constant actually reaches the GPU.** That gap let an inert patch
pass 10/10. Added:

| # | Check | Method | Result |
|---|-------|--------|--------|
| 11 | Temporal marshaller writes `GBufferScale` | `read_file FReSTIRPass.cpp:424-450` | **PASS** (after fix) — `:450`; was absent, marshalling stopped at `PrevSceneYaw:442` |
| 12 | Spatial marshaller writes `GBufferScale` | `read_file FReSTIRPass.cpp:518-543` | **PASS** (after fix) — `:543`; was absent, stopped at `DebugVis:530` |
| 13 | Temporal `Pad[0]`/`Pad[1]` (near/far) marshalled | same | **PASS** (after fix) — `:448-449`; **were absent, a pre-existing bug** |

**Row 11/12 are the rows that would have caught the inert patch.** Lesson for
future cycles: when a cbuffer is marshalled field-by-field rather than
`memcpy`'d, "field declared in the header" and "field visible to the shader"
are different claims, and only the second one matters. Any row set that
verifies a new shader constant MUST include a marshaller row.

## What these rows do NOT establish

They establish that the patch is **internally consistent, in scope, compiled,
and correctly partitioned**. They establish nothing about runtime behaviour.

Specifically NOT established:
- that the shaders compile under slangc (no compiler available);
- that `M mean` rises above 2.93 (no run);
- that the display image improves (no run, no vision);
- that `validate_restir_gi.py` passes (no python3).

Any future tick claiming acceptance gate 1, 2, 5, 6 or 7 on the basis of this
patch, without a build/run log dated after 2026-08-30, is fabricating.
