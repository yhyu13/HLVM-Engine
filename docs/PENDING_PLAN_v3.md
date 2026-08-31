# Pending Plan v3 — verify v2 fix + extend handle-identity diagnostic

- task: Test the v2 fix (revert of the v22 split, single binding set, HLSL `space1`
  removed) on a fresh build. If v2 worked, this card closes; if not, extend the
  per-frame handle-identity diagnostic to ALL frames and add a GPU-side
  sentinel-compare debug mode that distinguishes "texture handle mismatch" from
  "binding descriptor mismatch" in a single dump.
- source: docs/DIAGNOSTIC_2026-07-30.md (root-cause hypothesis tree)
          + docs/PENDING_COMMIT_v2.md (the v2 fix already applied)
          + direct file-only inspection of FGIPass.cpp + TestCornellBoxGI.cpp.
- approach:
  1. **Verification of v2 (decision branch):** operator runs the rebuild +
     HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 invocation and
     reports whether `dumps/.../gi_raw_frame8.png` shows non-zero per-channel
     std. If yes: v2 worked, card closes. If no: v2 didn't fix it, proceed to
     step 2.
  2. **Decisive experiment if v2 didn't fix it — handle-identity on every
     frame + sentinel-compare debug mode.** Per DIAGNOSTIC_2026-07-30.md
     "Recommended next step" (the cheapest probe):
     - Change the existing `if (Desc.FrameIndex < 4u)` gate at
       `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:547` to
       `if (Desc.FrameIndex < 64u)` so we get the handle log for all 8
       accumulated frames, not just the first 4. Same change at
       `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:2226` (the
       matching RenderGBuffer log).
     - Add a new debug mode `HLVM_PT_DEBUG_MODE=23` in
       `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`
       (and the test-data-dir copy) that:
         - reads `GBufferWorldPos[pixel]` AND
         - reads the same pixel via a small constant write into the OutputTexture
           from the host code (CPU-uploaded magenta at pixel 0,0 sentinel)
         - outputs (GBufferValue.rgb, sentinel_flag).
       If the SRV returns garbage (handle mismatch), the sentinel flag flips;
       if the SRV returns zero (descriptor mismatch), the GBufferValue is zero.
       Both cases distinguishable in one dump.
  3. **Run, eyeball, decide.** Per the methodology (skill
     § Path-Tracing/RT Debugging), "trust measurements, not code reading."
     The handle-id logs at frames 0-7 + the sentinel-compare dump together
     pin down which of the two remaining hypotheses (handle mismatch vs
     descriptor mismatch) is the cause.
- diff_estimate: ~+15 / -5 lines if step 2 is needed; +0 if v2 worked (just verify).
- skip_plan_review: no (the v2 commit went through review and KEEP'd, but the
  outcome is unknown without operator build; this card is the verification + the
  contingency, both need review because v2's KEEP was based on the assumption
  the operator would run the build, which has not yet happened).
- test_strategy: Operator-side only (terminal blocked in cron profile). The plan
  itself produces two files of debug output for the operator to inspect:
  (a) the [handle-id] log lines for frames 0-7 — match between RenderGBuffer
  and DispatchRays = binding is wrong; mismatch = handle identity issue;
  (b) the HLVM_PT_DEBUG_MODE=23 sentinel-compare dump.
- risks:
  - **The cron profile has `terminal` blocked by tirith** (per the 2026-07-03
    empirical finding recorded in software-development-practices §
    "Empirically verify what subagents can do"). The plan can stage the code
    change but the build/run/dump-inspection cycle MUST be executed by the
    operator at the keyboard. This is the same constraint that left v2
    unverified.
  - If the operator reports v2 worked, no further code changes are needed
    and the card closes.
  - If step 2 (handle-id extension) is implemented, the `if (FrameIndex <
    64u)` gate is a DEBUG-only change and should be reverted before any
    production commit. Stage it as a v3 commit; revert to v2 if v2 worked.
- acceptance criteria:
  1. Operator runs rebuild + dumps and reports whether v2 fixed the bug.
  2. If yes: card closes in PENDING_PICK.md, no further commits.
  3. If no: handle-id logs + sentinel-compare dump from step 2 produce a
     single deterministic answer to "is this handle mismatch or descriptor
     mismatch". The next card (v4) implements the fix.

## Why this card is the verification, not a new fix attempt

v2 IS the fix attempt. It landed cleanly (per PENDING_IMPL_REVIEW_v2.md
KEEP verdict + PENDING_TEST_AUDIT_v2.md ALL_KEEP verdict). The plan-criticer
and reviewer both signed off. But neither role can run the build, and the
tester role also cannot run the build. ALL_KEEP in file-only mode means
"mechanically sound + would catch a regression if run", NOT "fix
verified to work in a fresh run". The skill warns about this:

> "single-profile deployment without explicit caveat ... the freshness
> guarantee of the planner/impler split and the plan-criticer/reviewer
> split collapses to 'same head with different prompt text.' The reviewer
> cannot catch the planner's biases because it IS the planner. The
> testing-verifier cannot run pytest when shell is blocked (file-only
> mode). Bake this caveat into the dispatcher prompt and weight reviewer
> verdicts accordingly."

This card exists to make the "tester ran the test" verification step
EXPLICIT and to plan the contingency for the case where v2 didn't fix it.

## What this card does NOT do

- Does NOT propose a code-level fix beyond extending diagnostic logging.
  The fix (if v2 didn't work) is the next card (v4), which would propose
  specific code changes (e.g., enable Vulkan validation layer properly,
  or refactor to use `nvrhi::BindingSetDesc` directly instead of the
  FBindingSetBuilder wrapper if the wrapper is the bug).
- Does NOT commit anything to git. v2's commit was uncommitted
  ("working tree (cron tick v2; not committed)") — that's consistent
  with this card's "wait for operator verification before committing
  anything."
- Does NOT touch governance files (per the user instruction "Do not
  commit, push, or modify governance files").
