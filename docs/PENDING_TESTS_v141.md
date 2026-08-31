# Pending Tests v141
- commit: docs/PENDING_COMMIT_v141.md
- status: BLOCKED
- tester: tester (single-profile self-check)
- timestamp: 2026-08-05

## Existing test surface
- Build/integration target: `TestReSTIR_GI_Temporal`.
- Regression probe: `HLVM_PT_DEBUG_MODE=20` must return non-zero `GBufferMaterial` through the GI shader SRV binding.
- End-to-end run: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`.
- Structural validator: `validate_restir_gi.py`, newest dump group only.
- Runtime log scan: no Vulkan VUID/ERROR or command-list errors.
- Visual acceptance: fresh display image must show recognizable Sponza at sane exposure.

## Execution result
The scheduled worker attempted `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`. The terminal tool returned `pending_approval`, `exit_code=-1`, `pattern_key=tirith:unknown`; therefore no build, runtime, PNG, numpy, validator, or visual result is claimed.

## Test files produced
None. The existing GPU integration and validator tests are the required coverage.
