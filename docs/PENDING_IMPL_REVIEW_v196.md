# Pending Impl Review v196

- plan: docs/PENDING_PLAN_v196.md
- commit: docs/PENDING_COMMIT_v196.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-542)
- timestamp: 2026-08-30

## plan_fidelity_check

The plan proposed a determination with no source change; the impler produced
exactly that. **Zero source files modified**, re-verified: `TestPathTraceGI.cpp`
carries no `v196` token and no edit. The plan review's explicit instruction —
"if the impler finds itself editing `TestPathTraceGI.cpp`, the cycle has
drifted" — was honoured.

**One declared deviation (`§Plan Deviations`), and I endorse it.** The impler
spent leftover budget on re-scoped card I rather than ending early. That is a
scope *addition* with nothing dropped, and it is the only net-new finding this
cycle produced. Per the deviation policy, a deviation is a FIX trigger *unless
justified*; this one is justified on its face and produced value.

## Independent re-derivation

I re-derived the impler's two load-bearing claims rather than accepting them —
the standing rule from v195 applies to markers as much as to cards.

**Claim: the `gbScale` ratio is identically 1 in `TestPathTraceGI`.** Confirmed.
Both operands trace to `CurrentFBInfo`. Not v195's shape.

**Claim: `RenderGBuffer`'s parameters are unused.** Confirmed, and this is the
sharper form: the signature is literally `void RenderGBuffer(uint32_t /*W*/,
uint32_t /*H*/)` — the parameter *names* are commented out, which is the C++
idiom for "deliberately unused." The body's viewport is
`nvrhi::Viewport(0.f, float(WIDTH), 0.f, float(HEIGHT), ...)`. So the call site
passing `FB.width, FB.height` is inert **today**. The impler's characterisation
— "harmless today, a loaded gun" — is accurate, and its decision not to patch is
correct on both stated grounds.

**A third point the impler missed, which strengthens card J's closure.**
`TestPathTraceGI` contains an *internal inconsistency* that happens to be
harmless for the same reason everything else there is: the CPU reference render
calls `GetCameraProj(Rig, WIDTH, HEIGHT)` — the fixed constants — while
`UpdateViewConstants` and `FillGBufferTextures` call it with `W`/`H` from the
swapchain. Two call sites of the same projection helper, fed from two different
extent sources. **Under `Resizable = false` they are equal and the CPU reference
matches the GPU path; under a resizable window they would silently disagree and
the CPU-vs-GPU comparison — the methodology's rule-5 control — would be
comparing two different cameras.** This does not change the verdict (still not
reachable, still no patch), but it is worth recording: the non-resizability is
load-bearing for *more* of this target than the ratio analysis alone shows.

## Security scan

- [x] No hardcoded secrets — no source change
- [x] No shell injection — none
- [x] No eval/exec — none
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: candidate set closed at 12 `CurrentFBInfo` hits, each classified
- [x] Error handling: N/A — no code path added
- [x] Tests: `produces_test_files: no`; HARD INVARIANT #2 not engaged, and
      `skip_impl_review: no` so this gate ran regardless

## On the zero-diff outcome

**A cycle that correctly changes nothing is a success, and the pipeline should
record it as one.** The lineage has closed nine cards with nine substitutions,
and there is a real gravitational pull toward a tenth — the query shape matched,
the remedy is known, the edit is three characters. The plan declined it, the
plan gate tested the declining, and the impler held the line.

The counterfactual is worth stating plainly: had this cycle substituted
`WIDTH`/`HEIGHT` into the known-good control "for consistency," and had that
edit contained a typo, the resulting compile error would have surfaced on the
**first build of the entire unbuilt v183-v196 chain** and been indistinguishable
from a genuine defect in the nine cycles the control exists to exonerate.

## Feedback for impler

None — KEEP. Card K as written is well-scoped; note for whoever takes it that
the honest fix is a **signature change** (drop the two parameters), not a call
site substitution, since substituting arguments into unused parameters would
preserve the misleading appearance the card is about.
