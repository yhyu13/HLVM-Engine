# Pending Test Audit v215

- tests: docs/PENDING_TESTS_v215.md
- commit: docs/PENDING_COMMIT_v215.md
- verdict: **ALL_KEEP**
- verifier: agent_6_testing_verifier (tick-563)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526) — and the impler disclosed one near-miss inside this cycle
- [x] Every zero controlled by a same-shape positive (v205) — rows 7/8
- [x] No count inherited across markers without re-derivation (v211) — I re-ran rows 3, 4, 5 myself
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199) — **and this row fired, see below**
- [x] No absence asserted where a scope must be read (v198) — the cycle's entire subject
- [x] No runtime result fabricated — nothing was built, run, or viewed

## Row 5 — I could not reproduce it at first, and the reason IS this cycle's thesis

The tester recorded row 5 (all four jobs grant `terminal`) from a `read_file`. Re-deriving by query:

- `search_files pattern="enabled_toolsets" path=~/.hermes/cron` (**directory** scope) → **0 hits**
- `search_files pattern="enabled_toolsets" path=~/.hermes/cron/jobs.json` (**file** scope) → **4 hits**
  (lines 43, 88, 134, 180); `pattern="terminal"` at file scope → 4 declarations (44, 89, 135, 181)

Same tool, same pattern, same content — **directory scope returned zero, file scope returned four.** This
is v199's known trap (`path`-at-a-file / path-at-a-directory asymmetry) firing in the opposite direction
from how it was originally written down.

I nearly recorded "row 5 UNSUPPORTED". Had I done so, this cycle would have produced a *false negative
about the correction to a false negative* — the identical error, one meta-level up, in the very cycle
diagnosing it. **Row 5 stands: 4/4 jobs declare `["terminal","file"]`.**

This retires any remaining confidence in bare zeros from this toolchain. Three independent
zero-producing mechanisms are now documented: `|` alternation (tick-526), directory-vs-file scope (v199,
both directions), and wrong-root scope (this cycle). A zero from `search_files` is not evidence until the
same query shape has produced a hit somewhere.

## Per-row verdict

**12/12 KEEP.** Rows 1, 6, 7-8 carry the cycle:

- **Row 1** — the file the lineage declared absent, read in full.
- **Row 6** — the stored prompt of job `c6abd4d5fc39` is byte-identical to this session's instruction.
  This is what converts the finding from "a cron exists" to "**this session is that cron**", which no
  prior tick could have concluded while claiming to be a parent-session emission.
- **Rows 7-8** — the controlled zero. Row 7 alone would have been another uncontrolled negative.

## What this cycle established, and what it did not

**Established (file-only, sound):**
1. `~/.hermes/cron/jobs.json` exists with 4 jobs; 2 are `enabled: true, state: scheduled`
   (`c6abd4d5fc39` completed **3543** runs, `fdc2760d58cb` **4702**).
2. All 4 declare `enabled_toolsets: ["terminal","file"]` — the "file-only host" premise is false.
3. This session is a tick of `c6abd4d5fc39` (prompt byte-identical).
4. `terminal` returns `pending_approval` / `smart_denied: false` / `allow_permanent: true` — an
   **unanswered manual-approval prompt**, not a permission denial.
5. `approvals.mode: manual`, `timeout: 60`; `tirith_enabled: true` with `tirith_path: tirith` pointing at
   a binary that **does not exist** on this system (controlled zero).
6. The 562-tick "pipeline DORMANT / no jobs.json / terminal blocked" conclusion is **refuted on all three
   claims**, and the prescribed remedy ("register a cronjob", "widen toolsets") would not have worked.

**NOT established — load-bearing:** that anything compiles, links, runs, renders, or validates. All seven
acceptance gates remain unreached. The v183-v215 chain (32 source patches) is still unbuilt.

**Severity: HIGH, and it is diagnostic rather than functional.** No pixel moves. What changes is that the
operator's remedy was wrong for 562 ticks: the recommendation was "register a cron / widen permissions"
when the cron was already live with terminal already granted, and the actual blocker is that
`approvals.mode: manual` cannot be satisfied by an unattended session whose security scanner is missing.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged — with corrected causes)

| # | Gate | Status | Corrected basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | approval never granted — not "toolset denied" |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | UNKNOWN | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 32 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or touch governance files.
Did not modify any engine source. Did not fabricate any runtime result.
