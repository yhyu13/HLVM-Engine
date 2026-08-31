# Pending Tests v4

- commit: docs/PENDING_COMMIT_v4.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py (unchanged from v1; already exists with 3 structural checks)
- test_strategy: parent-driven log capture + validator run. v4 adds no new test files. The acceptance check is the v3+v4a log evidence correlation (see PENDING_PLAN_v4.md acceptance criteria), NOT a new automated test.
- rationale: per `software-development-practices §Test-Driven Development`, every fix should have a failing test first. v4 is a diagnostic-only cycle (v4a) with a conditional fix (v4b, gated). The "test" for v4 is the structural validator `validate_restir_gi.py` (already in place from v1, 3 checks: non-black mean > 5, spatial std > 30, cell-variance std > 8). The validator is the only automated test that exists for this pipeline.
- red_phase: N/A — v4 is observability, not a new behavior change.
- green_phase: N/A — same reason.
- verification commands (parent-driven, terminal blocked in cron):
  ```bash
  cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
  ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
  cd Engine/Source/Runtime/Binary/Debug
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 180 ./TestReSTIR_GI_Temporal
  # Capture log: TestReSTIR_GI_Temporal.log
  # Look for these NEW lines per dump frame (last frame only):
  #   DumpRGBA32FTexture: dumping gi_raw Texture=0x... Frame=N
  #   FGIPass::DispatchRays ENTER: OutputTex=0x... OutputW=800 OutputH=600 Frame=N CmdList=0x...
  # The two OutputTex/Texture handles MUST match.
  cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
  python3 validate_restir_gi.py
  # Expected after v3+v4a (no fix yet): 0/3 PASSED (gi_raw still 0,0,0)
  ```

## What this cycle did NOT do

- Did not write a new test file.
- Did not modify `validate_restir_gi.py`.
- Did not modify any shader (`GIPathTracing.hlsl`, `GIAccumulate_cs.hlsl`, etc.).
- Did not modify any binding layout.

## What this cycle DID do

- Added 1 HLVM_LOG info-level call + 7-line comment in `TestReSTIR_GI_Temporal.cpp::DumpRGBA32FTexture` (file-only patch via the patch tool — landed and verified by re-reading the file at line 1685+).
- Documented the parent-driven verification procedure in PENDING_COMMIT_v4.md.
- Did NOT apply v4b (the HLVM-bypass removal) — that is a separate v5 cycle gated on v4a's log evidence.