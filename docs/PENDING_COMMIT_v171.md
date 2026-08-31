# Pending Commit v171 (DRAFT — operator-side commit; cron writes this proposal only, does not apply)

- plan: docs/PENDING_PLAN_v171.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: file-only diagnostic this tick; supersedes v170 which lacked a commit-recipe
- target: local working tree (no push per job hard rules)
- task: Disable the constant mid-gray ambient baseline so per-pixel GI variance (primaryDirect + indirect/spp) can drive the display std above the validator threshold
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal && python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- skip_impl_review: yes (produces_test_files=no; patch touches TestReSTIR_GI_Temporal.cpp only)
- produces_test_files: no
- notes: Hard rule per v170 plan § "Cross-skill reconciliation": this is a 1-line surgical patch proposal; operator decides whether to apply. Without operator action the patch does not land.

## Proposed patch

```cpp
// In TestReSTIR_GI_Temporal.cpp, inside the per-frame Desc init block
// (search for "DescGI." or "FGIPassDesc GI" near the frame loop),
// add or modify these lines:

DescGI.AmbientScale = 0.0f;   // disable fake ambient baseline; let per-pixel lighting drive variance
```

**Rationale**: `FGIPass.cpp:493-495` honors `Desc.AmbientScale >= 0` as an override (the predicate is `>=`, not `>`, so `0.0f` disables ambient cleanly). The shader at GIPathTracing.hlsl:548 computes `primaryAmbient = diffuse * g_GI.AmbientColor.rgb * ambientScale`; with `ambientScale = 0`, the constant mid-gray contribution vanishes. Per-pixel std then comes from `primaryDirect` (NdotL varies across Sponza surfaces) and `indirect/spp` (8 SPP diffusion noise). Predicted post-fix std ≈ 0.15-0.20, well above the validator threshold of ~0.08.

## Optional companion patch (apply only if Patch 1 alone yields black image)

If the post-fix image is too dark or zero-mean, also nudge `DescGI.AmbientColor[3]` to 1.0 so the alpha channel contributes a baseline (but the RGB stays zero — `primaryAmbient = diffuse * 0 * 0 = 0`). For pure Sponza with one Directional light, this should not be necessary; only try if `mode=3` from Run 1 shows primaryDirect near-zero.

```cpp
// Compensating baseline — only if post-fix image is too dark
DescGI.AmbientScale = 0.05f;     // 1/6 of original; small but nonzero
```

## Patch verification recipe (operator-side)

1. Apply the one-line patch above.
2. `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` (~3 min incremental).
3. `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` (the test runs in headless mode by default; logs to `Binary/Debug/TestReSTIR_GI_Temporal.log`).
4. Verify new log line near `display floats: ...`:
   - `mean ≈ 0.30` (was 0.458)
   - `std ≈ 0.18` (was 0.046)
   - Both changes meet Acceptance criteria.
5. `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the new dump group; expect 6/6 PASS (was 3/6 FAIL predicted).
6. Vision-check `display_frame8.png`: expect Sponza walls visible (was bright monochrome).

## Plan deviations

None. The patch exactly matches v171 plan § "The fix" lines 1-4.

## Self-review checklist (operator-side)

- [ ] Validation: `validate_restir_gi.py` exits 0 with 6/6 PASS
- [ ] Error handling: no Vulkan validation layer errors in new log (grep VUID- → 0)
- [ ] Tests: post-fix log shows recognizable Sponza (vision gate)
