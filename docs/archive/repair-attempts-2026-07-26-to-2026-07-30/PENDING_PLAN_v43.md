# Pending Plan v43 — extend fresh-evidence-scan.sh to cover v37-v41 patches (file-only diagnostic-surface completeness)

## State-machine routing decision

- Read every `docs/PENDING_*.md` marker. v42 cycle is complete: `PENDING_PLAN_v42.md`, `PENDING_PLAN_REVIEW_v42.md`, `PENDING_COMMIT_v42.md`, `PENDING_IMPL_REVIEW_v42.md`, `PENDING_TESTS_v42.md`, `PENDING_TEST_AUDIT_v42.md` all present; final verdict `ALL_KEEP`. v42 was a structural standby tick.
- Rule 9 (audit exists → next item from PICK) fires. Topmost unchecked item in `PENDING_PICK.md` is the v42 staging line (parent-evidence-gated decision matrix). v42 itself is a routing target, not a fix.
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 30+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset is file-only.
- Decision: identify the LAST mechanically actionable file-only fix and execute it.

## v43 candidate: extend fresh-evidence-scan.sh patch inventory from 17 → 21

**The gap (verified this tick via read_file of `fresh-evidence-scan.sh` lines 47-73):**

The v32 fresh-evidence-scan.sh helper script was written at v32 and freezes the patch inventory at that point. It has 22 CHECKS entries (header says "17-patch inventory" but the actual array has 22 entries). Since v32, the diagnostic surface was extended by v37, v38, v39, v40, v41 — 5 additional file modifications:

| Patch | Site | Status in script |
|-------|------|-----------------|
| v37 validator alpha-check | `validate_restir_gi.py:134` `check_alpha_sentinel()` | NOT CHECKED |
| v38 cerr DebugMode value | `FGIPass.cpp:487-491` `DebugMode effective=` | NOT CHECKED |
| v39 decode_v38_evidence.py | new file in `TestReSTIR_GI_Temporal_Data/` | NOT CHECKED |
| v40 dump_pixelstats alpha-stats | `dump_pixelstats.py` alpha block | NOT CHECKED |
| v41 encoder alpha preservation | `FImageDump.cpp:27` `rgbaData[i*4+3] * 255.0f` | NOT CHECKED |

The script's banner currently emits `BANNER: source-patch-missing` if any of the 22 entries fail, but it cannot detect v37-v41 drift. Parent's next run will get `BANNER: source-patch-missing (MISSING=0)` even if the v37 validator alpha-check has been reverted — masking a real regression.

**v43 fix:** append 5 new entries to the CHECKS array, bringing the inventory to 27 (matches the documented 21-patch cumulative count when v23/v24/v32 are counted as their own patch entries).

## Files modified this tick

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` (+5 entries to CHECKS array; +4 docstring lines documenting v43 attribution; net ~+15 lines)
- `docs/PENDING_PLAN_v43.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v43.md` (new)
- `docs/PENDING_COMMIT_v43.md` (new)
- `docs/PENDING_IMPL_REVIEW_v43.md` (new)
- `docs/PENDING_TESTS_v43.md` (new)
- `docs/PENDING_TEST_AUDIT_v43.md` (new)
- `docs/PENDING_PICK.md` (modified — v42 marked [x], v43 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v43 tick section)

**0 source-code changes (C++/HLSL) this tick.** Only the helper script and pipeline markers.

## skip_plan_review: no
- Standard audit trail. v43 is well-scoped but parent-evidence-gated follow-ups depend on this script's banner being authoritative.

## produces_test_files: no
- Helper-script change; no test file created. HARD INVARIANT #2 does NOT fire.

## skip_impl_review: no
- Pipeline invariants require full audit trail even for documentation/helper cycles.

## Test strategy

1. **Static tests (this tick, file-only)**:
   - 5 new CHECKS entries added with valid regex patterns matching v37/v38/v39/v40/v41 patches
   - Pattern regex correctness verified via static regex semantics
   - Banner verdict unchanged (still exits 0/1/2 based on MISSING count)
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent runs `bash fresh-evidence-scan.sh` after rebuilding
   - Banner should now report `MISSING=0` for ALL 27 entries (was 22) IF v37-v41 patches are present
   - If parent reverts any of v37/v38/v39/v40/v41, banner now reports `MISSING=N` correctly (vs pre-v43 silent pass)

## Risks
- Regex pattern match failure: each new entry's regex pattern is constructed to match a unique substring (per-line comment text, function name, or unique literal). Verified by search_files on the actual source.
- No risk of breaking the existing 22 entries (additive change, no modification to existing lines).
- No risk to the renderer (script is read-only; v23 dump-rotation is unchanged).

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

## Recommendation
KEEP. v43 closes the last real file-only diagnostic-surface gap: the helper script's patch inventory will now match the actual 21-patch cumulative count. After v43, the file-only work space is TRULY exhausted; subsequent cron ticks without parent terminal access will be identical-standby markers.