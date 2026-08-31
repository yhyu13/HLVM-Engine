# Pending Plan v7 — complete documentation drift cleanup

- task: clean up the stale comment at TestReSTIR_GI_Temporal.cpp lines 650-652 that v5 missed
- source: no bundle
- approach: replace the 3-line stale comment block (which claimed RenderGBuffer's v1-introduced HLVM-bypass `close+execute+waitForIdle+open` was still in place) with a comment that accurately describes post-v5 behavior (all passes write into the same open CommandList; whole frame submits at line 675). Also update the bug-088 explanation paragraph below it to remove the now-stale "close+execute+waitForIdle at lines 1486-1491" reference (the bypass is gone) and add a Note pointing to v5's NOTE comment near line 1516. Pure documentation drift correction. No behavioral change. The renderer status (broken or fixed) is unchanged by this patch.
- diff_estimate: +6 / -7 lines (net -1 line after reformatting)
- skip_plan_review: no — comment touches a critical fix history note; needs plan-criticer sign-off
- test_strategy: no new tests needed; the validator and log-shape acceptance from v5/v6 still apply unchanged
- risks: minimal — pure text replacement of comments; no code path changes; no API surface changes; no shader changes. If the comment is wrong, parent will see a typo (cosmetic only).

## Rationale

Prior PIPELINE_HEALTH tick (2026-07-27) flagged this exact drift but explicitly chose NOT to fix it "to avoid introducing changes the parent may not expect to see in the next run." That rationale was valid at the time (immediately after v5/v6 patches landed) but now several cron ticks have passed without parent verification. The drift is real, the fix is mechanical and safe, and the parent has not run a verify since before the drift was even visible. Applying this fix now is the cron doing the mechanically actionable documentation cleanup that the pipeline state machine allows.

## What v7 explicitly does NOT do

- Does NOT change any code, shader, binding layout, or pipeline behavior.
- Does NOT add or remove tests.
- Does NOT change the validator, the test harness, or the build system.
- Does NOT trigger any v6 sub-plan (a/b/c/d). Those are still gated on parent's v5 verification.
- Does NOT commit or push (cron rules).
- Does NOT make any claim about the renderer's correctness post-fix. The renderer is still in the same state it was when v5's patch landed; this patch only updates stale documentation.

## Honest caveat

If parent runs the test after this patch, the renderer output will be IDENTICAL to what they would have seen with v5/v6 alone (the comments are non-executing text). The pipeline is still at v6 audit SOME_RELAX awaiting parent verification of v5's actual code patch.

## Files this cycle will touch

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (comment-only update at lines 650-672)