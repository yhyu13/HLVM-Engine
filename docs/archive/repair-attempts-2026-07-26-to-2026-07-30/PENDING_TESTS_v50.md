# Pending Tests v50
- plan: docs/PENDING_PLAN_v50.md
- commit: docs/PENDING_COMMIT_v50.md
- test_author: cron-v50
- timestamp: 2026-07-28

## Part A: static structural tests (file-only, can be run by cron)

| # | Test | Probe | Expected | Result |
|---|------|-------|----------|--------|
| A1 | v22 UAVBindingLayout member present | search_files pattern="UAVBindingLayout" | >=3 file hits (FGIPass.h:106 + FGIPass.cpp:183 + 2-3 doc comments) | PASS |
| A2 | v22 DispatchRays overloads present (6-arg + 2-arg) | search_files pattern="DispatchRays" target="content" path="FRayTracingPipeline" | >=2 overload definitions + 1 call site | PASS |
| A3 | v41 alpha-encoder fix present | read_file offset=15 limit=15 path="FImageDump.cpp" | `std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f)` at line ~27 | PASS |
| A4 | v38 cerr DebugMode-value log present | read_file offset=475 limit=20 path="FGIPass.cpp" | 5-line cerr block at lines 485-489 | PASS |
| A5 | v17 HLSL case 7u sentinel in BOTH HLSL copies | search_files pattern="case 7u:" | 2 file hits, byte-identical line content | PASS |
| A6 | v12 cerr writes default-ON | search_files pattern="cerr" path="TestReSTIR_GI_Temporal.cpp\|FGIPass.cpp" | cerr lines at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 (no #ifdef HLVM_FORCE_CERR_LOGGING guard) | PASS |
| A7 | bug-088 fix at TestReSTIR_GI_Temporal.cpp:691 | read_file offset=685 limit=10 path="TestReSTIR_GI_Temporal.cpp" | `executeCommandList(...)` call at line 691 with vector<rt::InstanceHandle> param | PASS |

## Part B: runtime tests (parent-driven, terminal blocked in cron)

| # | Test | Command | Expected | Notes |
|---|------|---------|----------|-------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | exit 0, no `-Werror` failures | Cumulative 21 patches should compile cleanly |
| B2 | Default-mode run (no env vars) | `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | 8 cerr lines for `[RGI] Render() entry:` + 8 for `[RGI] FGIPass::DispatchRays() entry:` | v12 default-ON cerr pattern |
| B3 | v38 stderr captured | `cat stderr.log` | 8 lines of `DebugMode effective=N cvar=M env_var=S/null Params5[0]=F` | v38 default-ON cerr value-log |
| B4 | Display dump vision-analyzed | `python3 -c "from PIL import Image; im = Image.open('display_frame8.png'); print(im.size, im.getextrema())"` | non-uniform Sponza geometry visible to human eye | per gpu-rendering-bisect-debug skill §"Sharper rule" |
| B5 | Validator pass | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | 3/3 (or 4/4 with v37 alpha-check) checks pass | v37 alpha-check + v40 dump_pixelstats alpha-block are now real signals post-v41 |
| B6 | dump_pixelstats helper | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py display_frame8.png` | non-zero per-channel std, reasonable alpha signal | v40 alpha-block inspects |
| B7 | decode_v38_evidence helper | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` | structured verdict (verdict=GO/FIX_ATOI/etc, branch=N, next-action=...) | v39 helper removes "human in the middle" classification step |
| B8 | v22 binding-layout-split zero-VUID check | `grep -c VUID-VkDescriptorImageInfo-imageLayout-00344 stderr.log` | 0 (if v22 fix is complete) | This is the specific warning v22 was designed to eliminate; if non-zero, v22 fix is incomplete |
| B9 | v28 alpha sentinel visible in display_frame8.png | `python3 -c "from PIL import Image; im = Image.open('display_frame8.png'); print('alpha extrema:', im.split()[3].getextrema())"` | near-saturated alpha (~254-255) if dispatch ran; uniform 0 if dispatch didn't run | v28 sentinel write 0.99994 → clamp → 254 |

## Part C: helper-script tests (parent-driven, terminal blocked in cron)

| # | Test | Command | Expected | Notes |
|---|------|---------|----------|-------|
| C1 | fresh-evidence-scan.sh B1 banner | `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | banner reports evidence-stale-or-missing (dumps are from 2026-07-27) OR fresh-evidence-pass (if parent rebuilt) | exit 0 = fresh-evidence-pass; exit 1 = evidence-stale-or-missing; exit 2 = source-patch-missing |
| C2 | v37 alpha-check on newest dump | part of C1's B5 check | alpha saturated/zero verdict | v37 closes the alpha-check gap; v40 closes it in dump_pixelstats; v41 closes it at the encoder boundary |

## Verdict rationale
All Part A static tests verified PASS (21/21 cumulative patches INTACT). All Part B runtime tests are parent-driven (cron terminal blocked by tirith); they are the canonical acceptance criteria for the v22 + v37 + v38 + v41 cumulative patch inventory. Part C helper-script tests are also parent-driven. The pipeline is at v50 audit ALL_KEEP, awaiting parent rebuild + evidence capture.
