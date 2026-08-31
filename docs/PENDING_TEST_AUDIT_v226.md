# Pending Test Audit v226

- tests: docs/PENDING_TESTS_v226.md
- commit: docs/PENDING_COMMIT_v226.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-579)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] **No `|` alternation in any pattern** (tick-526) — every query single-term
- [x] **No `file_glob` in any load-bearing query** — none used
- [x] **No conclusion resting on `output_mode=count` alone** (v198/v225) — **enforced and reproduced.** Cycle's net-new finding: `MaterialPlaceholderTexture` in `Runtime` returns **0 in `count`** and **6 in `files_only`**, with the `count` map showing only ThirdParty/build-log files. The verifier independently confirmed via row 9 of the tester table; the two observations are now two-run / two-scope / two-cycle and the rule is upgraded to a hard prohibition rather than a guideline.
- [x] **Every load-bearing zero paired with a same-shape positive control in the same scope** (v217) — rows 1+2, 3+4, 5+8 each form a positive/negative pair
- [x] **No count inherited across markers without re-derivation** (v211) — every row is re-run by the tester
- [x] **No runtime result fabricated** — nothing built, run, executed, or viewed by any role
- [x] **Patch-tool diff read before declaring done** (v203/v224) — N/A, zero patches this cycle

## Per-test verdict

9 PASS / 9 KEEP.

## What this cycle established

1. **The v200–v225 source window is compile-coherent.** v206 (comment), v207 (live UAV binding change with dual-copy hazard), v209 (class member deletion) — all three audited, all three clean in both consumers (where applicable). The deletion's residual-reference sweep survived the v225 enumeration-cap reproduction.
2. **v207's safety argument survives the dual-copy resolution it had not done itself.** v207's line-number argument was made against the `Test/..._Data/` copy; the consumer that hits the fallback compiles the `Private/Renderer/Shader/GI/` copy instead. The two copies are byte-identical in v207's load-bearing region, so the argument transfers — but the gap existed for 19 cycles (v207 to v226) and was found only because v225's rule about re-deriving marker claims was applied at the plan gate.
3. **v225's enumeration-cap defect is now standing, twice-observed.** Two runs, two scopes (`docs/` queue in v225, `Engine/Source/Runtime` source in v226), one defect (`output_mode=count` at directory scope enumerates only the first ~50 files and silently omits the rest). Upgraded to hard rule: any audit conclusion depending on a directory-scoped query must use `files_only`.
4. **The standing rules added by this cycle are the audit itself.** No new test files; the verifier table's nine rows ARE the artifact, and every row is re-derivable from the source as written.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused at tool boundary (3 fresh probes this tick, all `tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 46+ unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no `vision_analyze` tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

**This cycle's contribution to gates: 0.** It did not build, run, or render anything. What it repaired is upstream of the gates: the three post-v200 source changes are no longer unaudited going into the operator's first build, and the next build that runs will surface defects only from genuinely new code (v226-onward), not from changes already merged.

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit or push. Did not modify engine source — `FGIPass.cpp`, `FGIPass.h`, `FReBLURPass.h` still byte-unchanged at the version the impl reviewed. Did not modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file. **Did not fabricate any runtime result.**

## Verdict

**ALL_KEEP.** The cycle's three verdicts (v206 clean, v207 clean in both consumers, v209 deletion complete) are independently re-derived and held. The dual-copy resolution closes a 19-cycle gap. The enumeration-cap finding is now twice-observed and upgraded to a hard rule.