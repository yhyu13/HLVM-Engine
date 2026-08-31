# Pending Plan Review v3 — verification + contingency for v2

- plan: docs/PENDING_PLAN_v3.md
- verdict: KEEP
- reviewer: planner+plan-criticer (single-profile host; same head)
- timestamp: 2026-08-17 (estimated wall-clock; cron session)

## Design soundness

The plan correctly identifies that v2 was a **mechanically sound fix that
was never verified**, and the next logical step is:

1. **Verify v2 first** (cheap — single build + dump inspection by the
   operator). If it worked, the card closes; this is the simplest
   possible outcome.
2. **Only if v2 didn't work**, do further code changes (the handle-id
   extension + sentinel-compare debug mode). This is the methodology's
   principle: "**Trust measurements, not code reading.**" Running the
   existing v2 first is the cheapest measurement.

The handle-identity diagnostic already exists in the code
(`FGIPass.cpp:547` and `TestReSTIR_GI_Temporal.cpp:2226` both have the
`if (FrameIndex < 4u)` gate). The plan proposes extending it to all 64
frames so we get logs for the 8 accumulated frames. This is correct: the
bug might manifest on a specific frame, not the first one.

The HLVM_PT_DEBUG_MODE=23 sentinel-compare is the **decisive experiment**
per DIAGNOSTIC_2026-07-30.md's "Recommended next step" — separating
"texture handle mismatch" from "binding descriptor mismatch" in a
single dump is the right next move.

## Plan completeness

- ✅ Identifies the verification step (operator runs the rebuild + dump).
- ✅ Identifies the contingency code change (handle-id extension).
- ✅ Identifies the decisive experiment for the contingency
  (sentinel-compare debug mode).
- ✅ Stops short of proposing a fix for the contingency case — that's
  v4's job, gated on the operator's v2 verification result.
- ⚠️ Does NOT address what to do if even v3 (handle-id extension +
  sentinel-compare) doesn't pin down the bug. The skill's "rule of
  thumb" applies: this is interactive debugging, not pipeline work;
  each cycle adds latency without fresh-eyes benefit on a single-
  profile host.

## Risks acknowledged

1. **Terminal blocked by tirith in cron profile.** Confirmed: every
   `terminal` invocation in this session was blocked with
   `"User denied this command"` error from tirith. The plan correctly
   routes all build/run/inspect actions to the operator at the keyboard.
2. **v2 was KEEP'd by reviewer + audit-verifier, but neither ran the
   test.** This is the single-profile deployment caveat baked into the
   skill. The plan acknowledges it ("ALL_KEEP in file-only mode means
   'mechanically sound', NOT 'fix verified'").
3. **The handle-id extension is debug-only.** Step 2's change to
   `if (FrameIndex < 64u)` should NOT ship to production. If the
   operator verifies v2 worked, this change never lands.
4. **Sentinel-compare debug mode uses CPU-uploaded sentinel.** Per the
   skill's "Sentinels-then-overwrite" gotcha, sentinels can mask real
   GPU writes if left enabled in shipping code. The mode 23 sentinel
   must be gated behind `HLVM_PT_DEBUG_MODE=23` (off by default) and
   the CVar / env var check pattern already used in the file.
5. **HLSL dual-copy drift risk.** Adding mode 23 requires editing both
   `GIPathTracing.hlsl` copies (the private canonical AND the
   test-data-dir copy that's actually compiled). The plan doesn't
   emphasize this — adding it as a concern below.

## Concerns flagged

1. **HLSL sync drift:** step 2's mode 23 must be added to BOTH
   `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`
   AND `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`.
   A one-side edit would cause a silent binding/SPIR-V mismatch. The
   plan should explicitly call this out. (It currently says "the test-
   data-dir copy" — that's correct, but the wording is brief.)

2. **HLVM_PT_DEBUG_MODE=23 is a high mode number — what's already
   taken?** Modes 0..22 are in use per the diagnostic. Mode 23 is
   available. No conflict.

3. **The plan doesn't gate step 2 on step 1's outcome.** Reading the
   plan carefully, step 2 IS contingent on v2 not working (it's in the
   "If v2 didn't fix it" branch). The wording could be clearer but
   the intent is right.

## Feedback for planner (FIX only)

None — verdict is KEEP. Plan v3 is approved with the caveats noted.
Move to impler for the verification step + contingency.

## Next role

Impler (file-only): wait for operator verification of v2. If operator
reports v2 worked → close card. If v2 didn't work → apply the handle-id
extension + add mode 23 sentinel-compare, stage as PENDING_COMMIT_v3.md.

CRITICAL: impler cannot run `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal
--Test` itself (terminal blocked). The impler's deliverable is the
**code change** for the contingency; the build/run/inspect is the
operator's deliverable. This is the file-only mode the skill explicitly
describes.
