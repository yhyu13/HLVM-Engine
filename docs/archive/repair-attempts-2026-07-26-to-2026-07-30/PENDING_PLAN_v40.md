# Pending Plan v40 — extend dump_pixelstats.py to read alpha channel (parallel to v37's validator alpha-check)

## State-machine routing decision
- Read `PENDING_PICK.md`, all v39 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT, all KEEP/ALL_KEEP), `PIPELINE_HEALTH_2026-07-27.md` tail.
- v39 cycle is complete at audit ALL_KEEP. Rule 9 fires → next item from PICK.
- Topmost unchecked item in `PENDING_PICK.md` is `v40 (parent-evidence-gated; ONLY fires after parent runs decode_v38_evidence.py and pastes the structured verdict back)`. This is a 9-branch decision matrix keyed to the decoder's verdict shape — it's a routing target, not a mechanically actionable fix.
- All other unchecked PICK items (v33, v36, v32, v15, v13a, v17) are also parent-evidence-gated.
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 13+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset remains file-only.
- Cron's prompt explicitly authorizes: "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop."
- Reasoning: the v37 cycle extended `validate_restir_gi.py` to read the v28 alpha-channel sentinel (the validator was previously stripping alpha via `convert('RGB')`). The companion fast-first-look tool `dump_pixelstats.py` (v24) has the same alpha-stripping bug: it reads `img.convert("RGB")` and never inspects the alpha channel. This means the only way to verify the v28 alpha sentinel from stale dumps (without rebuilding) is to run the validator — but the validator is a slower, full-judgment tool. Extending `dump_pixelstats.py` to also report alpha stats makes the v28 sentinel immediately visible from any dump group, including stale ones.
- This is purely additive: it does not change the v37 validator, does not change the GPU path, and does not require a parent rebuild. The next parent run will produce dumps with the v28 sentinel; running `dump_pixelstats.py` on the existing stale dumps (20260727_000706-000708) is impossible because those predate v28 — but it WILL work on any future dump group without waiting for a rebuild.
- Decision: fire v40 cycle as a Python helper extension. No source-code (C++/HLSL) modification, no GPU-path change. Same pattern as v37, v38, v39 (diagnostic-surface expansion that makes the v28 sentinel verifiable without rebuild).

## Why v40 — the gap being closed

The v37 patch extended `validate_restir_gi.py::check_alpha_sentinel()` to read the alpha channel of `display_frame8.png` and emit a 5-pattern verdict (saturated / zero / mixed / low / no-dump). This is the project's main PASS/FAIL validator.

The v24 patch wrote `dump_pixelstats.py` as a "fast first-look" companion — it walks every dump group and emits per-channel mean/std/unique/sat255%/sat0% without the validator's structural thresholds. The intent was: when you can't rebuild (terminal blocked, GPU driver issue, etc.), you can still see "which channel is broken" from stale dumps.

**But v24 has the same alpha-stripping bug as pre-v37 `validate_restir_gi.py`**: line 88 reads `Image.open(path).convert("RGB")` and never touches alpha. So `dump_pixelstats.py` on a v28-sentinel dump would show R/G/B stats but completely miss the alpha signal — exactly the signal that disambiguates "dispatch body ran" vs "dispatch body didn't run" vs "pre-v28 binary".

v40 closes this gap by:
1. Reading the image in RGBA mode (not RGB) when PIL is configured to preserve alpha
2. Adding a separate `compute_alpha_stats()` function that emits per-frame alpha-channel mean/std/unique/sat255%/sat0%
3. Adding a sentinel classification (saturated / zero / mixed / low) matching v37's verdict ladder
4. Tagging each frame's block with `[v40-alpha]` so parent can grep for the new evidence shape

## Files produced
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` (modified; +~60 lines net)
- `docs/PENDING_PLAN_v40.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v40.md` (new)
- `docs/PENDING_COMMIT_v40.md` (new)
- `docs/PENDING_IMPL_REVIEW_v40.md` (new)
- `docs/PENDING_TESTS_v40.md` (new)
- `docs/PENDING_TEST_AUDIT_v40.md` (new)
- `docs/PENDING_PICK.md` (modified — v39 confirmed [x], v40 marked [x], v41 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v40 tick section)

## skip_plan_review: no
- This is a modification to an existing Python helper (not a source-code patch), and the alpha-classification ladder mirrors v37's 5-pattern verdict ladder. The per-role audit trail must be preserved for parent's review-on-demand and to keep the verifier/auditor roles in sync.

## produces_test_files: no
- Modifying an existing diagnostic helper, not creating a test file. HARD INVARIANT #2 does NOT fire.

## skip_impl_review: no
- Modifying existing file with non-trivial alpha-handling logic. Full audit trail invoked.

## Test strategy
1. **Static tests (this tick, file-only)**:
   - File syntax is valid Python 3 (verified via docstring + structure inspection)
   - `compute_alpha_stats()` function exists and emits 5 stats (mean/std/unique/sat255%/sat0%)
   - `classify_alpha_sentinel()` function exists and emits 5-pattern verdict (saturated / zero / mixed / low / unknown)
   - The 5-pattern ladder matches v37's `check_alpha_sentinel()` verdict ladder (consistency check)
   - File is independently runnable (no new dependencies; same numpy + PIL as v24)
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent runs `python3 dump_pixelstats.py` on any dump group from a v28-or-later binary; expects alpha=254-255 dominated frame blocks tagged `[v40-alpha] PASS (dispatch body ran)`
   - Parent runs on a pre-v28 dump group; expects alpha-uniformly-low frame blocks tagged `[v40-alpha] FAIL alpha=low (pre-v28 binary)`
   - Parent runs on a future dump where dispatch is broken (H-A confirmed); expects alpha-uniformly-zero blocks tagged `[v40-alpha] FAIL alpha=0 (dispatch body never ran; bug is upstream)`

## Risks
- **Single-head host caveat**: same model writes planner + plan-criticer + impler + reviewer + tester + verifier. Verdicts are self-checks. Patch is purely additive so verdicts are reproducible.
- **PIL RGBA-mode availability**: PIL's `Image.open(path)` returns mode "RGBA" for PNGs with alpha, "RGB" for PNGs without. The patch must handle both modes gracefully (RGB PNGs will have no alpha stats, fallback message printed).
- **Existing RGB-only behavior preserved**: the v24 RGB-channel stats must still emit identically. The patch only ADDS a separate alpha block after each frame's RGB block, does not modify the existing RGB logic.
- **No GPU/dispatch impact**: purely a Python helper extension.

## Decision matrix (post-parent-rebuild, post-v40)
- Parent runs `python3 dump_pixelstats.py` on the next dump group
- If every frame's `[v40-alpha]` block reads `PASS (dispatch body ran)` (alpha saturated 254-255) → v28 sentinel confirmed working in next-run binary; bug must be in lighting/payload/accumulate chain → routes to v41 (which stages accumulate/ReSTIR/denoise investigation)
- If every frame's `[v40-alpha]` reads `FAIL alpha=0 (dispatch body never ran)` → bug is upstream of the v28 sentinel write; routes to v41a (dispatch/binding-layout investigation)
- If every frame's `[v40-alpha]` reads `FAIL alpha=low (pre-v28 binary)` → parent has not rebuilt since v28; routes to v41b (rebuild reminder)
- If frames show `alpha=mixed` → partial dispatch (likely barrier or dispatch-tile issue); routes to v41c (barrier investigation)

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

After v40 lands, `dump_pixelstats.py` joins `validate_restir_gi.py` as a file-only verifier of the v28 alpha sentinel. No `PIPELINE_GOAL_DONE_<date>.md` written.