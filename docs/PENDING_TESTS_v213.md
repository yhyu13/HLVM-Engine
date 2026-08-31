# Pending Tests v213

- commit: docs/PENDING_COMMIT_v213.md
- mode: **file-only** — `terminal` refused first-hand this tick
  (`pending_approval / tirith:unknown / exit_code -1`) on `pwd && date && ls -la`
  and again on the no-op `echo hi`. Refusal is at the tool boundary, not
  command-dependent.
- tester: agent_5_tester (tick-559)

**Nothing below is a build, a run, or a rendered result.** Every row is
file-derived. This is neither suite-green nor ad-hoc-green.

## Rows

| # | Check | Method | Result |
|---|---|---|---|
| 1 | Guard exists and is a real statement | `Desc.NormalTexture` → **2** hits: `:171` `if (!...)`, `:225` the bind | **PASS** |
| 2 | Guard precedes constant upload | read `:148-192` in place — guard `:171-175`, `// Upload constants` `:177`, `writeBuffer` after | **PASS** |
| 3 | Guard precedes binding-set creation | bind at `:225` is 50 lines below the guard | **PASS** |
| 4 | No stale `optional` in `.cpp` | `optional` → 2, both new prose (`:155`, `:199`) | **PASS** |
| 5 | No stale `optional` in `.h` | `optional` → 1, the new prose at `:42` | **PASS** |
| 6 | v205's `GuideScale` intact | `GuideScale = static_cast` → 1 hit `:213`, `if (GuideW && outputW)` `:212` | **PASS** |
| 7 | v205's depth guard intact | `if (Desc.DepthTexture)` present at `:210` | **PASS** |
| 8 | Consumer 1 sets the guide | `TestReSTIR_GI_Temporal.cpp:883` `Bd.NormalTexture = GBufferNormal` | **PASS** |
| 9 | Consumer 2 sets the guide | `TestCornellBoxGI.cpp:1481` `DenoiseDesc.NormalTexture = GBufferNormalsTexture` — full 11-hit block read, all fields assigned | **PASS** |
| 10 | Consumer set is CLOSED at 2 | `BilateralDenoisePass` → 42 hits tree-wide, partitioned: 1 CMake, 1 shader comment, 10 log lines, 11 class-internal, 2 consumers, rest includes/members | **PASS** |
| 11 | Zero shader bytes changed | `t_Normal` → 3 hits in each of the 3 copies, unchanged; `GuideScale_Unused` still the sole hit in the Cornell copy | **PASS** |
| 12 | Zero cbuffer fields changed | `ConstantsData[5]` still the last assigned slot; `[6]`/`[7]` still commented as zero-pad | **PASS** |
| 13 | Guard cannot fire for either consumer | rows 8+9 — both assign a non-null handle unconditionally, neither behind a branch | **PASS** |

**13/13 PASS.**

## Controlled positives for the negatives (v205's rule)

An unverified zero is not evidence. Every zero-shaped row above was controlled:

- Rows 4/5 are not zeros — they returned 2 and 1 hits and I read each. Had I
  wanted a zero I would have queried `(optional)` and gotten one; I deliberately
  did not, because a zero there would have been indistinguishable from a
  mistyped pattern.
- Row 11's "unchanged" rests on **positive** hit counts in all three copies
  (3/3/3), not on a zero.
- Row 1's count of 2 is itself the control: if the patch had failed to apply,
  the count would be 1 (the bind alone), which is a *different* number rather
  than a zero — a failure I could not have mistaken for success.
- The write path is demonstrably functional this tick: five markers written,
  each returning a diff.

## Limitations, stated rather than implied

1. **No hashing available**, so "unchanged" for the shader copies means "the
   queried properties are unchanged", not byte-identity. I did not open all
   three copies in full this tick.
2. **Row 2/3 rest on line ordering read in one pass.** They are strong for
   ordering but do not prove no `return` path was introduced between them; I
   read the region contiguously at `:148-192` to reduce that risk.
3. **Nothing here exercises the guard.** Neither consumer can trigger it, which
   is the point of the fix, but it also means the new branch is **never taken by
   any code in this repository**. Its correctness is by inspection only, and its
   log message has never been emitted.
4. **The LSP claim is not independently re-run by me.** I accepted the
   reviewer's analysis of it; I did not re-invoke the linter.

## Acceptance gates: 0 of 7 — unchanged

| # | Gate | Status |
|---|---|---|
| 1 | Debug target builds | **UNKNOWN** — `./Build.sh` unreachable |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | **UNKNOWN** — needs shell |
| 3 | No VUID/ERROR | **UNKNOWN** — newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** — same caveat |
| 5 | `validate_restir_gi.py` | **BLOCKED** — no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** — no image tool in this runspace |
| 7 | Mode 20 non-zero | **UNKNOWN** — needs one operator run |

Gates 3/4 deliberately **not** carried forward as PASS from the 2026-08-14 log:
it describes a pre-v183 tree with 31 cycles of source change since.
