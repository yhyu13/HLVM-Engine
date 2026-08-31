# Pending Test Audit v216

- tests: docs/PENDING_TESTS_v216.md
- commit: docs/PENDING_COMMIT_v216.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-564)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] Every load-bearing zero controlled by a same-shape positive (v205) — rows 3/4 and 8/9/10
- [x] No count inherited across markers without re-derivation (v211) — I re-ran rows 8, 12, 15 myself
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199 / v215 both directions) — see below
- [x] No absence asserted where a scope must be read (v198)
- [x] No runtime result fabricated — nothing built, run, or viewed by any role this cycle

## Rows I re-ran, and one the tester did not think to run

Row 12 (block membership) is the finding's single point of failure, so I re-derived it by a *third* query
shape — `pattern="timeout: 60"` with context, which anchors from the line *between* the two claims:
`:472 approvals:` / `:473 mode: manual` / `:474 timeout: 60` / `:475 cron_mode: allow`. Contiguous. Note this
query also returns `timeout: 60` at `:242` and `:254` under unrelated model blocks — **a same-token control
proving the query is not simply matching the region I expected**. Membership confirmed a third time.

**The row nobody ran:** a global config value can be overridden per-job, and `cron_mode` living in the global
file says nothing about job `c6abd4d5fc39`'s effective value. I checked —
`search_files pattern="cron_mode" path=~/.hermes/cron` → **0 hits** (directory scope, per v199/v215), against
`pattern="enabled_toolsets"` on the same tree → 28 hits, so the zero is controlled and the directory scope is
not the v215 trap firing again. **No per-job override exists; the global `allow` is operative for this job.**
Had an override existed with a different value, the entire finding would have collapsed. It does not.

## Per-row verdict

**20/20 KEEP**, plus the two rows I added above. Rows 4, 10 and 18 carry the cycle:

- **Row 4** — turns rows 3/5/6/7 from bare zeros into evidence. Without it, "`tirith` is absent" is exactly
  the vacuous negative that invalidated 562 ticks.
- **Row 10** — the `ZZZ_NO_SUCH_FLAG` control. Rows 8-9 are *positive* claims, and the reflex is to skip a
  control for a positive; the tester ran one anyway, which is what makes "this file contains `cron_mode`"
  distinguishable from "this file matches whatever it is asked for".
- **Row 18** — the row that limits the conclusion. It is the reason this cycle reports a *no-op remedy*
  rather than a *runtime defect*, and it is recorded as a PASS that constrains rather than supports.

## What this cycle established, and what it did not

**Established (file-only, sound):**
1. `terminal` is blocked at the tool boundary for *every* command shape including `true`; the signature is
   `pending_approval` / `smart_endpoint denied: false` / `allow_permanent: true` — an unanswered approval
   prompt, not a permission denial.
2. `tirith` is absent from all four standard binary paths (controlled).
3. **NET-NEW: `approvals.cron_mode: allow` (`:475`) and `security.tirith_fail_open: true` (`:487`) are both
   already set the way v215's remedy asks** — verified inside their blocks by three independent query shapes,
   with no per-job override anywhere in `~/.hermes/cron/`.
4. **Therefore v215's prescribed operator action is a no-op.** An operator following it finds nothing to change.
5. This session is a tick of cron job `c6abd4d5fc39` (prompt byte-matches the instruction).

**NOT established — and deliberately not claimed:** that the runtime ignores those two flags, or that there is
a runtime defect. No interpreting code is readable from this runspace (row 18). The finding is about the
*remedy*, not about the *runtime*.

**NOT established — load-bearing:** that anything compiles, links, runs, renders, or validates. All seven
acceptance gates remain unreached. The v183-v215 chain (32 source patches across 33 cycles) is still unbuilt.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged; cause corrected again)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | terminal blocked; approval never answerable in cron |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | UNKNOWN | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 33 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or touch governance files —
including `~/.hermes/config.yaml`, which was **read only**. Did not modify any engine source. Did not
fabricate any runtime result.
