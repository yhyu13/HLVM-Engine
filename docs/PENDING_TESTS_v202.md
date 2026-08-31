# Pending Tests v202

- commit: docs/PENDING_COMMIT_v202.md
- author: agent_5_tester (tick-548)
- mode: **file-only structural verification.** `terminal` is denied
  (`tirith:unknown`), so nothing here compiles or runs. No row may be cited as
  evidence that a gate passed.

## Rules honoured

Per the accumulated checklist: no `|` alternation (tick-526); `path` at a
directory for load-bearing negatives (v199); every zero paired with a same-shape
positive (v199); no conclusion from a hit count where reading the body is
required (v200); no enumeration resting on a convenience wrapper (v201).

## Rows

| # | Claim | Query / action | Expected | Actual | Verdict |
|---|---|---|---|---|---|
| 1 | Control's generate shader has no t4 | `gDirection` in `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl` | 0 | 0 | PASS |
| 2 | Row 1's zero is not vacuous | `register(t` same file | >0 | 4 | PASS |
| 3 | Primary's generate shader has a t4 | `register(t` in `TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl` | 5 | 5 (t0,t4,t1,t2,t3) | PASS |
| 4 | Shared layout declares SRV(4) unconditionally | read `FReSTIRPass.cpp` layout block in full | present, no branch | present, no branch | PASS |
| 5 | Control never sets DirectionTexture | `GenDesc.DirectionTexture` in `TestCornellBoxGI.cpp` | 0 | 0 | PASS |
| 6 | Row 5's zero is not vacuous | read the `GenDesc` block `:1524-1532` in full | assigns 6 fields | 6 fields, no Direction | PASS |
| 7 | Fallback keeps t4 populated | read the ternary at the binding set | substitutes Radiance | substitutes Radiance | PASS |
| 8 | Generation reads no GBuffer texture (primary) | read `main` `:62-78` in full | only gRadiance, gDirection | only gRadiance, gDirection | PASS |
| 9 | Generation reads no GBuffer texture (control) | read `main` `:76-120` in full | only gRadiance | only gRadiance (`:93`,`:114`) | PASS |
| 10 | Generation's live inputs are half-res | read creation `:1675-1680` | HalfW/HalfH both | HalfW/HalfH both | PASS |
| 11 | Generation dispatches half-res | read `Gd.OutputWidth/Height` `:911-912` | HalfResWidth/Height | HalfResWidth/Height | PASS |
| 12 | Spatial's UAV is in no SRV slot | read call site `:1083-1096` in full | SpatialRadiance only as OutRadiance | confirmed | PASS |
| 13 | bug-075's VUID never fired | `path=Binary/Debug pattern="00344"` | 0 | 0 | PASS |
| 14 | Row 13's zero is not vacuous | `path=Binary/Debug pattern="VUID"` | >0 | 23 | PASS |
| 15 | Diff is comment-only | inspect both `patch` diffs | no functional line | no functional line | PASS |
| 16 | Control byte-unchanged | `TestCornellBoxGI*` not in `files:` | absent | absent | PASS |
| 17 | No `.hlsl` modified (v182 hazard) | inspect `files:` | 0 shader files | 0 shader files | PASS |
| 18 | Namespace/structure intact after edits | `namespace ReSTIR`, `void FReSTIRPass::`, `bool FReSTIRPass::` | open+close, 4+1 defs | `:39-40`/`:658`, 4+1 | PASS |

**18/18 PASS.**

## Rows that could have falsified the cycle

Rows 2, 6, 9 and 14 are the load-bearing ones. Row 9 is the strongest: had the
control's shader actually *read* `gWorldPos`/`gNormals`/`gDepth`, the impler's
"generation needs no `GBufferScale`" comment would be wrong for that consumer and
the cycle's second finding would collapse. I read the whole body rather than
counting hits, per the reviewer's note.

Row 14 matters because a `00344` zero taken alone would be exactly the vacuous
negative tick-526 warned about; the 23-hit `VUID` positive in the same directory
proves the instrument works.

## Not tested — load-bearing

Nothing was compiled, linked, run, rendered or validated. Rows 1-18 establish
only that the source says what the markers claim it says.
