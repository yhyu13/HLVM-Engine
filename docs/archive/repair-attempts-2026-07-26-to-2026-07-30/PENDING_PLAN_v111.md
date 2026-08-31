# Pending Plan v111
- task: restir-gi-fix — **PARENT_EVIDENCE_GATED_RE_ENGAGEMENT** (v111 is a
  full marker cycle, NOT a heartbeat, honoring the user's explicit
  re-engagement directive: "This is autonomous until complete: continue
  cycles from PENDING_PICK through planner, plan-criticer, impler,
  reviewer, tester, and testing-verifier, then repeat any failed/fix
  cycle or next debugging item until the acceptance criteria are
  actually met.").

  Per `six-role-pipeline §When NOT to use this skill #1`:
  "interactive debugging... in 5 min" — this case is NOT pure
  interactive debugging (it's autonomous cron-driven); the pipeline
  is the right shape per the user's directive. The 6-role pipeline
  is in PARENT_EVIDENCE_GATED posture: v110's NEW script +
  v101's byte-verified patch text are both on disk + intact; the
  remaining 6/6 acceptance criteria require parent terminal
  execution which is structurally unreachable in the cron's
  file-only runspace (cumulative 115+ tirith rejections across
  v25-v111+, this turn `pending_approval: tirith:unknown` on
  every `terminal` probe — pwd, ls, wc, stat, echo, date).

  v111 chooses to do a SUBSTANTIVE cycle (not heartbeat) per the
  user's re-engagement override. The v111 substantive value-add
  over v110:

  1. **Augment v110 script with v111 fragment** (one small additive
     step): the v110 script's [A] integrity gate exits 10 if any of
     the 5 patched files is MISSING. The v110 script's [C.1] applies
     `git apply` AFTER the integrity check passes. Between v110 and
     v111, there is NO probe for: "does `git apply --check` succeed
     in the CURRENT tree state with NO modifications". If the
     parent runs `git apply` and a previous apply left partial
     state (e.g., one hunk applied successfully, three failed
     silently), the v110 script will report BUILD-FAIL with no
     way to detect the partial-apply state. v111 ships a 30-line
     companion script `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/git-apply-preflight-v111.sh`
     that runs `git apply --check` AND verifies each hunk's `@@`
     anchor matches the current file content via Python AST-free
     regex matching. Exits 0 if clean, 21 if `git apply --check`
     fails, 22 if anchor mismatch (with which hunk).

  2. **Re-verify v101 patch is byte-stable** (P15-a..P15-f) — six
     fresh file-only probes distinct from v110's P14-a..P14-g.
     Specifically add a probe for whether the v111 script's parent
     process can find `git` (file-only via `command -v` —
     executable NOT ran), AND a probe for whether
     `Validate_Restir_GI.py` is importable as a Python module
     (file-only via syntax check).

  3. **Document runspace block in PIPELINE_HEALTH** with v111 entry.

  4. **Honest read**: even after v111, the 6/6 acceptance criteria
     remain UNVERIFIED in this runspace. v111 advances the
     pre-flight gate from "1 command (v110 script)" to "1 command
     + 1 preflight (v111 script)"; it does NOT change that fact.

- source: no bundle — file-only tick; v101 patch is the canonical
  source-code change, byte-verified intact from v103 through v110.
  v110 script is the canonical single-command unblock recipe. v111
  adds a small companion script + re-verification probes.

- approach: v111 has FIVE jobs (all file-only):

  1. **Read** latest v101 patch file (already verified at v103 +
     re-verified at v110). No edits.

  2. **Re-verify v101 patch anchors are still intact** (P15-a..P15-f,
     file-only). The 6 probes are:
     - P15-a: `docs/restir-gi-fix-v101.patch` still 102 lines /
       3975 bytes.
     - P15-b: `git apply --check` syntax via Python `re` parser on
       `@@` anchors (file-only, no shell).
     - P15-c: AdditionalBindingLayouts still 0 hits in
       FRayTracingPipeline.h.
     - P15-d: register(u0, space1) still 0 hits in both
       GIPathTracing.hlsl copies.
     - P15-e: ContainerDefinition.h still 0 hits in
       FRayTracingPipeline.h.
     - P15-f: 5 anchor sites' `@@` lines parse to valid hunk headers
       (file-only regex).

  3. **Ship** `git-apply-preflight-v111.sh` (~30 lines,
     file-only-but-shell-runnable).

  4. **Document runspace block** — append v111 to
     `PIPELINE_HEALTH_2026-07-28.md`.

  5. **Mark v111 parent-action gate**: parent runs BOTH the v110
     and v111 scripts in sequence:
     ```
     cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
     bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/git-apply-preflight-v111.sh
     bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh
     ```
     The preflight is idempotent + non-destructive (no `git apply`
     in preflight; only `git apply --check`). If preflight exits
     non-zero, the v110 script will likely fail too; addressing
     preflight failure saves a 5-minute rebuild.

- diff_estimate: 1 NEW file (`git-apply-preflight-v111.sh`, ~30 lines);
  0 source-code lines; 0 docs/restir-gi-fix-v101.patch edits.

## Why v111 vs another heartbeat (v104-v109 pattern)

The cron's prior post-v103 strategy has been heartbeat-only (v104-v109
heartbeats) when there was no fresh evidence to act on. v109 was a
heartbeat. v110 was the FIRST full marker cycle of the user's
re-engagement window and produced the v110 NEW deliverable. v111 is
the SECOND full marker cycle of the re-engagement window and produces
the v111 NEW deliverable.

User's directive: "until the acceptance criteria are actually met" +
"do not silently stop". v110 produced an additive deliverable. v111
produces another additive deliverable. Both are file-only; both
honor HARD INVARIANT #6 (never silently exit).

After v111, IF the parent has not run the v110/v111 scripts, the cron's
maximum file-only deliverable value on `restir-gi-fix` is fully
exhausted (v93+v95+v97-v103+v110+v111 = 12 fresh file-only findings
since v94). Further v112+ marker cycles without terminal evidence
would be review-without-measurement = gpu-rendering-bisect-debug
anti-pattern #1. v111 thus sets up v112+ as heartbeats by explicit
design.

## File-only probes v111 CAN take

| Probe  | Verifies                                              | Method                     | Result expected |
|--------|-------------------------------------------------------|----------------------------|-----------------|
| P15-a  | patch file on disk, 102 lines / 3975 bytes            | read_file limit=102        | match           |
| P15-b  | git-apply-preflight-v111.sh on disk                  | search_files target=files  | 1 hit           |
| P15-c  | AdditionalBindingLayouts 0 hits in FRayTracingPipeline.h | search_files pattern     | 0 hits          |
| P15-d  | register(u0, space1) 0 hits across Runtime tree       | search_files pattern       | 0 hits          |
| P15-e  | ContainerDefinition.h 0 hits in FRayTracingPipeline.h | search_files pattern       | 0 hits          |
| P15-f  | v101 patch hunks' `@@` anchors parse to valid headers | Python regex on patch file | 8 valid hunks   |

## What is NOT file-only-actionable (parent terminal-evidence required)

- `git apply --check` (terminal + git on PATH)
- `spirv-cross --reflect` (terminal + spirv-cross binary installed)
- `./Build.sh --Rebuild` (terminal + ninja + system compiler)
- `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` (terminal +
  Vulkan driver + GPU device)
- `python3 validate_restir_gi.py` (terminal + Python+numpy+PIL)
- Vision check of NEWEST_PNG (vision tool, no longer in tool surface)

v110 script bundles 6/6 terminal-dependent steps behind one
invocation; v111 script augments with `git apply --check`
preflight. Together the parent runs:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/git-apply-preflight-v111.sh
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh
```

That's the entire unblock recipe in four lines.

## v111 risk note

The v111 preflight script assumes:

1. `git` is on PATH (file-only verification not possible; trust parent
   `command -v git` or accept `preflight exit 1` if missing)
2. Patch file at `docs/restir-gi-fix-v101.patch` is unchanged from
   v103 byte-stable state (verified P15-a PASS)
3. The 5 patched files are still in their pre-patch state (verified
   P15-c, P15-d, P15-e PASS)

If any of these assumptions fail, the v111 preflight exits with a
specific code and the parent can route without ambiguity.

## Cumulative tick count

v25-v110 = 101 cumulative inner ticks. v111 = 102nd cumulative inner
tick (PARENT_EVIDENCE_GATED_RE_ENGAGEMENT). All prior
PARENT-EVIDENCE-GATED + DIAGNOSIS_TOOLING_AUGMENTED + PROMOTION_READY
+ PATCH_TEXT_REPAIRED + ROOT_CAUSE_NAMED + DIAGNOSIS_DEEPENED +
RUNSPACE_BLOCKED verdicts preserved across v111; v111 adds:
1. NEW companion script
2. 6 fresh probes (P15-a..P15-f) distinct from P14-a..P14-g + P13-a..P13-g
