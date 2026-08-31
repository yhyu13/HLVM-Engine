# Pending Commit v6 — contingency (no code change)

- plan: docs/PENDING_PLAN_v6.md
- files: NONE (v6 is dormant contingency; no code change in this cycle)
- source: no bundle
- target: working tree (no commit per cron rules)
- task: v6 contingency plan staged in `docs/PENDING_PLAN_v6.md`. No code changes this cycle.
- verify: parent must run v5 first; the v6 sub-plan to execute depends on the parent's v5 verification outcome.
- skip_impl_review: yes — no implementation to review
- produces_test_files: no
- notes:
  - This commit marker records that v6 is staged but NOT triggered.
  - The actual v6 implementation will be one of v6a (texture recreation/barrier/payload), v6b (accumulate/ReBLUR/display chain), v6c (display blit), or v6d (no-op, v5 fixed everything).
  - The decision matrix is in `docs/PENDING_PLAN_v6.md`.
  - Cycle also includes: stale comment fix at TestReSTIR_GI_Temporal.cpp lines 395-398 — the previous comment claimed RenderGBuffer "internally closes+submits+reopens the CommandList" which was the v1-introduced HLVM-bypass that v5 removed. Updated to reflect actual v5 behavior.

## Implementation Evidence (impler fills this in)

- File modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — comment update at lines 395-398 (4 lines changed to 4 lines; net 0).
- No behavioral change. The comment now accurately reflects what RenderGBuffer does (leaves CL open) and points to the v5 NOTE comment near line 1516 for the full rationale.
- Build/run verification: BLOCKED by tirith in this cron (terminal approval denied for every command). Parent must drive the verify step.

## What v6 explicitly does NOT do

- Does NOT change FGIPass.cpp.
- Does NOT change any shader.
- Does NOT change any binding layout.
- Does NOT commit/push (cron rules).
- Does NOT trigger any sub-plan until parent's v5 verification produces log evidence.

## Note on the stale comment fix

The comment at lines 395-398 of TestReSTIR_GI_Temporal.cpp claimed "RenderGBuffer internally closes+submits+reopens the CommandList (see lines ~1529-1534) so the raster pass submits in isolation." This was true in v1 (when the HLVM-bypass was in place) but became false in v5 (when the bypass was removed). v5 missed this comment. The v6 cycle (this commit) updates the comment to accurately describe v5's behavior.