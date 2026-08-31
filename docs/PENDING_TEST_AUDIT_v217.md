# Pending Test Audit v217

- tests: docs/PENDING_TESTS_v217.md
- commit: docs/PENDING_COMMIT_v217.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-565)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] No `file_glob` in any load-bearing query (v217, this cycle)
- [x] Every load-bearing zero controlled by a same-shape, same-scope positive (v217)
- [x] No count inherited across markers without re-derivation (v211) — I re-ran rows 10 and the card-premise row
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No absence asserted where a scope must be read (v198)
- [x] No runtime result fabricated — nothing built, run, or viewed by any role this cycle

## Rows I re-ran

**Row 10** is the row that carries the cycle, so I re-derived it rather than reading it:
`path=~/.hermes/cron pattern="enabled_toolsets"` → **0**. v216's audit recorded **28** on that identical
tree with that identical pattern one tick ago. Confirmed dead.

**The card-premise row** the tester added: `path=FGIPass.cpp pattern="AddTextureSRV"` → **3** at
`:331 (1) // t1 - GBufferWorldPos`, `:332 (2) // t2 - GBufferNormal`, `:333 (3) // t3 - GBufferMaterial`,
pairing N-for-N with `SetTextureSRV` at `:608-610`, offsets zeroed at `:326`. Reproduces.

## The row nobody ran — and it is worse than the finding the cycle was built on

Every role this cycle demonstrated the false zero on `~/.hermes`, i.e. **outside** the project tree. The
unasked question is whether the project tree — where all 564 ticks made their engine claims — is affected.
It is.

- `path=<project root> pattern="SetBindingOffsets"` → **5 hits, ALL of them `docs/*.md`.**
- `path=<root>/Engine/Source/Runtime pattern="SetBindingOffsets"` → **5 hits, ALL of them source**:
  `TestReSTIR_GI_Temporal.cpp:1988/:2508/:2561`, `FGIPass.cpp:326`, `FBindingLayoutBuilder.cpp:30`.

**Two disjoint 5-hit result sets for the same pattern, neither a subset of the other.** The project-root
walk silently omitted every `.cpp` occurrence — including `FGIPass.cpp:326`, the exact line the card is
about — and returned a plausible-looking non-zero count instead. This is strictly more dangerous than the
zeros diagnosed upstream: **a zero invites suspicion, a wrong non-zero does not.** A tick that ran the
root-scoped query would have concluded the offsets fix exists only in documentation.

The tester's own observation belongs here too: tick-527 cited `:306-308` / `:583-585`; the same bindings
are now at `:331-333` / `:608-610`. **Counts were invariant across 39 cycles of source patches; line
numbers were not.** Any row citing the old numbers would read as a false negative today.

## Per-row verdict

**20/20 KEEP**, plus the tester's unspecified row and the two I added. Rows 5, 7, 10 and 12 carry the cycle:

- **Row 12** (`/usr/bin` `git*` → 4) is the row that makes this cycle honest. It falsified the planner's
  first mechanism at the gate, before anything was built on it. Without it, v217 would have shipped
  "directory scope fails outside the project root", which the root-vs-Runtime result above also refutes.
- **Rows 5 and 7** convert "0 hits" from a conclusion into a symptom, by exhibiting the token.
- **Row 10** is the one with consequences for the lineage: the control v216 leaned on is invalid.

## What this cycle established, and what it did not

**Established (file-only, controlled):**
1. `search_files` returns false zeros by at least two mechanisms — unflagged search timeouts on large
   trees, and `file_glob` suppression — and additionally returns **false partial results** on wide
   directory walks (the root-scoped case above), which no prior tick had identified.
2. The standing rules from tick-526, v199 and v215 each fixed the wrong variable. The replacement —
   **every load-bearing negative needs a same-shape, same-scope positive control; never `file_glob`** —
   is the first that catches all observed cases, and it caught a defect in the impl review that was
   auditing it.
3. v216's conclusion (v215's operator remedy is a no-op) **survives**; v216's **support for it does not**.
   Re-derived file-scoped with a same-file control: no per-job `cron_mode` override, global `allow`
   operative.
4. `terminal` is refused in **both** foreground and background invocation shapes — net-new; 564 prior
   ticks probed only foreground.
5. The card's own premise — GBuffer SRV bindings unbound — remains refuted on first-hand, controlled,
   current-line-number evidence.

**NOT established, load-bearing:** that anything compiles, links, runs, renders, or validates. The
v183-v216 chain (33 source patches across 34 cycles) remains unbuilt and unexecuted.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused, both invocation shapes |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | UNKNOWN | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 34 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine
source or governance file; `~/.hermes/config.yaml` and `~/.hermes/cron/jobs.json` were **read only**.
Did not fabricate any runtime result.
