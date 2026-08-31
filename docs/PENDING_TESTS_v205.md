# Pending Tests v205

- commit: docs/PENDING_COMMIT_v205.md
- mode: file-only structural verification (terminal denied — see row 0)
- tester: agent_5_tester (tick-551)

No test files produced; `produces_test_files: no` is accurate. This is a
2-line change inside a shared pass class with no test harness reachable from
this runspace. What follows is verification of the patch's claims, each row
executed by me this tick, not copied from another marker.

| # | Row | Method | Result |
|---|-----|--------|--------|
| 0 | Terminal availability | 2 probes: compound `git log`, bare `pwd` | **DENIED** (`pending_approval / tirith:unknown / exit_code -1`), categorical — a bare `pwd` is refused, so the block is not command-shaped |
| 1 | Scale sourced from the mandatory guide | read `:182-189` in place | PASS — `if (Desc.DepthTexture)`, `GuideW` from `Desc.DepthTexture->getDesc().width` |
| 2 | Optional guide no longer sources the extent | `Desc.NormalTexture` | PASS — **1** hit, `:199`, binding-set item only |
| 3 | Positive control for row 2 | `Desc.DepthTexture` | PASS — **3** hits (`:183`, `:185`, `:198`); row 2's low count is real, not a broken query |
| 4 | Optional guide still bound | `:199` read in place | PASS — `Texture_SRV(2, Desc.NormalTexture)` intact; the fix does not unbind it |
| 5 | Constant slot unchanged | `ConstantsData\[5\]` | PASS — 1 hit, `:189` |
| 6 | Positive control for row 5 | `ConstantsData` | PASS — 9 hits, slots 0-5 + decl + memset + writeBuffer |
| 7 | Buffer bounds | `:156` read | PASS — `float ConstantsData[64]`; index 5 far in bounds, v184 overflow class not engaged |
| 8 | Zero-divide guard survives | `:186` read | PASS — `if (GuideW && outputW)` |
| 9 | Shader-side degeneracy floor survives | primary shader `:37` | PASS — `int s = max(int(GuideScale), 1)` |
| 10 | Primary shader byte-unchanged | `GB(` per-file | PASS — **10** hits, identical to v204: helper `:35`, 4 guide loads `:82`/`:83`/`:113`/`:118`, 5 comment mentions |
| 11 | Fix not over-applied | `t_Input.Load` per-file | PASS — **2** hits, both **raw** coords; v189's fix not regressed |
| 12 | Control shader byte-unchanged | `GB(` per-file on control | PASS — **0** hits; controlled by row 10's 10 on the same query shape, so the zero is real |
| 13 | No cbuffer layout drift | slot index compared, rows 5+12 | PASS — slot 5 in both copies as before; **neither HLSL file needed editing**, v182 dual-copy hazard not engaged |
| 14 | No-op for the control, by derivation | assignment sets closed | PASS — control guides `:1187`/`:1199` from one `Desc` block; primary `:1630` (via `WpDesc` `:1620`, `W,H`) and `:1655`/`:1664` (`W,H`). Equal widths at every extent ⇒ `GuideW` identical whichever guide sources it |
| 15 | Header comment is truthful post-fix | read `FDesc` block | PASS — states the invariant, names `DepthTexture` as the source, and records that guides need **not** match `OutputWidth` |
| 16 | Both consumers set the mandatory guide | primary `:882`, control `:1480` | PASS — neither omits `DepthTexture` |

## Query-instrument note (rows 5, 10, 12)

Two distinct escaping failures hit this tick and both are recorded because each
returns a **false zero**, the polarity that fakes a failure:

1. `ConstantsData[5]` → 0 hits. `[5]` is a character class. Escaped form
   `ConstantsData\[5\]` → 1 hit. Caught at the review gate.
2. `GB\(` → **query error** (`grep: Unmatched ( or \(`), reported as
   `total_count: 0` with an `error` field. A caller skimming the count sees a
   zero. The plain substring `GB(` works. So `[` needs escaping and `(` must
   **not** be escaped — opposite conventions in one tool, one row apart.

Rule for successors: **read the `error` field before believing any zero**, and
control every zero with a same-shape positive on a file known to contain the
token (row 10 controls row 12).

## What was NOT verified

Nothing was compiled, linked, shader-compiled, run, validated or viewed. The
LSP diagnostics emitted on the header edit are include-path artifacts of parsing
a standalone header with no include dirs — they flag untouched lines (43, 44,
48-54) and the entire `nvrhi`/`FString` surface, so they are not attributable
to this change and are not evidence that it compiles either.
