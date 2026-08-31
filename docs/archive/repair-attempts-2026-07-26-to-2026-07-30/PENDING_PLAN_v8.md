# Pending Plan v8 — complete documentation drift cleanup (round 2)

- task: clean up the v4a diagnostic comment at TestReSTIR_GI_Temporal.cpp lines 1685-1691 that v7 missed — it still references the HLVM-bypass close+execute+waitForIdle+open flow as a candidate v4b fix, but v5 already removed that bypass and the comment was left dangling
- source: no bundle
- approach: replace the 4-line stale diagnostic comment block with a v4a/post-v5 version that accurately documents what the diagnostic should now correlate against. The comment's job is: "if gi_raw = 0 after v5's bypass removal, the GI pass write was dropped for some OTHER reason — look at v3's ENTER/EXIT/binding-set logs and the v5 NOTE near line 1521". Pure text replacement. No behavioral change. No test surface change.
- diff_estimate: +6 / -5 lines (net +1)
- skip_plan_review: no — comment touches diagnostic-log correlation documentation; needs plan-criticer sign-off
- test_strategy: no new tests; v5/v6/v7 patches unchanged; validator and log-shape acceptance from earlier cycles still apply unchanged
- risks: minimal — pure text replacement of non-executing diagnostic comment; no code path changes; no API surface changes; no shader changes. If the comment is wrong, parent will see a typo (cosmetic only).

## Rationale

v7's PIPELINE_HEALTH tick applied one documentation drift fix (lines 650-672). It missed the v4a diagnostic comment at lines 1685-1691 that still references "the HLVM-bypass close+execute+waitForIdle+open flow at lines 1516-1531, which is the v4b candidate fix". After v5 removed that bypass, the v4a comment is doubly stale: (a) it points to a flow that no longer exists, (b) it describes a v4b fix that v5 already applied differently.

The v4a diagnostic still has value: if gi_raw=0, parent's next debug step is to look at v3's FGIPass::DispatchRays ENTER/EXIT/binding-set logs to determine if the dispatch body was reached. The comment needs to point at the correct evidence chain post-v5.

## What v8 explicitly does NOT do

- Does NOT change any code, shader, binding layout, or pipeline behavior.
- Does NOT add or remove tests.
- Does NOT change the validator, the test harness, or the build system.
- Does NOT trigger any v6 sub-plan (a/b/c/d). Those are still gated on parent's v5 verification.
- Does NOT commit or push (cron rules).
- Does NOT make any claim about the renderer's correctness post-fix. The renderer is still in the same state it was when v5's patch landed; this patch only updates stale diagnostic documentation.

## Honest caveat

If parent runs the test after this patch, the renderer output will be IDENTICAL to what they would have seen with v5/v6/v7 alone (the comments are non-executing text). The pipeline is still at v6 audit SOME_RELAX awaiting parent verification of v5's actual code patch.

## Files this cycle will touch

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (comment-only update at lines 1685-1691)