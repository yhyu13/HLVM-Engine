# Pending Tests v92
- commit: docs/PENDING_COMMIT_v92.md
- tester: tester (single-profile, file-only runspace)
- timestamp: 2026-07-28T23:25Z

## Part A — file-only structural spot-checks (1/1 PASS)
- [x] A1: v91 marker group complete and intact (verified via read_file)

## Part B — execution-side tests (8/8 UNVERIFIED — terminal blocked by tirith)
- [ ] B1: `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` — UNVERIFIED
- [ ] B2: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` exits 0 — UNVERIFIED
- [ ] B3: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` runs cleanly — UNVERIFIED
- [ ] B4: No `Cannot open a command list that is already open` in fresh log — UNVERIFIED
- [ ] B5: No Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 — UNVERIFIED
- [ ] B6: `python3 validate_restir_gi.py` passes newest dump stamp group — UNVERIFIED
- [ ] B7: Newest display dump visually contains recognizable non-uniform Sponza geometry — UNVERIFIED
- [ ] B8: `FGIPass::DispatchRays` ENTER/EXIT log both present in fresh log — UNVERIFIED

## Notes
Part B is structural-blocked by tirith in this cron's runspace. Per cron prompt's "do not silently stop" + HARD INVARIANT #6, Part A is honored. Per gpu-rendering-bisect-debug skill's "don't fabricate" rule, no Part B test is fabricated.