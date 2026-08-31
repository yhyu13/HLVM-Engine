# Pending Tests v13

- task: parent-driven test plan for v13 patch (UAV-write sentinel debug mode 6u)
- plan: docs/PENDING_PLAN_v13.md
- commit: docs/PENDING_COMMIT_v13.md
- impl_review: docs/PENDING_IMPL_REVIEW_v13.md
- tests_author: tester (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Test surface

The v13 patch is observable only when:
1. The binary is rebuilt with the v13 patch in source.
2. `HLVM_PT_DEBUG_MODE=6` is set in the env.

The test surface is the existing `TestReSTIR_GI_Temporal` test harness with the new env var. No new test files are added.

## Staged tests (parent-driven; terminal blocked in cron)

### Test 1: Build cleanliness

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

Expected: clean build, no shader compile errors, no link errors. The slangc compilation of GIPathTracing.hlsl will produce a slightly larger .sblob due to the new case 6u (negligible size delta — a few bytes).

Pass criterion: build exits 0; no warnings related to the new case.

### Test 2: Default-mode run (v12 evidence path)

```bash
cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log
cat stderr.log
cat TestReSTIR_GI_Temporal.log | grep -E "(Render|GIPass|DispatchRays|gi_raw)"
```

Expected stderr.log: 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) — confirms v12 patch is live.

Expected log content (one of three):
- (H-A confirmed) v3 spdlog markers fire per frame: `Pre-GIPass`, `Post-GIPass`, `FGIPass::DispatchRays ENTER`, `FGIPass: per-frame binding set created OK`, `FGIPass::DispatchRays EXIT`
- (H-B confirmed) v3 spdlog markers DO NOT fire even with cerr visible — spdlog config issue
- (cerr does NOT fire) v12c territory: stderr not reaching stream

Pass criterion: at least one of the above is observed; report back to cron with the shape.

### Test 3: v13 mode-6 run (the new test)

```bash
cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 ./TestReSTIR_GI_Temporal 2>stderr.log
python3 -c "
import sys
sys.path.insert(0, 'Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data')
import numpy as np
from PIL import Image
img = np.asarray(Image.open('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/20260727_*_gi_raw_frame8.png').convert('RGB'))
print('gi_raw shape:', img.shape)
print('R range:', img[..., 0].min(), img[..., 0].max(), 'mean:', img[..., 0].mean(), 'std:', img[..., 0].std())
print('G range:', img[..., 1].min(), img[..., 1].max(), 'mean:', img[..., 1].mean(), 'std:', img[..., 1].std())
print('B range:', img[..., 2].min(), img[..., 2].max(), 'mean:', img[..., 2].mean(), 'std:', img[..., 2].std())
print('Unique R values:', len(np.unique(img[..., 0])))
"
```

Expected (per v13 plan decision matrix):
- **Per-pixel gradient visible** (R=0..3, G=0, B=0..2): dispatch body runs, UAV write lands. Bug is downstream of the Output[pixel] write.
- **All zeros**: dispatch body not reached. Most likely source/binary mismatch (H-A) or spdlog config (H-B).
- **Garbage**: UAV write is being overwritten by something downstream (bilateral denoise, ReSTIR, accumulate). Investigate.
- **Single uniform value**: something is reading OutputTexture and writing a uniform value. Investigate.

Pass criterion: report the gi_raw pixel-stats shape back to cron.

### Test 4: Vision analysis (per the gpu-rendering-bisect-debug skill)

Open `display_frame8.png` with vision_analyze (or equivalent). Expected: recognizable non-uniform Sponza geometry with sane exposure.

Per the skill: "After a fix, the image should look BETTER, not just non-broken." A passing validator on a still-broken-looking image is incomplete. Open the dump and ask: does this look like what the user actually wanted to see?

Pass criterion: vision-analyze confirms the image looks like the Sponza scene (not noise, not uniform color, not a sentinel pattern).

### Test 5: Validator

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected: 3/3 status (assuming the upstream bug is fixed). If the v13 patch surfaces a downstream bug, validator may still fail — that's the value of v13's probe.

Pass criterion: report 3/3 or the specific failure shape back to cron.

## Honesty caveats

- All 5 tests are parent-driven. The cron's terminal is blocked (tirith denies every terminal command). Tests cannot run from cron.
- The v13 patch is a probe, not a fix. The pass criteria are about producing diagnostic information, not about the renderer being correct.
- The validator may still fail after v13 even if v13's mode=6 shows the per-pixel gradient. This is expected: v13 tests the dispatch body, not the downstream chain.
- The cron's structural block (no terminal) means v13's effect is observable only after parent action. The cron cannot claim the v13 patch "fixes" the renderer.
