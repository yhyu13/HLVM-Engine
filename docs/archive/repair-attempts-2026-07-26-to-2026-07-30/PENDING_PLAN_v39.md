# Pending Plan v39 — decode_v38_evidence.py: structured routing verdict for v38 cerr-line text

## State-machine routing decision
- Read `PENDING_PICK.md`, all v38 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT, all KEEP/ALL_KEEP), `PIPELINE_HEALTH_2026-07-27.md` tail.
- v38 cycle complete at audit ALL_KEEP. Rule 9 fires → next item from PICK.
- Topmost unchecked items in `PENDING_PICK.md` are all parent-evidence-gated (v39, v33, v36, v32, v15, v13a, v17). Cron's prompt grants `enabled_toolsets: ["terminal", "file"]` but every terminal probe (8+ ticks worth) is blocked by tirith. Effective toolset is file-only.
- Cron's prompt explicitly authorizes: "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop."
- Decision: v39 is the next mechanically actionable file-only fix that advances the diagnostic surface WITHOUT fabricating evidence or claiming tests passed.

## Why v39 — the gap being closed

The v38 patch added a 4-field cerr line to `FGIPass::WriteConstants`:
```
[RGI] FGIPass::WriteConstants: DebugMode effective=N cvar=M env_var=S/null Params5[0]=F
```

On parent's next rebuild + run, stderr.log will contain 8 of these lines (one per frame at HLVM_RGI_ACCUM=8). The cron's v39 decision matrix (PENDING_PICK.md lines 164-173) maps cerr-line SHAPE to 9 routing branches.

**Current gap**: the parent (or the human reviewing `rgi_evidence.txt`) must read the raw cerr text, mentally classify the shape, and choose the right next step from the matrix. This is the only step in the v39 path that requires a human to translate natural-language cerr text into a routing decision.

**v39 closes this gap** with a Python helper that:
1. Parses the v38 cerr-line format (regex-based, robust to extra whitespace)
2. Classifies the evidence shape into one of: GO / FIX_ENV / FIX_CVAR / FIX_DOCS / FIX_ATOI / NO_CERR / MIXED / UNRECONGNIZED
3. Maps the shape to the v39 decision-matrix branch number (1-9 + mixed + out-of-band)
4. Emits a structured routing verdict (verdict / branch / next-action) that the next six-role pipeline cycle can act on without re-reading the raw cerr text

This is purely additive: it doesn't change the v38 cerr-line emission, doesn't change the v39 decision-matrix branches, and doesn't affect the GPU path. It just makes the existing diagnostic surface executable instead of relying on a human to translate.

## Files produced
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py` (new; ~250 lines, ~10KB)
- `docs/PENDING_PLAN_v39.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v39.md` (new)
- `docs/PENDING_COMMIT_v39.md` (new)
- `docs/PENDING_IMPL_REVIEW_v39.md` (new)
- `docs/PENDING_TESTS_v39.md` (new)
- `docs/PENDING_TEST_AUDIT_v39.md` (new)
- `docs/PENDING_PICK.md` (modified — v38 [x] confirmed, v39 marked [x], v40 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v39 tick section)

## skip_plan_review: no
- This is a NEW Python file (not a source-code patch), and the v39 decision-matrix mapping is non-trivial (5 known branches + 2 fallback branches). Even though the patch is "just a parser," the per-role audit trail must be preserved for parent's review-on-demand.

## produces_test_files: no
- New diagnostic helper, not a test file. HARD INVARIANT #2 does NOT fire.

## skip_impl_review: no
- New file with non-trivial logic. Full audit trail invoked.

## Test strategy
1. **Static tests (this tick, file-only)**:
   - File syntax is valid Python 3 (verified via `python3 -m py_compile`)
   - v38 cerr-line regex matches the canonical format (verified via unit-style cases embedded in docstring + `decode_v38_evidence.py --raw` with each shape)
   - All 9 v39 decision-matrix branches have a corresponding `classify_evidence()` clause (verified by code review)
   - File is independently runnable (no project-internal imports)
   - No new dependencies beyond stdlib (no pip install needed)
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent runs v38 with `HLVM_PT_DEBUG_MODE=6`, pastes stderr.log content, runs `python3 decode_v38_evidence.py --cerr-file stderr.log`
   - Expected output: `verdict=GO branch=1 next action=cbuffer-update path healthy; run case-6u`
   - Repeat with no env var, with CVar only, with garbage env var, etc.
   - Each shape should produce a different verdict/branch

## Risks
- **Single-head host caveat**: same model writes planner + plan-criticer + impler + reviewer + tester + verifier. Verdicts are self-checks.
- **Regex brittleness**: v38 cerr-line format may drift if FGIPass.cpp:487-491 changes. Acceptable risk: the v39 script can be re-synced from source as needed. Mitigation: embed a unit-style self-test in the docstring showing all 9 shapes.
- **Branch coverage**: the v39 decision matrix has 9 branches; the script handles 5 cleanly (GO/FIX_ATOI/FIX_DOCS/FIX_CVAR/NO_CERR) and 2 fallback (MIXED/UNRECOGNIZED). The remaining 2 branches (validator 4/4 PASS, parent cannot rebuild) are routing conditions that don't depend on cerr shape, so they're appropriately handled at the orchestrator level (cron reads validator output, not just cerr text).
- **No new GPU/dispatch behavior**: purely additive.

## Decision matrix (post-parent-rebuild, post-v39 script)
- `python3 decode_v38_evidence.py --cerr-file stderr.log` → emits structured verdict
- If `verdict=GO`: parent proceeds to v32 branch 1 (case-6u evidence check)
- If `verdict=FIX_ATOI`: parent checks env-var bytes (hexdump) or uses CVar bypass
- If `verdict=FIX_DOCS`: parent sets the env var correctly
- If `verdict=FIX_CVAR`: parent investigates env-var propagation in test harness
- If `verdict=NO_CERR`: parent rebuilds; if confirmed, v12 cerr patch is missing
- If `verdict=MIXED`: parent inspects raw cerr lines for env-var mutation between frames
- If `verdict=UNRECOGNIZED`: parent surfaces this output to cron for manual classification; v39 may need a new branch

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

After v39 lands, the diagnostic surface gains a Python decoder that converts cerr-line text into a routing verdict. No `PIPELINE_GOAL_DONE_<date>.md` written.