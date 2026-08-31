# Pending Commit v214

- plan: docs/PENDING_PLAN_v214.md
- plan_review: docs/PENDING_PLAN_REVIEW_v214.md (KEEP)
- files:
  - Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
- source: no bundle — direct edit
- target: branch already on disk; commit is staged as PENDING_COMMIT marker, NOT applied to git (per `software-development-practices §Cron context delivery` and `OVERSEER_ESCALATION` hard veto #8)
- task: Move MaterialPlaceholderTexture creation+upload from DispatchRays to Initialize; eliminate the per-frame `waitForIdle()` at `FGIPass.cpp:671` and the `Device->executeCommandList(WriteCmd)` at `:670` from any per-frame hot path.
- verify: `search_files path=Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp pattern="waitForIdle"` → exactly 1 hit at line 415 (Shutdown). Pre-patch: 2 hits (415, 671).
- skip_impl_review: yes (one functional move + lifecycle reshape, no shader/cbuffer/signature touched; cycle's full-bleed verification is in the test/audit rows; `produces_test_files: no`).
- produces_test_files: no
- notes: per v203's standing rule, anchor the `old_string` for the per-frame removal at the closing comment of the descriptor-array fill (line 683), NOT at the comment above `:651` that introduces the placeholder block — the latter is the v203 near-miss geometry. Apply the replacement edit to `Initialize()` with `old_string` anchored on the `UploadLights()` call at `:171`, NOT on `bIsInitialized = true` at `:174` (also v203 geometry).

## Verify-line correction (superseded by tick-574's v224 cycle)

**The `verify:` line at `docs/PENDING_COMMIT_v214.md:10` (above) is stale and produces a FALSE FAILURE on a correct tree.** Card T in `docs/PENDING_PICK.md` documents this and this section is the supersede per the v224 plan (chosen over rewriting the original audit text in place, which would have silently changed what the marker chain claims happened). v214's verdict (KEEP) and audit trail are unchanged — the original text above is byte-identical to v214's commit; this section is an addition, not a modification.

**What the verify line says**: "*exactly 1 hit at line 415 (Shutdown). Pre-patch: 2 hits (415, 671).*"

**What the current tree actually shows** (re-derived independently by v223's tick-574 and re-verified for v224): `search_files pattern="waitForIdle" path=Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` → **3 hits**: one COMMENT hit at `:177` (a comment in the v214 commit's own description of what was removed), plus two CODE hits — one in `Initialize()` at `:197` (post-`UploadLights()`, the moved call), and one in `Shutdown()` at `:441` (the original call at `:415`, now relocated by an intervening refactor).

**Why the count is 3, not 1 or 2**: v214 *moved* the per-frame `waitForIdle` from `DispatchRays:671` into `Initialize()` (now `:197`) rather than deleting it. The Shutdown call at `:415` survived but was relocated to `:441` by an unrelated refactor (pre- or post-v214, source-decidable which by `git blame` but not relevant here). The original count of 2 code hits is correct (Initialize + Shutdown), but the marker predicted "deleted" — wrong. The third hit is a comment in v214's own description of the move (`:177`), which the marker forgot to exclude.

**Corrected verify** (worked example an operator can paste): the patch is **PRESENT** if and only if all three of the following hold:
1. `waitForIdle` appears **in `Initialize()`** (after `UploadLights()`) — the moved call. **PRESENT at `:197`.**
2. `waitForIdle` appears **in `Shutdown()`** — the original call (line number drifts; check the function, not the line). **PRESENT at `:441`.**
3. `waitForIdle` does **NOT** appear in `DispatchRays()`. **Verified absent** by reading `void FGIPass::DispatchRays` end-to (e.g., spans roughly `:533-741`; check the function, not the line — v217 finding).

**Why this outranks a stale number**: v192 established that a false failure is worse than a false pass — a false pass wastes a cycle, a false failure sends someone to re-patch code that is already right. The Shutdown call is on `FGIPass`, the one file whose per-frame path this lineage has already broken and repaired once. A correct verify is the difference between "operator at the keyboard trusts the audit trail" and "operator at the keyboard debugs code that is fine."

**Stale-line caveat**: the line numbers cited above are observed at the v224 cycle's re-derivation (2026-08-21). Counts survive across cycles; line numbers drift. A future operator running this corrected verify should treat the symbols (`Initialize`, `Shutdown`, `DispatchRays`) as the load-bearing claim, not the numbers — see v217's "counts are not invariants, sets are" lesson.

**Inserted by v224's supersede patch**; original text above is byte-identical to v214's commit (HARD INVARIANT: markers are append-only).

## Plan Deviations (impler fills this in if it deviated)

None.

## Functional change manifest

| File | Lines removed | Lines added | Net |
|---|---|---|---|
| `FGIPass.cpp` | 19 (`:654-672`) | 21 (in `Initialize`, after `:172`) | +2 |

`search_files` expected post-state:
- `waitForIdle` → 1 hit at line 415
- `MaterialPlaceholderTexture = Device->createTexture` → 1 hit in `Initialize`, 0 in `DispatchRays`
- `Device->executeCommandList` → 0 hits in `DispatchRays` (was 1, the per-frame placeholder upload)