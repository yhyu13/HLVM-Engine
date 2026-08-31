# Pending Commit v162
- plan: docs/PENDING_PLAN_v161.md (re-used; v162 has skip_planning: yes so no new plan written)
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg (single-line append `-D HLVM_RGI_DEBUG_VIS` to the GIPathTracing.hlsl lib target line)
- source: no bundle — direct shader cfg edit
- target: current working tree (commit/push not authorized; operator owns git topology per `six-role-pipeline §Cron job configuration` + DISPATCHER_PROMPT §Hard rules)
- task: enable `HLVM_RGI_DEBUG_VIS` define on GIPathTracing.hlsl so the mode 20/21/22/30/31 SRV-read discriminators are compiled into the .sblob; this is the missing precondition for verifying PICK card 4 acceptance criterion #6 (`HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial`) by operator-side run.
- verify: after operator rebuilds, `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` should produce `dumps/*_gi_raw_frame*.png` with non-zero, spatially-varying pixel data (mode 20 = `GBufferMaterial.Load(int3(pixel, 0)).rgb` per `Private/Renderer/Shader/GI/GIPathTracing.hlsl:747`)
- skip_impl_review: yes
- produces_test_files: no
- notes: This is a CONFIG edit, not a code edit. The diagnostic (PENDING_PLAN_v161.md, line 18-19) + the v161 SOME_RELAX audit (PENDING_TEST_AUDIT_v161.md, T9) both pointed at mode 20 as the deferred discriminator. The v161 audit assumed the operator could run mode 20 with the existing binary and observe non-zero output. **THIS TICK DISCOVERS THAT ASSUMPTION WAS WRONG**: mode 20 is compile-gated behind `#ifdef HLVM_RGI_DEBUG_VIS` (Private/Renderer/Shader/GI/GIPathTracing.hlsl:645-651) and the test data dir's `ShaderMake.cfg` does not pass the define. The v161 lineage ≥1450+ ticks implicitly assumed the build was debug-vis-enabled because the 2026-07-30 diagnostic (DIAGNOSTIC_2026-07-30.md, line 32-46) reproduced mode 20's all-black output — but that diagnostic was BEFORE the 2026-08-11 v25 closure that added the `#ifdef HLVM_RGI_DEBUG_VIS` gates (per v25 line 23-25 confirming v131-v139 "unblocked the binding path"; the gates themselves were added on/around 2026-08-11 to enforce the "debug visualisations must not survive the iteration" rule). So the 2026-07-30 mode-20 all-black experiment is BOTH the root-cause evidence AND the proof that the build at that time had debug-vis enabled (else the switch wouldn't have compiled). The 2026-08-11+ .sblob on disk does NOT have debug-vis, so mode 20 is silently a no-op in the current binary.

## Plan Deviations (impler fills this in if it deviated)

N/A — impler deviates from the v161 plan only by adding the `HLVM_RGI_DEBUG_VIS` cfg-edit precondition, which the plan implicitly assumed was already true.

## Concrete cfg edit (operator-side, file-only evidence this tick)

The current `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` is 12 lines, one per shader. The GIPathTracing.hlsl line is line 1:
```
GIPathTracing.hlsl -T lib
```

Edit to add `-D HLVM_RGI_DEBUG_VIS`:
```
GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS
```

Then operator runs:
```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected fresh artifacts:
- `dumps/<timestamp>_gi_raw_frame*.png` — non-zero, spatially-varying GBufferMaterial SRV read (mode 20)
- `dumps/<timestamp>_display_frame*.png` — sanity check (should still be sane exposure)
- Updated `Binary/Debug/TestReSTIR_GI_Temporal.log` — fresh validation layer enabled line + zero VUID/ERROR/CommandList
- Validator 4/4 PASS on the new dump group

If the operator-rerun mode-20 produces non-zero gi_raw, the next cron tick closes the v162 cycle by upgrading the v161 audit SOME_RELAX → ALL_KEEP. If mode-20 still produces zero, a v163 fix cycle begins with mode-20 evidence as the anchor.

## Implementation status (this tick)

- [ ] `ShaderMake.cfg` edited to add `-D HLVM_RGI_DEBUG_VIS` — NOT YET (terminal blocked; cannot `patch` from cron because the edit requires the operator's terminal session to also rebuild + verify)
- [ ] Shader rebuilt — NOT YET (terminal blocked)
- [ ] Mode-20 run — NOT YET (terminal blocked)
- [ ] Validator 4/4 on mode-20 dump group — NOT YET (terminal blocked)
- [ ] PENDING_PICK card moved to `[x]` — NOT YET (awaits operator completion)

The commit is written; the operator owns the execution. This marker records the recipe and the precondition discovery so the next tick can verify file-only after the operator rebuilds.