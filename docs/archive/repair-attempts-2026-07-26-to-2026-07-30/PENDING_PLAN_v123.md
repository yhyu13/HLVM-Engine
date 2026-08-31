# Pending Plan v123
- task: restir-gi-fix-runtime-verification-v123 — obtain fresh executable, log, validator, structural, and visual evidence for the v114 split-layout repair
- source: current working-tree v114 implementation recorded by `docs/PENDING_COMMIT_v114.md`; no bundle, stale helper, patch application, or speculative renderer edit
- approach: Preserve renderer, shader, and test source until fresh execution identifies a concrete failure. Retry the canonical Debug build and then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` with fresh log/dump frontiers; inspect appended log bytes, isolate and validate only the newest coherent frame-8 group, calculate structural statistics, and inspect the display for recognizable non-uniform sane-exposure Sponza geometry. If authorization blocks execution, record the exact result and requeue unchanged.
- diff_estimate: +0 / -0 production lines on the verification-first path
- skip_plan_review: no
- test_strategy: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`; then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`; scan fresh log bytes for command-list/Vulkan errors, validate newest group only, compute RGB statistics and alpha sentinel, and inspect PNG directly.
- risks: Terminal authorization may return `status=pending_approval`, `exit_code=-1`, `pattern_key=tirith:unknown`; this is not runtime evidence. Historical logs/dumps are prohibited. No goal-done marker without all acceptance gates.

## Failure routing
- Build/process failure: preserve exact evidence and make no speculative renderer edit.
- Validator/structural/visual failure: bisect one variable per run with existing debug modes and inspect fresh images.
- All gates pass: downstream roles may write completion evidence.
