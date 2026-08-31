# Pending Tests v187

- commit: docs/PENDING_COMMIT_v187.md
- tester: agent_5_tester (tick-534)
- timestamp: 2026-08-30
- mode: **file-only static verification.** `terminal` denied by tirith, so no
  build / compile / run / image. Every row below is a query I actually executed
  this tick and the result is transcribed, not predicted.

## Verifier rows

| # | Check | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | Cornell spatial declares `GBufferScale` | `pattern="GBufferScale"` on the file | 2 hits: `:20` (comment), **`:33` (declaration)** | PASS |
| 2 | It is a scalar, followed by scalar `Pad` | full read `:32-34` | `float GBufferScale;` / `float Pad;` / `};` | PASS |
| 3 | No vector pad left in any ReSTIR struct | `pattern="float2 Pad;"` over Runtime | 2 hits, both `TestSponzaDeferred_Data` (`SSAOBlur_cs.hlsl:9`, `ExposureAdaptation_cs.hlsl:12`) — neither is a ReSTIR struct | PASS |
| 4 | Three-way agreement with the shared header | `FReSTIRPass.h:70-72` vs Cornell `:32-33` vs Temporal `:25-26` | header `DebugVis;GBufferScale;Pad;` — Temporal copy `float GBufferScale;` at `:25` — Cornell now identical in order and kind | PASS |
| 5 | **No un-initialized ReSTIR struct remains in Cornell** | `pattern="ReSTIR::FReSTIR\w+Constants \w+;"` (i.e. a declaration with NO `{}`) on `TestCornellBoxGI.cpp` | **0 hits** | PASS |
| 6 | …and all three are positively value-initialized | `pattern="FReSTIRSpatialConstants "` / `"FReSTIRTemporalConstants "` over Runtime | `TestCornellBoxGI.cpp:1607` `SpatConstants{}`, `:1556` `TempConstants{}`, `:1513` `GenConstants{}`; Temporal test `:1042`/`:961` already `{}` | PASS |
| 7 | `GBufferScale` explicitly assigned at the Cornell call site | read `TestCornellBoxGI.cpp:1622` | `SpatConstants.GBufferScale = 1.0f;` | PASS |
| 8 | The `1.0f` is correct for this call site | read `:1631-1632` | `SpatDesc.OutputWidth/Height = CurrentFBInfo.width/height` — full res, same as the GBuffer MRTs sampled → ratio exactly 1 | PASS |
| 9 | **Edited shader is on a compiled path** (the v182 trap) | read `TestCornellBoxGI_Data/ShaderMake.cfg` | `:7 ReSTIR_Spatial_cs.hlsl -T cs` | PASS |
| 10 | Scope fence: Cornell did NOT become half-res-aware | `pattern="int2 GB"` and `pattern="gConstants.GBufferScale"` over `TestCornellBoxGI_Data` | **0 hits each** | PASS |
| 11 | v183/v184/v185 chain unperturbed | `pattern="GBufferScale"` over Runtime, inspect paths | all `TestReSTIR_GI_Temporal*` hits unchanged (`:1005`, `:1051`, shader `:25`,`:54`, `:42`,`:80`) | PASS |
| 12 | Marshaller untouched | `FReSTIRPass.cpp:547` | still `ConstantsData[offset++] = Constants.GBufferScale;` — writes float 9 as before | PASS |

12/12 PASS.

## Row 5 is the discriminating row

Rows 1-4 would pass on a patch that fixed only the shader — the regression the
plan warned about. Row 5 is the one that fails on that patch, because it searches
for the *absence* of `{}` rather than the presence of the field. It returned 0
hits, so the coupled half actually landed.

Row 10 is its mirror: it fails if someone "helpfully" added a `GB()` helper to
Cornell, which would read a field this test dispatches at scale 1 and turn an
inert alignment into a live behaviour change.

## Deliberately NOT claimed

No compile, no build, no run, no dump, no image, no validator. Rows 1-12 are
static text properties of the tree. They establish that the declarations agree
and that no indeterminate read remains reachable from the C++ side; they
establish **nothing** about whether slangc accepts the struct or whether
`TestCornellBoxGI` still renders correctly. That needs the operator run in
`PENDING_COMMIT_v187.verify`.
