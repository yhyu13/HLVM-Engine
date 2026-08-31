# Pending Test Audit v202

- tests: docs/PENDING_TESTS_v202.md
- commit: docs/PENDING_COMMIT_v202.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-548)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-ran rows 9, 13, 14 myself
- [x] No source-incomplete-relative-to-test — every row names path and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-directory misuse for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive — rows 1/2, 5/6, 13/14
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted by a query where a scope must be read (v198)
- [x] No conclusion resting on a hit-count where the hits are comments (v200)
- [x] No enumeration resting on a convenience wrapper (v201)
- [x] **No claim of the form "X is never used" resting on a symbol count where a
      usage-shaped query exists (v202, new)** — see below

## Independent re-derivation

**Row 13/14 re-run.** `path=Binary/Debug pattern="00344"` → **0**. Control:
`pattern="VUID"` → **23**, and I took the per-file breakdown rather than the
total: `TestReSTIR_GI_Temporal_2.log` 8, `_1.log` 10, `TestPathTraceGI_1.log` 5,
and **`TestReSTIR_GI_Temporal.log` (the current run) 0**. So the instrument
demonstrably fires in that directory and the `00344` zero is a real negative.
The impler's bug-075 "no alias, and it has never fired" claim stands on evidence,
not on a vacuous search.

**Row 9 re-run, and I improved the instrument rather than repeating it.** The
tester read the control shader's `main` in full to establish that no GBuffer
texture is sampled. Correct, but a *usage-shaped* query settles it more sharply:
`pattern="Load"` in that file → exactly **2 hits**, `:93` and `:114`, **both
`gRadiance`**. That is strictly better than reading a body, because it enumerates
every sampling operation in the file at once and cannot miss one further down.
`gWorldPos`/`gNormals`/`gDepth` are declared and never sampled — confirmed by an
exhaustive query, not by inspection.

This is the correct discharge of the reviewer's note to the impler, one role
later.

## New checklist row

Row 15 generalises what row 9 exposed. The impler asserted "never Loaded" from
**symbol counts of 1 per texture**. A 1-hit count proves the symbol occurs once;
it does not prove that occurrence is a declaration rather than a use. The
reviewer caught this and read the body; the correct instrument is neither —
query the **operation** (`Load`, `Sample`) and enumerate its operands.

Distinct from row 14 (v201), which concerns enumerating a *wrapper* instead of
the operation. This concerns enumerating the *operand* instead of the operation.
Both fail the same way — a complete-looking enumeration over the wrong set — but
they are different wrong sets.

## Per-row verdict

**18/18 KEEP.** Rows 2, 6, 9 and 14 carry the cycle; all four were re-derived
here independently, and row 9 was strengthened.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. A **shared binding layout is inconsistent with one of its two consumers'
   shaders**: `GenerationLayoutSRV` declares `Texture_SRV(4)` unconditionally
   while the `TestCornellBoxGI_Data` generate shader declares t0-t3 only (4
   registers vs the primary's 5). Latent, because the `DirectionTexture ?:
   RadianceTexture` fallback keeps the descriptor populated and the control never
   sets that field.
2. This is a **new invariant class** for the codebase, invisible to every
   dual-copy check in the lineage because all of those are *sameness* checks and
   these two copies are correctly different. The invariant: for a shared binding
   layout, every consumer's shader must declare every binding the layout
   declares.
3. The ReSTIR **generation** pass is structurally immune to the v183 half-res
   class — it samples no GBuffer texture in either copy (exhaustive `Load`
   enumeration: 2 hits, both `gRadiance` in the control; primary's `main` reads
   only `gRadiance`/`gDirection`), and both of its live inputs are half-res like
   its dispatch.
4. The **spatial** layout's missing SRV/UAV split is safe, not an oversight: no
   texture is reachable as both SRV and UAV at its call site, and `00344` has
   never fired (against a 23-hit `VUID` positive control).
5. Zero functional lines changed; the known-good control is byte-unchanged; no
   `.hlsl` touched.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates. This is a **static** audit, as v200 and v201 were. **No row above
may be cited as evidence that an acceptance gate passed.**

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
