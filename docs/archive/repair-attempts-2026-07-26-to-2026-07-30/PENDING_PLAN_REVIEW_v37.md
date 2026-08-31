# Pending Plan Review v37 — alpha-channel sentinel awareness in validate_restir_gi.py

## Verdict
- **KEEP** — v37 plan correctly identifies the structural gap (validator strips alpha) and proposes a precisely-scoped fix that aligns with the v28 sentinel design.

## Design soundness
- The plan correctly diagnoses that `convert('RGB')` strips the alpha channel where the v28 sentinel signal lives. This is structurally identical to the v6 dump-clamp anti-pattern (data correct, validator doesn't see it).
- 5-alpha-pattern verdict ladder is exhaustive: saturated / zero / mixed / low / no-dump. Each maps to a precise diagnostic that drives the next cycle without ambiguity.
- Backward compatibility is correctly assessed: existing pre-v28 dumps will FAIL with `alpha=low` (correct diagnostic, not regression).

## Plan completeness
- 1 file modified, ~+35/-5 lines — minimal blast radius.
- HARD INVARIANT #2 fires correctly (test file → triggers reviewer → tester → testing-verifier).
- skip_impl_review: no (correctly defaults for test files).
- 5-branch decision matrix covers all post-rebuild evidence shapes.
- Goal gate correctly notes that criterion (e) becomes verifiable after v37 lands.

## Feedback for planner (FIX only)
- None — plan is well-scoped, minimal, and addresses a real diagnostic-evidence gap.

## Self-review checklist
- [x] Validation: alpha-sentinel check is exhaustive; each verdict shape has a clear downstream action.
- [x] Error handling: load_display_rgba + sentinel check both handle missing-display gracefully (returns `no-dump`).
- [x] Tests: 4-check validator correctly maintains the existing 3 checks AND adds a 4th; no regression to RGB-only checks.

## Single-head caveat
- Same model writes planner + plan-criticer. The KEEP is a self-check. The plan's mechanical structure (additive-only change, no source-code touched, decision matrix covers all evidence shapes) keeps the verdict reproducible.

## Recommendation
- KEEP. Proceed to impler (apply the validator patch).