# Pending Plan Review v39 — decode_v38_evidence.py

## Verdict
- **KEEP** — v39 plan correctly identifies a real diagnostic-surface gap (the v38 cerr-line evidence shape requires a human to translate to a routing verdict) and proposes a precisely-scoped Python helper that closes that gap with zero changes to the GPU path.

## Design soundness
- The plan correctly diagnoses that the v39 decision matrix is keyed to cerr-line SHAPE, and the only step that requires a human is the shape→branch mapping. This helper automates that mapping.
- The script is purely additive: parses cerr text, classifies shape, emits verdict. No GPU path is touched. No existing diagnostic surface is removed.
- The 5 known-shape branches (GO / FIX_ATOI / FIX_DOCS / FIX_CVAR / NO_CERR) plus 2 fallback branches (MIXED / UNRECOGNIZED) cover all expected evidence shapes. The 2 routing branches that don't depend on cerr shape (validator 4/4 PASS, parent cannot rebuild) are appropriately handled at the orchestrator level (not the script level) — the script's job is to classify cerr evidence, not to drive the pipeline.
- Backward compatibility: the v38 cerr-line emission is unchanged. The v39 decision-matrix branches in PENDING_PICK.md are unchanged. Only the human-translation step is replaced by a script.

## Plan completeness
- 1 new file, ~250 lines, ~10KB. No source-code (C++/HLSL) modifications. Minimal blast radius.
- HARD INVARIANT #2 does NOT fire (new diagnostic helper, not a test file).
- `skip_impl_review: no` correctly justified: new file with non-trivial regex + classification logic.
- 7 self-tests via `--raw` flag cover all 5 known-shape branches + 2 fallback branches.
- Goal gate correctly notes that criterion (a/b/c/d) remain unverifiable without parent rebuild.

## Feedback for planner (FIX only)
- None — plan is well-scoped, minimal, and addresses a real diagnostic-evidence gap.

## Self-review checklist
- [x] Validation: 7 self-tests via `--raw` cover all classification branches.
- [x] Error handling: argparse mutually exclusive group enforces exactly-one input source; OSError handling for `--cerr-file`; pyright warning at line 187 (`__doc__.split`) addressed by `(or "").split("\n", 1)[0]` fallback.
- [x] Tests: PENDING_TESTS_v39.md defines 4 static tests (syntax validity, regex match, branch coverage, file presence) + 7 parent-driven runtime tests (one per branch).

## Single-head caveat
- Same model writes planner + plan-criticer. The KEEP is a self-check. The plan is mechanically simple (one new file with parser + classifier + argparse main) so the verdict is reproducible.

## Recommendation
- KEEP. Proceed to impler (write `decode_v38_evidence.py`).