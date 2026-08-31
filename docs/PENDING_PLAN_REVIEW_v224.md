# Pending Plan Review v224

- plan: docs/PENDING_PLAN_v224.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-now, autonomous invocation #574, this turn)
- timestamp: 2026-08-21

## Design soundness

The plan addresses card T's specific finding (stale verify line in PENDING_COMMIT_v214.md:10 that produces a false failure on a correct tree) and the design is sound. Card T's premise is correct, **re-verified first-hand this turn**:

- `search_files pattern="waitForIdle" path=Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` → **3 hits**:
  - `:177` (a comment, part of the v214 cycle's *description* of what was removed — "per-frame `Device->waitForIdle()` at line 671 and an open/close")
  - `:197` (real code, inside `Initialize()` per the v214 move)
  - `:441` (real code, inside `Shutdown()`)
- The marker claims "*exactly 1 hit at line 415*" but the real count is 3 hits with 2 code hits (Initialize + Shutdown). The pre-patch count of 2 hits (Shutdown + DispatchRays) is correct — v214 moved the DispatchRays one to Initialize, so the 2 code hits survive, just at new line numbers and in a different function.
- The marker's prediction was "deleted, not moved" — wrong; v214 *moved* the per-frame call into Initialize, retaining the call in Shutdown. The 2 code hits survive; the count should be 2 not 1.

The plan correctly distinguishes (a) "fix the verify line in place" from (b) "supersede with a correction note" and chooses (b) for the right reason: the marker chain is the audit trail. Modifying a closed cycle's marker text in place would silently change what the audit chain claims happened. A supersede note preserves the original text and adds the correction.

## Plan completeness

Three binding additions before endorsing the patch:

1. **The corrected verify must name the structural location, not the line number.** v217 established that counts survive across cycles but line numbers do not. The original marker named `:415` for Shutdown, but Shutdown moved to `:441` (possibly in v214 itself, possibly in an earlier refactor — controlled via `waitForIdle` count: 3 hits means Initialize + Shutdown + 1 comment, so the original 2-code-hit count survives). The corrected note must say "2 code hits, one in `Initialize` (post-`UploadLights`), one in `Shutdown`, plus 1 comment hit at `:177`" — symbols, not numbers.

2. **Add a worked example of the false failure to the correction note.** Card T established that an operator running v214's verify line on a healthy tree gets `waitForIdle` → 3 hits ≠ 1 hit and reads it as "the patch is not present." A worked example ("`search_files ... pattern="waitForIdle"` → 3 hits where 1 was expected; the 3 are comment/Initialize/Shutdown, the 2 code hits are at Initialize (post-UploadLights) and Shutdown") makes the false-failure pattern recognisable on first encounter, not after an hour of debugging.

3. **State up front that the marker chain's append-only invariant holds.** HARD INVARIANT in the six-role pipeline says markers are append-only. The plan uses patch insertion BEFORE `## Plan Deviations`, but a future reader could mistake the inserted section for prior content. The patch tool's returned diff is the verification; the note must say "inserted by v224's supersede patch; original text above is byte-identical to v214's commit."

## Feedback for planner (FIX only)

n/a — KEEP with the three additions above binding on the impler's marker (rows 1, 2) and on the inserted section's framing (row 3).