# Pending Test Audit v220

- tests: docs/PENDING_TESTS_v220.md
- commit: docs/PENDING_COMMIT_v220.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-568)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] No `file_glob` in any load-bearing query (v217)
- [x] No directory-scoped query under a dotfile directory treated as evidence (v219) — the one query
      against `~/.hermes/bin` is `target=files`, a different code path, and it returned 3 hits, not 0
- [x] Every load-bearing zero paired with a same-shape, same-scope positive control (v217)
- [x] **No count inherited across markers without re-derivation (v211)** — violated by the impler this
      cycle, caught by the reviewer, re-verified here. See below.
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No runtime result fabricated — nothing built, run, viewed, or executed by any role this cycle

## Rows I re-ran rather than read

**Row 10 — the reviewer's correction of the impler.** Re-run independently:
`path=tools/approval.py pattern="load_permanent_allowlist"` → **2 hits** (`:1702` def, `:3390` call),
confirming the reviewer and refuting the impler's 13. Then the part neither checked: `:3390` is the
**final line of a 3390-line file**, at column 0, under the comment `"Load permanent allowlist from
config on module import"`. Module-scope, unconditional, no `if __name__` guard — read as a contiguous
range `:3379-3390`, because a bare grep hit cannot distinguish module scope from a nested call, and
that distinction is the entire claim. **Confirmed.** The rejection of the allowlist remedy therefore
rests on ground (i) alone, exactly as the review states — and ground (i) I re-derived too: `:1660`'s
regex contains `&&` and `:1678` returns False on match, so the compound acceptance command is
categorically ineligible.

**Row 13 — the tester's third-ordering finding, which carries the cycle.** Re-derived the call-site set
(`HERMES_CRON_SESSION` → 4 hits, unchanged) and read `:2160-2183` myself. Confirmed `:2171` guards on
two operands where `:2698` guards on three, with byte-identical comments above each. **Then checked the
one thing the tester asserted without testing**: it observed that `:2171` has an `elif
fail_closed_when_no_human:` (`:2182`) and `:2698` has none, and read that as "not written to the same
contract." Verified the parameter is real and live: `fail_closed_when_no_human` → **5 hits** —
declaration `:2108` (default `False`), docstring `:2138`/`:2145`, the branch `:2182`, and **exactly one
caller passing `True`: `:2480`, the plugin-escalation path**, whose `cron_deny_message` at `:2470-2474`
names `approvals.cron_mode` explicitly. So the `:2171` site is a *parameterised, documented* treatment
of the no-human case, and `:2698` is an ad-hoc one. The tester's reading is upheld on evidence it did
not itself gather.

**The `pattern_key` cross-check that ties this to what we actually received.** `:2480` is the second of
the two sites v219 identified as constructing the seven-field pending dict, and v219 excluded it
because our `pattern_key` was `tirith:unknown` rather than a plugin rule id. That exclusion still
holds, and it now has a second, independent confirmation: `:2480` passes `fail_closed_when_no_human=True`,
so under a cron session with `cron_mode: allow` it would have fallen through to auto-approve at `:2181`
rather than returning pending. **Our refusal came from the `:2698` path. The one site with three
operands is the one that fired.**

## Per-row verdict

**13 PASS / 1 of them corrected upstream and re-confirmed here / 2 rows strengthened — all 13 KEEP.**
Rows 10, 12 and 13 carry the cycle.

- **Row 13 is the finding.** Not "two functions disagree" (commit) nor "the ordering is inverted"
  (review), but: **one policy, four call sites, three gate shapes, and the only site with the extra
  `not is_ask` operand is the terminal path.** The variable that operand tests is set unconditionally
  at gateway import (`gateway/run.py:1791`, module scope, verified col 0). That conjunction is why
  `approvals.cron_mode: allow` — correct in the config for 568 ticks — has never been reached.
- **Row 10 is the process finding.** An inherited count nearly rejected a remedy for a false reason.
  Third occurrence of "counts are not invariants, sets are" in four cycles. It was caught, twice, by
  roles whose job is to not take the previous role's word — which is the pipeline working as designed
  on a single-profile host, and the one thing the split has demonstrably bought here.

## What this cycle established, and what it did not

**Established (file-only, controlled, every load-bearing line read as a contiguous range):**
1. v219's stated reason for leaving candidates (A)/(B) unseparated is refuted: the separation does not
   require a runtime environment read, because the four cron call sites constrain the control flow.
2. Both candidates collapse to one statement and one remedy shape, so naming the operand is unnecessary.
3. The remedy is `approvals.mode: off` — the only early return above `:2698` that both approves and is
   config-reachable — with wiring verified end-to-end including the YAML bare-`off` bool hazard.
4. `command_allowlist` is **not** a viable remedy, on the compound-command guard, not on reachability.
5. The four-site/three-shape divergence is a defect in the agent, reported and deliberately not patched.

**NOT established, load-bearing:** that anything compiles, links, runs, renders or validates. The
v183-v220 chain remains unbuilt. This cycle changed the *remedy and its justification*, not any gate.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused; even `date` → `pending_approval` |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 37 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine
source or governance file. **Did not modify the `hermes-agent` source or `~/.hermes/config.yaml`** — a
cron job must not patch the agent executing it, nor silently widen its own permissions; both findings
are reported for the operator to act on. Did not fabricate any runtime result.
