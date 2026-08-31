# Pending Plan v161
- task: TestReSTIR_GI_Temporal mode-20 discriminator (operator runspace)
- source: no bundle — verification-only run against the existing GIPathTracing mode-20 probe and GBuffer SRV binding fix
- approach: Run the exact acceptance sequence on the current Debug target: build/test, execute the normal 8-frame dump run, execute a fresh `HLVM_PT_DEBUG_MODE=20` 8-frame dump run, run the canonical validator against the newest applicable dump group, and inspect the fresh display and mode-20 PNGs. Mode 20 must produce a non-zero, spatially varying GBufferMaterial image; do not infer this criterion from descriptor inventories or a mode-0 render. No production edit is authorized unless fresh mode-20 evidence fails, in which case the failure must be routed into a new evidence-driven fix cycle.
- diff_estimate: +0 / -0 production lines for the verification experiment; any resulting fix requires a subsequent reviewed plan
- skip_plan_review: no
- test_strategy: Build with `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`; run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal`; run the same target with `HLVM_PT_DEBUG_MODE=20`; scan the fresh log for Vulkan VUID/ERROR and command-list errors; run `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the newest dump group only; compute numpy per-channel min/max/mean/std and non-zero ratio for the newest mode-20 `gi_raw_frame8.png`; vision-check the newest normal display PNG for recognizable Sponza and sane exposure. PASS requires every user acceptance criterion, including direct non-zero mode-20 evidence.
- risks: This cron runspace's terminal request was denied again with `status: pending_approval`, `pattern_key: tirith:unknown`, before it could acquire `.pipeline.lock`, build, run, validate, or inspect pixels. Existing `TestReSTIR_GI_Temporal.log` is a clean mode-0 run at 2026-08-09 20:45:11 and cannot satisfy the direct mode-20 criterion. PNG vision tooling is also unavailable in this runspace. Do not substitute prior/inferred evidence, do not add the speculative one-line transition without a failing mode-20 run, and do not claim completion until an execution-enabled tick produces all direct evidence.

## Acceptance commands

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

## Required evidence record

The tester must record exact fresh filenames and command exit codes, log match counts for `VUID|ERROR|CommandList`, validator output, and numpy mode-20 RGB statistics. The testing-verifier must reject inferred substitutes. The display-image criterion requires actual visual inspection, not variance alone.
