# Pending Test Audit v221

- tests: docs/PENDING_TESTS_v221.md
- commit: docs/PENDING_COMMIT_v221.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-569)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] No `file_glob` in any load-bearing query (v217)
- [x] No dotfile-directory content query treated as evidence (v219) — and this cycle produced a
      *fresh demonstration* of the hazard rather than merely asserting it
- [x] Every load-bearing zero paired with a same-shape positive control (v217)
- [x] No count inherited across markers without re-derivation (v211) — **not violated this cycle**,
      the first clean cycle on this row in four
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No runtime result fabricated — nothing built, run, viewed, or executed by any role

## Rows I re-ran rather than read

**Rows 2-3, the probe result.** Re-read both tool envelopes field by field rather than comparing
summaries: `status`, `exit_code`, `pattern_key`, `smart_denied`, `allow_permanent` — all five
identical across foreground, `background=true`, and `pty=true`. **Confirmed.** And I checked the
impler drew the *right* conclusion from it: a negative result that upholds 568 ticks of prior
claims is exactly the kind a role is tempted to inflate into a discovery. The commit marker reports
it as "the claim is upheld — but it was luck, not rigour." That is the honest reading and it is
rare enough to note.

**Row 10, which carries the cycle, because it overturns the previous cycle.** I did not accept the
impler's tokenisation or the reviewer's. Took the regex's five alternatives and searched the
candidate string for each independently: `\n` — absent, single line; `&&` — absent; `||` — absent;
the class `[;&|<>\`]` — no `;`, `&`, `|`, `<`, `>` or backtick anywhere in
`./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`; `$(` — absent, no `$` at all.
**Five for five. The command is eligible, and v220's "categorically ineligible" is wrong.**

**Then the thing neither the tester nor the reviewer checked: is eligibility sufficient?** No — and
the tester's row 4 quietly proves it. The bare command was *run* this tick and still refused,
because `command_allowlist` currently holds one unrelated entry (`config.yaml:479 - script execution
via -e/-c flag`) and `:1690`/`:1692` match by exact string or fnmatch. Neither matches. **So the
remedy requires an operator to add the entry; it is not latent-working.** The commit marker says
this ("eligibility is not the same as membership") and I confirm it is not overstated anywhere.

## Per-row verdict

**14 PASS / 14 KEEP.** Rows 2-3, 10 and 6 carry the cycle.

- **Row 10 is the finding of record.** It corrects the immediately preceding cycle on a point that
  cycle treated as settled, and the correction *widens* the operator's options from one remedy to
  two. The narrower remedy is materially better: `approvals.mode: off` disables approval for every
  command in every session on this host; an allowlist entry authorises exactly one command string.
- **Row 6 is the strongest evidence in 569 ticks that this is an agent defect, not a host policy.**
  The author guarded `is_gateway` against cron *and wrote down the precise failure mode* — "submit a
  pending approval with no listener and block the job indefinitely" — that the unguarded `is_ask`
  beside it then produces. The lineage has spent hundreds of ticks characterising this as a
  permission denial. It is a conjunction with one hardened operand and one unhardened one.
- **Row 2-3 is the process finding.** 568 ticks asserted a *categorical* block from a *single*
  invocation shape. The assertion happened to be true. It was not entitled to be.

## What this cycle established, and what it did not

**Established (file-only, controlled, every load-bearing line read as a contiguous range):**
1. The terminal block is genuinely execution-mode-independent — now tested across three shapes.
2. v220's rejection of the `command_allowlist` remedy is refuted; the disqualifying `&&` was an
   artifact of the lineage's own phrasing, made redundant by the job's own `workdir`.
3. Two remedies now exist, one strictly narrower and safer than the other.
4. The defect is an omitted cron guard on one operand of `:2698`, with the sibling operand's guard
   and docstring serving as the author's own statement of intent.

**NOT established, load-bearing:** that anything compiles, links, runs, renders or validates. The
v183-v221 chain remains unbuilt. **This cycle changed the remedy set and its justification, not any
acceptance gate.**

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused in all 3 execution modes this tick |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 38 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine
source or governance file. **Did not modify `tools/approval.py`, `gateway/run.py`, or
`~/.hermes/config.yaml`** — a cron job must not patch the agent executing it nor widen its own
permissions; `approval.py:262-275` names that exact scenario as the reason the config is a guarded
write target. Both remedies are reported for the operator. Did not fabricate any runtime result.
