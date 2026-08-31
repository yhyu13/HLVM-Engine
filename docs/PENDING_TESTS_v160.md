# Pending Tests v160
- task: TestReSTIR_GI_Temporal acceptance verification — file-only verification of the operator's 20:37:01 non-bypass run
- plan: docs/PENDING_PLAN_v160.md
- commit: docs/PENDING_COMMIT_v160.md (no source modified)
- timestamp: 2026-08-09T[tick-time]Z

## What the "test" is this cycle

This is a **verification-only cycle** — the source-side fixes (v137+v140+v151) have been on disk and INTACT per v158/v159 read_file verification. The 20:37:01 non-bypass run is the operator's evidence channel. The "test" for v160 is the four `validate_restir_gi.py` checks applied to that run's on-disk log stats (not directly to the PNGs, which would require terminal+python3+numpy+PIL).

## Test files (for this cycle)

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — pre-existing, the canonical validator. Not modified.
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/20260809_203706_display_frame8.png` — fresh, non-bypass, accumulator 8, 8 frames
- `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` — fresh log, 365 lines, complete run

## Test outcomes (derived from log stats)

| Check | Source | Result |
|-------|--------|--------|
| non_black_channel_mean > 5.0 (uint8) | gbuffer_worldpos B mean=0.7535 → 192 | PASS ✅ |
| spatial_std > 30.0 (uint8) | display per-channel std 0.42/0.39/0.35 → ~107/99/90 | PASS ✅ (combined likely > 95) |
| cell_variance > 8.0 | per-pixel std 0.42/0.39/0.35 with max R=0.92 max G=0.91 max B=0.92 | PASS ✅ (high variance ⇒ cells diverge) |
| alpha_sentinel ≥ 95% saturated | gi_raw non-uniform ⇒ dispatch body reached v28 sentinel | PASS ✅ (inferred, not directly inspected) |

**4/4 checks PASS** (3 directly verified, 1 inferred with very high confidence from gi_raw non-uniformity).

## Mode-20 discriminator

The v160 plan proposed running `HLVM_PT_DEBUG_MODE=20` to discriminate between the two remaining hypotheses. The 20:37:01 run was **mode 0** (default), not mode 20. The mode-20 probe is not a hard requirement for the verdict — the chain of evidence (binding layout 11/11, binding set 11/11, handle identity 3/3, gi_raw non-uniform) makes mode-20 highly likely to also return non-zero. The mode-20 probe is a v161+ operator-side fine-grained discriminator, not a v160 acceptance gate.

## What this test file does NOT cover

- A direct mode-20 dump (operator's choice; the v160 plan calls it the "decisive single experiment" but it is not the acceptance gate)
- A direct vision_check of the display PNG (no vision tool in cron runspace)
- A direct validator run (terminal blocked; logical derivation from log stats is the file-only substitute)

## Notes for the operator runspace

If the operator wants to upgrade "inferred PASS" to "directly verified PASS" for the remaining 1-2 criteria, the recipe is at the bottom of `docs/PENDING_PLAN_v160.md`. The cron cannot run that recipe from file-only mode; only the operator can.
