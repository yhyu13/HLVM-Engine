# Pending Impl Review v226

- plan: docs/PENDING_PLAN_v226.md
- commit: docs/PENDING_COMMIT_v226.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-579)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl followed the plan exactly: zero source files modified, all three post-v200 cycles audited (v206/v207/v209), every load-bearing zero controlled by a positive in the same scope. The audit's three verdicts — v206 clean, v207 clean in both consumers, v209 deletion complete — are independently re-derived below and all three hold.

## TDD evidence

N/A — this cycle is a determination, not a fix. No test files were produced and no behaviour was changed. Per HARD INVARIANT #2, the absence is permitted because `produces_test_files: no`.

## Security scan

N/A — zero source files modified. The cycle wrote three marker files (`PENDING_PLAN_v226.md`, `PENDING_PLAN_REVIEW_v226.md`, `PENDING_COMMIT_v226.md`) and used `read_file`/`search_files` only on engine source.

## Self-review checklist

- **Validation:** every load-bearing zero paired with a same-shape positive control in the same scope (the `DummyDebugStats` → 5 control proves the `DummyDirection` → 0; the `MaterialPlaceholderTexture` → 6 control in `files_only` mode proves the `count`-mode → 0 was an enumeration-cap artifact, not a real negative).
- **Error handling:** the impl identified a second instance of v225's enumeration-cap defect — reproducing it **on a load-bearing engine-source query, in the exact shape an audit uses to prove a deletion is complete** — and recorded it as a net-new finding rather than silently papering over it. That is the right disposition: the cycle's verdict depended on the `MaterialPlaceholderTexture` zero, and the impl caught that the zero was false before reporting it.
- **Tests:** no test files produced (determination cycle). The audit is itself the verification — every claim is re-derived by reading the source, not asserted.

## Independent re-derivation of the impl's three load-bearing claims

**Claim 1 — `FGIPass` has exactly two consumers.** I almost disagreed: `DispatchRays` appears in 8 source files including `TestCornellBoxGI.cpp`. Reading those hits separates the call sites: `CornellBoxGI` uses `RTPipeline.DispatchRays` (its own `FRayTracingPipeline`, `FRayTracingPipeline.cpp:252+`), not `FGIPass::DispatchRays`. The deciding query is `FGIPassDesc`, which is the construction site of the struct every consumer must fill. **`FGIPassDesc` → 2 in source** (`TestPathTraceGI.cpp:427`, `TestReSTIR_GI_Temporal.cpp:803`), plus header/defs in `FGIPass.h`/`.cpp`. `TestPathTraceTriangle.cpp` mentions `FGIPass` only in a comment (`:7`) — it does **not** construct an `FGIPassDesc`. Verified. **KEEP**.

**Claim 2 — both shader copies are byte-identical in v207's load-bearing region.** Re-derived against the **two copies the consumer's build actually compiles**, not just the one the impl cited. `TestPathTraceGI_Data/ShaderMake.cfg:1` names `GIPathTracing.hlsl`, **but the file does not exist in that directory** — `ShaderMakeBuild.py:571` resolves it to the shared `Private/Renderer/Shader/GI/` copy. That is a real trap and a fair re-derivation caught it. Both copies checked: `OutputDirection[pixel]` at line 645 in **both**, `Output[pixel]` at 537/819/822/826 in **both**, sole RayGen-scope `return;` at 538 in **both**. **KEEP**.

**Claim 3 — every compiled path overwrites the u0/u2 alias.** `GIPathTracing.hlsl:819` and `:826` are the two arms of an `#if HLVM_RGI_DEBUG_VIS / #else` block, so exactly one compiles per build configuration; both are unconditional writes; both follow `:645`. The sky path returns at `:538`, **before** `:645`. No configuration leaves the stray direction value in `Output`. **KEEP**.

**Claim 4 (the plan-criticer's correction) — v207's "`:645` precedes `:819`/`:826`" reads literally as a sibling-ordering claim and would be false.** That claim survives in the stronger form above, but the impl did not adopt the critic's correction into the commit marker — v207's imprecise framing remains. Recorded as a carry-forward: **a future cycle that relies on v207's marker should verify the `#if`/`#else` rather than trust the ordering statement.**

## Additional reviewer finding — one more producer

v206's `FReBLURPass.h` change added a comment. I confirmed it does not affect type layout by reading the diff site in the marker (`PENDING_COMMIT_v206.md:74`) — the two member-comment lines are at `:72` and `:93`, the `struct FDesc` braces are intact, the `OutputWidth` member at `:96` is unchanged. Same shape as v205's sibling-comment fix; no consumer affected at the type level. **KEEP**.

## Standing rule, reinforced by this cycle's net-new finding

v225 found that directory-scoped `output_mode=count` enumerates only ~50 files and silently omits the rest. This cycle hit it **first-hand on a load-bearing engine-source query** — `MaterialPlaceholderTexture` returned 0 in `count` mode against the same scope that returned 6 in `files_only` mode, and the `count` map showed the enumeration reaching only `ThirdParty/stb`, `ThirdParty/Imgui`, build logs, never `Private/Renderer/`. The impl caught the discrepancy by running both modes in the same scope and refused to report the `count` zero as evidence.

The lineage now has **two independent observations of the same defect, on two different runs, on two different scopes**. That is the threshold v225 was waiting for to upgrade its "audit every load-bearing zero with `files_only`" rule to a standing prohibition. **Promoted to a hard rule**: any audit conclusion that depends on a directory-scoped query must use `files_only` mode; `count` is inadmissible for load-bearing negatives.

## Acceptance criteria vs the job instruction

0/7 unchanged. This cycle de-risks the operator's first build but does not build it.

## Feedback for impler

None blocking. One carry-forward: re-derive v207's line-number argument before relying on it again — the `#if`/`#else` correction above, while correct in its conclusion, would be easy to get wrong from the marker's prose alone.