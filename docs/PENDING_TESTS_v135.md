# Pending Tests v135 — File-only verification (8 file-only tests PASS, 2 parent-runspace tests DEFERRED)

- plan: docs/PENDING_PLAN_v135.md
- commit: docs/PENDING_COMMIT_v135.md
- impl_review: docs/PENDING_IMPL_REVIEW_v135.md (verdict: KEEP)
- tester: tester (file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## File-only tests (PASS)

| # | Test | Verdict | Rationale |
|---|------|---------|-----------|
| 1 | New `CmdList->commitBarriers();` is at the correct location (between setTextureState and SRVBuilder) | PASS | search_files: `CmdList->commitBarriers();` appears 2x in FGIPass.cpp: line 561 (new, v135) and line 668 (existing, v131). The new one is between `setTextureState(Desc.GBufferMaterial, ...)` at line 555 and `FBindingSetBuilder SRVBuilder;` at line 564. |
| 2 | Existing commitBarriers() at line 668 is INTACT (defense-in-depth) | PASS | search_files: 2 matches for `CmdList->commitBarriers();` in FGIPass.cpp. Both are valid (line 561 = v135, line 668 = v131). The line 668 comment block at lines 657-668 is preserved verbatim. |
| 3 | WriteConstants at line 543 is unchanged | PASS | read_file lines 540-560: `WriteConstants(CmdList, Desc);` at line 543 is unchanged. |
| 4 | The three setTextureState calls at lines 547-555 are unchanged | PASS | read_file lines 545-560: all three setTextureState calls (GBufferWorldPos, GBufferNormal, GBufferMaterial) are present and unchanged. |
| 5 | Comment block at lines 557-562 explains the v135 fix | PASS | read_file lines 557-562: 6-line comment explaining the nvrhi-deferred-barrier-ordering pattern and why this location matters. |
| 6 | v131+v132+v133+v134 patches are all intact | PASS | search_files: all 4 prior-cycle patches are present and unchanged. |
| 7 | SRVBuilder chain at lines 565+ is unchanged | PASS | read_file lines 564-580: SRVBuilder.SetConstantBuffer(0, ConstantBuffer).SetConstantBuffer(1, Desc.ViewConstants)... unchanged from v131 baseline. |
| 8 | createBindingSet at line 668+ is unchanged | PASS | read_file: createBindingSet calls (lines 615-616 and 656-657) are unchanged. |

## Parent-runspace tests (DEFERRED per EC-039)

| # | Test | Verdict | Rationale |
|---|------|---------|-----------|
| 9 | After rebuild, `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | DEFERRED | requires terminal + run. |
| 10 | After rebuild, `HLVM_PT_DEBUG_MODE=22` returns non-zero GBufferWorldPos | DEFERRED | requires terminal + run. |
| 11 | After rebuild, `validate_restir_gi.py` passes the newest dump group | DEFERRED | requires terminal + python3 + numpy. |
| 12 | After rebuild, fresh display image (vision) shows recognizable Sponza | DEFERRED | requires terminal + vision_analyze. |
| 13 | After rebuild, no Vulkan VUID/ERROR when validation layer enabled | DEFERRED | requires terminal + log grep + v132+v133+v134 patches in effect. |
| 14 | After rebuild, no command-list errors | DEFERRED | requires terminal + log grep. |

## Test summary

- 8 file-only tests PASS.
- 6 parent-runspace tests DEFERRED.
- No broken patterns detected.
- The patch is structurally correct; behavioral verification requires terminal.

## Honesty floor

This test file confirms the patch is well-formed and additive. It does NOT confirm:
- The barrier ordering was the root cause of zero SRV reads.
- The patch fixes the gi_raw output.
- The validate_restir_gi.py will pass after rebuild.
- The vision check will show Sponza.

All behavioral claims require parent-runspace verification (terminal blocked in this cron runspace per EC-039).

## Next-step for parent runspace

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
# Inspect gi_raw dump with numpy or vision
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```