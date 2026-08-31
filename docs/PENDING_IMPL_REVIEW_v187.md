# Pending Impl Review v187

- plan: docs/PENDING_PLAN_v187.md
- commit: docs/PENDING_COMMIT_v187.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-534)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan on all six acceptance criteria, re-checked against the
patched files rather than against the diffs:

1. `TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:32-33` declares `GBufferScale`
   then scalar `Pad`, matching `FReSTIRPass.h:70-72` and the Temporal copy. ✔
2. `search_files pattern="float2 Pad;"` over Runtime → 2 hits, both in
   `TestSponzaDeferred_Data`, neither a ReSTIR struct. ✔
3. All three Cornell declarations carry `{}` (`:1513`, `:1556`, `:1607`). ✔
4. `SpatConstants.GBufferScale = 1.0f` at `:1622`, justified from `:1631-1632`. ✔
5. No `GB(` helper and no read of `gConstants.GBufferScale` in the Cornell
   shader — verified by full read of the 144-line file. ✔
6. `TestReSTIR_GI_Temporal_Data/` untouched. ✔

**Declared deviation assessed and JUSTIFIED.** The impler widened its check (not
its edit) to `TestReSTIR_GI_Temporal.cpp` before touching the shared header's
other consumer. That is precisely the v186 lesson applied — the header is shared,
and a one-sided patch relocates a mismatch instead of removing it. No edit was
needed there because it already value-initializes. This strengthens the claim
rather than expanding scope; it is the right kind of deviation.

## Defect found and fixed at this gate

The impler's new shader comment cited `TestCornellBoxGI.cpp:1620-1621` as the
dispatch-size site. The comment's own insertion shifted those lines: the real
site is now `:1631-1632`, and `:1620-1621` currently land in the middle of the
other comment the impler added — a pointer that was wrong the moment it was
written and would mislead the next reader.

Rewrote the reference to name `SpatDesc.OutputWidth/OutputHeight` symbolically
instead of by line number. **Line-number cross-references between two files that
are being edited in the same patch are inherently fragile**; symbolic references
survive. Corrected in place; no other line-number citation in the patch points
into an edited region (the `FReSTIRPass.h:70-72` and `FReSTIRPass.cpp:547`
citations point at unedited files, re-verified).

## Correctness of the central claim

I re-derived the offset argument rather than trusting it:

- HLSL packs `float2` as a vector at floats 9/10 (the v186 correction — arrays,
  not vectors, get fresh registers). Two scalars also occupy 9/10.
- `FReSTIRPass.cpp:535-547` writes offsets 0..9 and stops.
- Therefore the wire image is **byte-identical** before and after. The patch
  cannot move a Cornell pixel.

That is the property that makes this safe to land un-compiled, and it is real.

The indeterminate-read count of four is correct as tabulated: generation
contributes 0 because `:354-363` stops at `DebugVis`; temporal contributes 3;
spatial contributes 1.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no shell invoked)
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] Validation: `1.0f` is derived from the call site, not assumed; `max(int(s),1)`
      in the sibling shader means even a zero here would degrade to 1 rather than
      divide-by-zero — but this shader has no `GB()` at all, so the value is inert.
- [x] Error handling: unchanged; no new failure paths.
- [x] Tests: no test files produced → `produces_test_files: no` is accurate, and
      `skip_impl_review: no` was correctly set anyway (HARD INVARIANT #2 honored
      in spirit — this review ran).

## What remains UNVERIFIED (load-bearing)

slangc has not seen the edited struct. No build, no run, no image. `terminal` is
denied categorically by tirith — probes this tick returned
`status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`. The
mitigation (the post-patch form already compiles in the sibling file) is real but
is not a compile.

## Feedback for impler (FIX only)

None outstanding — the one defect was corrected at this gate.
