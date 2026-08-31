# PIPELINE_HEALTH_2026-07-27 — v21 Tick Audit (writing the v21 sequence; v20 cycle completed above)

This file covers both the v20 cycle (now complete) AND the v21 follow-up that the cron entered automatically per the user's "continue cycling until acceptance criteria are met" instruction.

## v20 cycle complete — one-shot diagnostic runner script landed

### State-machine routing decision

- v19 audit ALL_KEEP (verified at lines 575-684 of both HLSL copies; byte-identical 792 lines / 31766B each; 14 probes stable).
- v20 heartbeat at `docs/PIPELINE_HEALTH_2026-07-27_v20.md` correctly identified no more file-only diagnostic-surface additions were possible.
- Per the cron's prompt "continue cycling ... until the acceptance criteria are actually met," the cron identified ONE remaining file-only action: consolidate the v20 9-branch decision-matrix evidence-capture protocol into a single runnable bash script.
- v20 cycle fired: planner → plan-criticer → impler → reviewer → tester → testing-verifier.

### v20 cycle executed

#### Planner (role 1)
- Wrote `docs/PENDING_PLAN_v20.md` (~7568 bytes): rationale for runner script, patch shape, decision matrix, risks (5 enumerated), verification protocol.

#### Plan-criticer (role 2)
- Wrote `docs/PENDING_PLAN_REVIEW_v20.md` (~2595 bytes): KEEP verdict.
- Single-head freshness caveat applies (anti-pattern #7).

#### Impler (role 3)
- Wrote `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` (161 lines / 7232B / 1 new file, 0 source-code modifications).
- Wrote `docs/PENDING_COMMIT_v20.md` (~2862 bytes) with deviation documentation.
- 3 documented mid-flight enhancements (additive, not breaking): spdlog marker counts in evidence summary; deterministic mode ordering via indexed-array; `set -euo pipefail` for fail-fast.

#### Reviewer (role 4)
- Wrote `docs/PENDING_IMPL_REVIEW_v20.md` (~3100 bytes): KEEP verdict.
- Security scan clean, plan-fidelity check matches, deviations documented.

#### Tester (role 5)
- Wrote `docs/PENDING_TESTS_v20.md` (~7864 bytes): 10 staged tests (bash syntax, path resolution, build cleanliness, cerr fire, dump presence, validator verdict, vision analysis, regression carryover, cleanup, idempotency).

#### Testing-verifier (role 6)
- Wrote `docs/PENDING_TEST_AUDIT_v20.md` (~3872 bytes): SOME_RELAX (test surface is parent-driven; cron cannot execute).

### Static disk-evidence audit

- **New file verified on disk**: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` exists, 7232 bytes, 161 lines.
- **HLSL drift = 0**: both GIPathTracing.hlsl copies still 792 lines / 31766B each (v19 patches intact).
- **C++ diagnostic patches intact**: v11/v12 cerr default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:462; 0 `HLVM_FORCE_CERR_LOGGING` references source-wide; v3 spdlog markers intact.
- **No source-code regression**: v5 HLVM-bypass removal intact (NOTE at line 1521), bug-088 fix intact (line 691), bug-075 binding-layout split intact (FGIPass.cpp:277 Add* + lines 506-528 Set*).
- **Binary unchanged**: still at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal`.

### What this script does

The script is a 4-phase bash runner:

1. **Phase 1 (build)**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal` — single Debug rebuild. Tee'd to `rgi_build.log`.
2. **Phase 2 (10 mode runs)**: for each mode in {default, 6, 7, 8, 9, 10, 11, 12, 15, 99}, rotates `dumps/` → `dumps_<mode>/`, runs `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=N HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`, captures stdout/stderr to `rgi_<mode>.{stdout,stderr}`.
3. **Phase 3 (validator)**: restores `dumps_default` → `dumps/` so the validator sees the default-mode dump group; runs `validate_restir_gi.py` and captures to `rgi_validator.log`.
4. **Phase 4 (evidence summary)**: composes `rgi_evidence.txt` with build status, cerr fire counts (default + mode6), v3 spdlog marker counts, per-mode PNG dump counts, and validator 3/3 verdict — all in one human-readable table.

Total wall-clock: ~7-9 minutes. The parent invokes:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

…and pastes the `rgi_evidence.txt` contents back to the cron. The cron then routes to v21+ based on the evidence shape (9 branches defined in PENDING_PLAN_v21 decision matrix).

### Action taken this tick

- Read PENDING_PICK.md, all v19 markers, both GIPathTracing.hlsl copies.
- Wrote 6 v20 markers (PLAN, PLAN_REVIEW, COMMIT, IMPL_REVIEW, TESTS, TEST_AUDIT).
- Wrote the new `run_rgi_diagnostic.sh` runner script.
- Updated PENDING_PICK.md: marked v20 [x]; staged v21 with 9-branch decision matrix.
- Wrote this audit file.
- Did NOT: commit, push, archive, modify governance, create Kanban cards, fabricate evidence.

### Final-goal gate

**FAILED/UNVERIFIED — unchanged.** v20 is a runner-script artifact, NOT a renderer fix. Acceptance criteria still UNVERIFIED (terminal blocked). No `PIPELINE_GOAL_DONE_<date>.md` written.

### Hard invariants verified this tick

- (1) PICK authoritative — yes (v20 PICK item marked [x]; v21 staged).
- (2) Test-files trigger reviewer — runner script is not a test file (it orchestrates an existing test); reviewer ran anyway as a sanity check.
- (3) Impler deviation documentation — yes (3 mid-flight enhancements documented in PENDING_COMMIT_v20.md).
- (4) Plan-criticer FIX loops — N/A (KEEP).
- (5) Single-instance lock — N/A in file-only mode.
- (6) Never silently exit — this audit satisfies it.

---

## v21 cycle — STAGED, awaiting parent-driven evidence

The cron's "continue cycling" instruction, combined with v20 being parent-driven and the absence of fresh terminal access, would normally produce a heartbeat tick. But the user instruction is "until the acceptance criteria are actually met." The 9-branch decision matrix is STAGED in PENDING_PICK.md v21 entry; cron routes to it after parent runs the runner script. Until then, no v21 cycle can fire meaningfully (it depends on parent's `rgi_evidence.txt` shape).

The cron's v21 entry is therefore recorded as STAGED in PENDING_PICK.md (unchecked) and the cycle halts here until parent evidence arrives.

### Stall assessment

- **Intentionally gated, NOT stalled.** v21 is parent-evidence-gated; the diagnostic surface is complete (14 probes) AND the evidence-capture protocol is automated (the runner script). The next step requires parent action that the cron structurally cannot perform.
- Per `six-role-pipeline` HARD INVARIANT #6 ("Never silently exit"), this audit file satisfies the heartbeat requirement.
- Per `software-development-practices` "Don't fabricate findings": without fresh evidence from the runner, no v21 cycle can fire.
- Per `gpu-rendering-bisect-debug` anti-pattern #5: not trusting "PASS" until a human sees the image.

### What unblocks v21

Parent runs:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Then pastes the contents of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/rgi_evidence.txt` back to the cron (or via a new ticket). The cron routes to one of the 9 v21 branches:

1. Accumulate/ReSTIR/denoise investigation
2. AmbientColor uniform fix
3. TraceRay isolation
4. slangc dead-strip investigation
5. debugMode reach investigation
6. divide-by-256 issue
7. View cbuffer investigation
8. stderr buffering investigation
9. -Werror cascade-aware fix

Each branch has a known-shape fix in the v21 plan's decision matrix; the cron will execute the right one based on which line of the parent's `rgi_evidence.txt` shows PASS vs FAIL.

## v20 markers created

- `docs/PENDING_PLAN_v20.md` (7568 bytes)
- `docs/PENDING_PLAN_REVIEW_v20.md` (2595 bytes)
- `docs/PENDING_COMMIT_v20.md` (2862 bytes)
- `docs/PENDING_IMPL_REVIEW_v20.md` (3100 bytes)
- `docs/PENDING_TESTS_v20.md` (7864 bytes)
- `docs/PENDING_TEST_AUDIT_v20.md` (3872 bytes)
- `docs/PIPELINE_HEALTH_2026-07-27_v21.md` (this file)

## v20 source patches landed

+ `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` (1 new file, 7232B / 161 lines)
- 0 source files modified
- 0 lines added/removed from any renderer source

Tick complete. Pipeline trajectory: v0 → v19 (diagnostic surface) → v20 (one-shot runner) → STAGED v21 (parent-evidence-gated fix branch).

**The cron is now at the natural structural pause point**: the only remaining work is parent-driven (run script → paste evidence → cron routes to v21+). No further file-only patches are possible without fresh evidence. Pipeline progress = 0 since last tick; renderer status = BROKEN (unchanged).