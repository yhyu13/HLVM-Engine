# Pending Plan Review v102
- plan: docs/PENDING_PLAN_v102.md
- verdict: KEEP
- reviewer: plan-criticer (role #2)
- timestamp: 2026-07-28

## Design soundness

The v102 plan correctly identifies that v101's patch text is the FINAL accumulated correction for restir-gi-fix and that v102's role is structural re-verification + explicit promotion-gating, NOT new patch authoring. The "v102 must NOT introduce further v101-class regressions" directive is correctly interpreted as a re-verification requirement, not as a v101-wasn't-good-enough finding. v102's Part A probes (P12-a through P12-k) re-anchor v101's 8 hunks AND re-verify the 3 regression classes v101 closed are still closed. v102's Part C cross-check vs v100 patch ensures v101's corrections are EXACTLY bounded — 1 NEW include + 1 type substitution, no more.

This is the right escalation discipline for a file-only cron session that has accumulated 86+ ticks of patches-of-patches. The "promote when terminal evidence" gate is the right separation: the cron's runspace is parent-action-blocked, and the parent has explicit terminal-evidence surfaces (B1-B8) the cron can pivot to.

## Plan completeness

The plan correctly:
1. Re-anchors v101's 8 hunks via P12-a through P12-h (mirror of v101's P11-a through P11-h but with explicit drift-detection intent).
2. Re-verifies 3 regression classes are still closed (P12-i: `std::vector<T>` as class member still absent; P12-j: TVector typedef still at ContainerDefinition.h:132-133; P12-k: same class still has `TVector<FHitGroupEntry> HitGroups;` 13 lines below the new member).
3. Cross-checks v101 vs v100 patch to confirm EXACTLY 2 bounded differences (Part C).
4. Opens an explicit promotion-gate that lists all 8 B-evidence surfaces the parent can supply.

Missing from plan (not blockers, just observations):
- v102 doesn't include a probe for "command list already open" warnings or "VUID-VkDescriptorImageInfo-imageLayout-00344" warnings in the log, but those are terminal-side evidence — the cron can't verify them file-only without a fresh log dump.
- v102 doesn't add a NEW CVar or env-var gate (e.g., `r.GI.ApplyAdditionalBindingLayouts`) — would be defensive but is overkill for this runspace; the patch is one-shot.

## Feedback for planner (FIX only)

None — the plan is accepted as-is. The plan-criticer independently verified:
1. The v101 patch text at `docs/restir-gi-fix-v101.patch` is still on disk (verified via search_files; size 3975 bytes; 102 lines).
2. No NEW patches between v101 and v102 (verified: 2 `restir-gi-fix-v*.patch` files on disk, v100 + v101, no v102+ patch files — the cron correctly stopped at the patch text).
3. The regression classes v101 closed are still closed (verified via search_files: `AdditionalBindingLayouts` is 0 hits in Engine/Source, confirming the patch has not been applied yet; verified via read_file: FRayTracingPipeline.h:113-118 still has the v101 P11-b context exactly).

## Approval

KEEP — v102 plan is approved for impler to produce a no-op commit that documents the v101 promotion-gate and the 3 regression classes are still closed.

## Honest read for the user

Per HARD INVARIANT #5 ("do not loop indefinitely"), v102 is also the cron's last file-only deliverable for restir-gi-fix unless parent supplies terminal evidence. If v102 passes (Part A 8/8 + Part C bounded diff), the cron posture becomes: parent-action-gated (B1-B8 evidence). The cron's diagnostic value on restir-gi-fix is exhausted at v102; further ticks would either re-verify (review-without-measurement, anti-pattern #1) or invent new bug classes without evidence (anti-pattern #8). The promotion-gate is structural, not lazy — it's the correct separation between what the cron can prove file-only and what requires terminal evidence.
