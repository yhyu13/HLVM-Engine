# Pending Tests v185

- commit: docs/PENDING_COMMIT_v185.md
- tester: agent_5_tester (tick-532)
- timestamp: 2026-08-30
- mode: **file-only static verification.** `terminal` is denied by tirith, so
  no build, no run, no validator, no image. Every row below is a source-level
  check that was actually executed this tick; none is asserted from memory.

## Method constraints (tooling, honoured in every row)

`search_files` on this runspace is unsound for: `|` alternation (tick-526),
`\d` character classes and `file_glob` (tick-531), and — **newly confirmed
this tick** — patterns containing `[0]`, which are parsed as a regex
character class and return a vacuous 0 hits (`pattern="OutputSize[0]"` → 0
while `pattern="OutputSize"` → 46 on the same file). All rows below use
literal single-term substrings with `path` at a file or directory, and
anything load-bearing was confirmed with `read_file`.

## Test table

| # | Check | Method | Result |
|---|---|---|---|
| 1 | Migration complete: no ReSTIR constant block still uses the full-res FB | `pattern="float(FB.width)"` on the test file | **0 hits** — PASS |
| 2 | Both edits are present and attributed | `pattern="v185"` | 2 hits, `:875` (generation) + `:968` (temporal) — PASS |
| 3 | Generation constants now half-res | `read_file :882-885` | `HalfResWidth/Height`, incl. both Rcp — PASS |
| 4 | Temporal constants now half-res | `read_file :983-986` | `HalfResWidth/Height`, incl. both Rcp — PASS |
| 5 | Dispatch↔constant agreement, all three passes | `pattern="HalfResHeight"` → 14 hits | gen `:872`↔`:883/:885`; temporal `:959`↔`:984/:986`; spatial `:1040`↔`:1044/:1046` — PASS |
| 6 | **Negative control A** — spatial not over-applied | `read_file :1042-1046` | still original `// Phase D` block, not rewritten — PASS |
| 7 | **Negative control B** — Cornell untouched | `pattern="HalfResWidth"` on `TestCornellBoxGI.cpp` | **0 hits** (full-res test, correctly unmodified) — PASS |
| 8 | **Regression guard** — `GBufferScale` ratio not collapsed | `pattern="GBufferScale"` → 10 hits | `:1005` and `:1051` still `FB.width / max(HalfResWidth,1u)` — PASS |
| 9 | No shader was edited | `pattern="v185"` over `TestReSTIR_GI_Temporal_Data` | 0 hits — PASS |
| 10 | Cornell shader sibling unaffected by this class | `pattern="GBufferScale"` on `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` | 0 hits — that copy never received v183 either, so v185 cannot desync it — PASS |

10/10 PASS.

## Why rows 6, 7, 8 and 10 are the load-bearing ones

Rows 1-5 only show the intended edit landed. The cycle's real risk was
**over-application**, and three rows discriminate against it:

- Row 8 is the sharpest. A global `FB.width → HalfResWidth` substitution
  would have passed rows 1-5 while collapsing the `GBufferScale` ratio to 1,
  silently re-breaking v183 — the precise defect v184 had just repaired. It
  did not happen.
- Row 6 catches the symmetric-fix error the plan-criticer warned about.
- Row 7 catches blast-radius leakage into a full-res sibling test.
- Row 10 confirms the Cornell shader copy never took v183's `GB()`/
  `GBufferScale`, so the two trees are independent and this fix cannot
  desync them.

## What this table does NOT establish

It does not show the file compiles, that the shader receives the new values,
that `M mean` rises, or that the display image or validator improve rather
than regress. Those need the operator run in `PENDING_COMMIT_v185.verify`.
This is **static source verification only** — not suite-green, not
runtime-verified.
