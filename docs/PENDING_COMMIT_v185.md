# Pending Commit v185

- plan: docs/PENDING_PLAN_v185.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: working tree only (NO commit, NO push — per job instruction)
- task: fill ReSTIR generation + temporal `OutputSize`/`RcpOutputSize` from the
  half-res dispatch grid instead of the full-res framebuffer
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
  then `cd Engine/Source/Runtime/Binary/Debug &&
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- skip_impl_review: no
- produces_test_files: no
- impler: agent_3_impler (tick-532)

## Sites changed (2 blocks, 8 assignment lines, 1 file)

**1. Generation — `:874-885`** (was `:874-878`)

```
-            C.OutputSize[0]      = float(FB.width);
+            C.OutputSize[0]      = float(HalfResWidth);
```
…and the three siblings. Plus an 8-line comment recording that this one is
inert today and why it is still worth correcting.

**2. Temporal — `:968-986`** (was `:961-964`)

```
-            TC.OutputSize[0]    = float(FB.width);
+            TC.OutputSize[0]    = float(HalfResWidth);
```
…and the three siblings. Plus a 15-line comment naming both shader
consequences with their exact shader line numbers.

Net: +23 / -8 lines, of which 8 are the functional change and 15 are
explanatory comment. Under the 50-line surgical budget.

## What was deliberately NOT changed

- **Spatial, `:1042-1046`.** Already `HalfResWidth`. Verified post-patch by
  direct `read_file` — untouched, and now byte-identical in form to the two
  blocks I fixed. Touching it would have broken a working call site; this was
  the plan-criticer's one explicit instruction.
- **`TestCornellBoxGI.cpp:1556-1559`.** Dispatches full-res, so
  `CurrentFBInfo.width` is correct there. 0 edits.
- **Every shader.** No `.hlsl` was touched. All shader consumers already
  assume `OutputSize` describes the dispatch grid — that assumption is what
  makes the old C++ values wrong. Consequence: this patch cannot be defeated
  by the v182 "patched the copy nothing compiles" trap, because no shader
  copy is involved.
- **`FReSTIRPass.cpp`.** The marshaller already writes these four floats at
  offsets 32-35 (`:432-435`) / 0-3 (`:355-358`); only the values were wrong.

## Post-patch self-verification

- `search_files path=<file> pattern="OutputSize"` → both blocks now read
  `HalfResWidth`/`HalfResHeight`; `read_file :983-986` and `:1043-1046`
  confirm generation, temporal and spatial are now three-way consistent.
- 0 remaining `FB.width` in any ReSTIR constants block.
- `FB.width` survives where it is correct (`:754-764` resize detect +
  `UpdateViewConstants`, and the `GBufferScale` numerator at `:1005`/`:1051`
  which is *meant* to be full-res over half-res).

Note on tooling: `search_files` with `[0]` in the pattern returns 0 hits
(bracket chars are treated as a regex class). Consistent with tick-526/531
findings. All queries above use literal substrings, single term, `path` at a
file or directory.

## Plan Deviations

None. Implemented exactly as planned and as constrained by the plan review.

## Honest limits

Not compiled, not run, not validated, no image seen — `terminal` is denied by
tirith (probed twice this tick, `pending_approval / tirith:unknown /
exit_code -1`). This is a **production-path** change to a test that cannot be
executed from here. The reasoning is arithmetic on values read from source
and is checkable by a reader, but it is not evidence of runtime behaviour.
