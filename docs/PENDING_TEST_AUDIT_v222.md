# Pending Test Audit v222

- tests: docs/PENDING_TESTS_v222.md
- commit: docs/PENDING_COMMIT_v222.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-570)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] No `file_glob` in any load-bearing query (v217)
- [x] **No content-mode query under `~/.hermes` treated as evidence (v219)** — and this cycle found a
      five-cycle-old FALSE NEGATIVE created by exactly that violation
- [x] Every load-bearing zero paired with a same-shape positive control (v217)
- [x] No count inherited across markers without re-derivation (v211)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No runtime result fabricated — nothing built, run, viewed, or executed by any role

## Rows I re-ran rather than read

**Row 1, because it overturns five cycles.** I did not accept the tester's hit. Re-ran
`target=files` on `~/.hermes` for `tirith*` → 1 hit; then the control on `~/.hermes/bin` → `uv`,
`uvx`, `tirith`. Then the negative control the tester did NOT run: `*install*fail*` → 0 hits on the
same tree with the same *working* query mode, so that zero is trustworthy where a content-mode zero
would not be. **The binary is present, executable-path-resolvable at `tirith_security.py:685-690`,
and unmarked as failed. v215's central negative is false.**

**Row 12, the emitter.** The reviewer upgraded the impler's structural region to an exact site. I
tested that upgrade rather than adopting it, by trying to falsify uniqueness: searched the module for
`approval_pending` and for `status` and confirmed `:2999-3012` is the only return carrying both. The
`allow_permanent: true` reconciliation holds — `:2912` is gateway-path, `:2983` is the cron fallback,
and on the fallback the key is simply absent unless smart-denied (`:3011`).

**Then the row nobody wrote, which decides whether this is host policy or an agent defect.** The
tester proved `HERMES_EXEC_ASK=1` is set unconditionally in `gateway/run.py:1791`. It did not check
whether the *sibling* operands of the same conjunction are cron-aware. They are — asymmetrically:

`_is_gateway_approval_context` (`:227-245`) opens with `if env_var_enabled("HERMES_CRON_SESSION"):
return False`, and its docstring states the reason in the author's own words — *"letting cron fall
through to the gateway branch would submit a pending approval with no listener and block the job
indefinitely."*

**That is a precise description of what this job has done 570 times.** The author identified the
failure mode, hardened `is_gateway` against it, and left `is_ask` — evaluated on the next line at
`:2694`, in the same conjunction at `:2698` — unguarded. `HERMES_CRON_SESSION` has four call sites
(`:241`, `:2173`, `:2700`, `:3121`); the terminal path is the only one whose gate carries the extra
`not is_ask` operand without a cron check on it.

## Per-row verdict

**17 PASS / 17 KEEP.** Rows 1, 12 and 15 carry the cycle.

- **Row 1 is the finding of record.** Six cycles (v215, v216, v219, v220, v221 and every closure doc
  citing them) built on "the tirith binary does not exist." It exists. The false zero came from
  content-mode `search_files` on `~/.hermes` — the single directory this lineage has separately
  documented as returning false zeros. The premise was never re-tested because it had been recorded
  as settled.
- **Row 15 closes the latent-remedy question empirically.** The bare, allowlist-eligible command was
  actually attempted this tick and refused identically. So v221's remedy is not silently already
  working; it needs an operator to add the entry.
- **Row 12 + the cron-guard asymmetry together reclassify the blocker.** It is not a permission
  denial and not a missing scanner. It is a conjunction with two hardened operands and one unhardened
  one, resolving to a pending approval that no cron listener can ever answer.

## What this cycle established, and what it did not

**Established (file-only, every load-bearing line read as a contiguous range, every zero controlled):**
1. The tirith binary exists and resolves — the lineage's central negative is false.
2. The refusal is emitted at `approval.py:2999-3012`, below all three approve-gates.
3. `approvals.cron_mode` is structurally inert on this path; `mode: off` and `command_allowlist` are
   above the emitter and would work, the latter only after an operator adds the entry.
4. The proximate cause is `HERMES_EXEC_ASK=1` (`gateway/run.py:1791`) making `:2698` false, with the
   sibling operand cron-guarded and this one not.

**NOT established, load-bearing:** that anything compiles, links, runs, renders or validates. **The
v183-v222 chain remains unbuilt.** This cycle changed the diagnosis and the remedy's justification.
It did not move a single acceptance gate.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused in 4 shapes this tick |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 39 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine
source or governance file. **Did not modify `tools/approval.py`, `gateway/run.py`,
`tirith_security.py`, or `~/.hermes/config.yaml`** — a cron job must not patch the agent executing it
nor widen its own permissions. Did not fabricate any runtime result.
