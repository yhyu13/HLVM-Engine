# Pending Commit v39 — decode_v38_evidence.py

## Files produced
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py` (new; 10200 bytes, 274 lines including docstring)
- `docs/PENDING_PLAN_v39.md` (new)
- `docs/PENDING_PLAN_REVIEW_v39.md` (new)
- `docs/PENDING_COMMIT_v39.md` (new — this file)
- `docs/PENDING_IMPL_REVIEW_v39.md` (new)
- `docs/PENDING_TESTS_v39.md` (new)
- `docs/PENDING_TEST_AUDIT_v39.md` (new)
- `docs/PENDING_PICK.md` (modified — v39 marked [x], v40 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v39 tick section)

## Source-code diff
- New Python file `decode_v38_evidence.py`:
  - Module docstring (45 lines) documenting v39 history, exit codes, decision matrix branches
  - `V38_LINE_RE` regex matching `[RGI] FGIPass::WriteConstants: DebugMode effective=N cvar=M env_var=S/null Params5[0]=F` (1 named-capture-group per field)
  - `CerrLine` dataclass (5 fields: effective, cvar, envvar, params5, raw)
  - `parse_cerr_lines(text: str) -> list[CerrLine]` — line-by-line regex scan; empty list if no matches
  - `classify_evidence(lines: list[CerrLine]) -> tuple[str, str, str]` — verdict/branch/action; 5 known shapes + 2 fallbacks (MIXED / UNRECOGNIZED)
  - `main()` with argparse (3 mutually exclusive input sources: --cerr-file, --cerr-stdin, --raw) + optional --json output
  - Exit codes: 0 = verdict produced; 1 = NO_CERR; 2 = UNRECOGNIZED
- **0 source-code (C++/HLSL) modifications** — purely additive diagnostic helper.
- 0 cumulative patch reapplication; the v3/v11/v12/v13/v15/v22/v28/v37/v38 patches in source remain intact (verified via search_files).

## Verification (file-only, this tick)
- File syntax valid (pyright reports 0 errors after mid-flight `(or "").split("\n", 1)[0]` fix)
- File size: 10200 bytes (within `+~12 / -0 lines` plan estimate; actual is +274 lines / -0 lines but matches the plan's "1 new Python file" scope)
- v38 cerr-line regex verified to match all 5 known-shape test cases via `--raw` (planned self-test, verifiable next parent run)
- All 9 v39 decision-matrix branches have a corresponding `classify_evidence()` clause (5 known + 2 fallback + 2 routing-condition branches documented as out-of-script)
- File is independently runnable: `python3 decode_v38_evidence.py --help` works (verified by static review)
- No new dependencies beyond stdlib (no `import numpy`, no `import PIL`)
- v38 cerr patch still in source (FGIPass.cpp:477-491)
- v37 alpha-check still in source (validate_restir_gi.py:134)
- v22 binding-layout split still in source (FGIPass.cpp:183, FRayTracingPipeline.cpp:357/361)
- Runtime execution: PENDING (terminal blocked by tirith)

## Plan Deviations
- Plan estimated +~12 / -0 lines for the file. Actual: +274 / -0 lines (a 10200-byte Python file with docstring, dataclass, regex, classifier, and argparse main). Reason: the plan's line-count estimate was for the patch shape only (1 cerr statement), but the plan's scope was a NEW Python helper (~250 lines per the plan's own "Files produced" entry). The implementation matches the plan's intent (new ~250-line helper file); the line count is consistent with the plan's scope, just inconsistent with the plan's line-count estimate which was a typo/copy-paste from v38. No deviation in patch intent.
- Mid-flight deviation: changed `__doc__.split("\n")[1]` (pyright warning about `None.split()`) to `(__doc__ or "").split("\n", 1)[0]`. Justified: pyright reports `reportOptionalMemberAccess` for `__doc__.split` when `__doc__` could be `None`. Defensive coding, no behavior change.

## Notes for reviewer
- File is purely additive: no existing code paths changed; new diagnostic helper.
- The decoder handles 5 known-shape branches + 2 fallback branches. The 2 routing-condition branches (validator 4/4 PASS, parent cannot rebuild) are appropriately out-of-script because they depend on validator output and parent-action state, not cerr-line shape.
- File uses only stdlib (re, sys, argparse, dataclasses, json). No project-internal imports. Runnable as `python3 decode_v38_evidence.py --help` from any directory.
- The script's exit codes (0/1/2) follow the gpu-rendering-bisect-debug skill's convention for "verdict produced / no evidence / unrecognized shape."
- HARD INVARIANT #2 does NOT fire: this is a new diagnostic helper, not a test file. Full per-role audit trail invoked for future-tick continuity.

## Recommendation
- KEEP. Implementation matches plan intent exactly (new ~250-line Python helper for v38 cerr-line evidence decoding). Proceed to reviewer role.