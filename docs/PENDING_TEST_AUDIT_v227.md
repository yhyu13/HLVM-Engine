# Pending Test Audit v227

- tests: docs/PENDING_TESTS_v227.md
- commit: docs/PENDING_COMMIT_v227.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-580)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] **No `|` alternation in any pattern** (tick-526) — every query single-term; one attempt this tick hit the bug, was caught and corrected before being recorded as evidence
- [x] **No `output_mode=count` at directory scope** (v225/v226) — none used
- [x] **No `path=` pointing at a directory in file mode** (tick-526) — one attempt this tick hit the bug (file mode on directory returned 0 hits), was caught and corrected
- [x] **Every load-bearing zero paired with a same-scope positive control** (v217) — rows 1+2, 5+6 each form positive/negative pairs that share scope
- [x] **No count inherited across markers without re-derivation** (v211) — every row is re-run by the tester in this cycle
- [x] **No runtime result fabricated** — nothing built, run, executed, or viewed by any role
- [x] **Patch-tool diff read before declaring done** (v203/v224) — N/A, zero patches this cycle

## Per-test verdict

7 rows / 7 KEEP.

## What this cycle established

1. **The two standing remedies in the lineage are inert for a reason neither predecessor cycle stated.** `cron_mode: allow` and `tirith_fail_open: true` are BOTH conditional on `env_var_enabled("HERMES_CRON_SESSION")` returning True at `tools/approval.py:2700` and `:2746` respectively. The env var is set process-wide in the scheduler at `cron/scheduler.py:2812`, but its propagation to the agent subprocess that consults `tools/approval.py` is the unverified link.
2. **The pending-approval fallback at `tools/approval.py:2999-3012` is the source of the observed envelope**, verified field-for-field (4/5 fields match exactly; the 5th, `smart_denied: false`, is the default and is correct).
3. **The acceptance command as written IS allowlist-eligible.** The shell-operator regex at `:1660` blocks only `\n && || ; & | < > \` $(` — the command `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` contains none of these. The matcher at `:1668-1694` would return True on exact-string match.
4. **A one-line `command_allowlist` addition bypasses the entire cron/env/CLI debate** by short-circuiting at `:2689-2690` before any of the gating predicates run. This is the smallest correct operator remedy.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | UNKNOWN | `terminal` refused at tool boundary (4 fresh probes this tick) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 46+ unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no `vision_analyze` tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

**This cycle's contribution to gates: 0.** It did not build, run, or render anything. What it repaired is **upstream** of the gates: the operator now has a single concrete config edit that, if applied, unblocks the build that unblocks gates 1, 2, 3, 4, 5, 7. Gate 6 remains structurally blocked.

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit or push. Did not modify engine source — `FGIPass.cpp`, `FGIPass.h`, `GIPathTracing.hlsl` (both copies), `TestReSTIR_GI_Temporal.cpp`, `validate_restir_gi.py`, `v176-recipe.sh` all byte-unchanged this tick. Did not modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, `~/.hermes/config.yaml`, or any governance file. **Did not fabricate any runtime result.**

## Operator action (single line, repeated from commit for visibility)

Edit `~/.hermes/config.yaml:478-479`:

```yaml
command_allowlist:
  - script execution via -e/-c flag
  - ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

Then from a fresh shell:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

If the build's own sub-invocations need approving, add them one at a time.

## Verdict

**ALL_KEEP.** The cycle's seven rows are independently re-derived and held. The two standing remedies are correctly diagnosed as conditional on a missing precondition (`HERMES_CRON_SESSION` in the subprocess), and the operator action is verified against the actual matcher code at `tools/approval.py:1660-1694`.