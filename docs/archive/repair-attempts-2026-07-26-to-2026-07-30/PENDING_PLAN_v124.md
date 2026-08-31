# Pending Plan v124
- task: restir-gi-fix-runtime-verification-v124 — obtain fresh build, GPU-run, log, newest-dump, validator, structural, and visual evidence for the existing v114 split-layout repair
- source: current working tree and `docs/PENDING_COMMIT_v114.md`; verification-first, with no bundle or speculative renderer edit
- approach: Preserve all renderer, shader, test, and unrelated working-tree changes until a fresh run identifies a concrete failure. First run the read-only fresh-evidence scan, then the canonical Debug target build and a fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` test; establish pre-run log/dump frontiers, inspect only newly appended log bytes and the newest coherent frame-8 dump group, run the validator against that newest group, compute structural image statistics, and directly inspect the display for recognizable non-uniform sane-exposure Sponza geometry. If execution authorization blocks launch, retain the exact tool result without claiming runtime acceptance or editing source.
- diff_estimate: +0 / -0 production lines on the verification-first path
- skip_plan_review: no
- test_strategy: Run `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh`; `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`; then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`. Require a successful build/run, no fresh command-list-already-open diagnostic, no fresh Vulkan `ERROR`/`VUID`, newest-group-only validator PASS, non-uniform structural statistics with valid alpha sentinel, and direct image inspection showing recognizable Sponza at sane exposure.
- risks: Terminal launch may again be rejected as `status=pending_approval`, `exit_code=-1`, `pattern_key=tirith:unknown`; that is external blockage, not test evidence. Historical logs/dumps, validator-only PASS, or static source inspection cannot satisfy acceptance. A fresh runtime failure may expose a different all-zero-GI defect than the earlier magenta/sentinel bug, so any follow-up must use one-variable debug-mode bisection rather than broad edits.

## Failure routing
- Authorization/build/process failure: preserve exact evidence and make no speculative renderer edit.
- Fresh command-list or Vulkan failure: localize the exact first fresh diagnostic before changing code.
- Fresh zero/uniform/visually wrong output: bisect one stage per run using existing debug modes and sentinel-boundary checks; inspect each fresh image.
- All gates pass: downstream roles may record completion only with paths/timestamps and concrete validator, log, structural, and visual evidence.
