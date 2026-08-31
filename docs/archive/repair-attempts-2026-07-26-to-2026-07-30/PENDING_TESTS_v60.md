# Pending Tests v60
- task: v60 structural standby (markers-only) — Part A static verification + Part B parent-driven runtime verification
- commit: docs/PENDING_COMMIT_v60.md

## Part A — File-only static verification (this cron tick, COMPLETED)

12 fresh `search_files` + `read_file` probes verified the cumulative 21-patch inventory intact (NOT by-reference to v59 audit; explicit discipline improvement maintained since v53):

| # | Site | Patch | Result |
|---|------|-------|--------|
| A1 | Public/Renderer/GI/FGIPass.h:106 | v22 split UAVBindingLayout member | PASS |
| A2 | Private/Renderer/GI/FGIPass.cpp:183 | v22 split UAVBindingLayout init | PASS |
| A3 | Private/Renderer/GI/FGIPass.cpp:311 | v22 split UAVBindingLayout createBindingLayout | PASS |
| A4 | Private/Renderer/GI/FGIPass.cpp:612 | v22 split UAVBindingSet use-site | PASS |
| A5 | Private/Renderer/GI/FGIPass.cpp:487 | v38 cerr DebugMode effective= | PASS |
| A6 | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:345 | v22 2-overload DispatchRays signature | PASS |
| A7 | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:381 | v22 2-overload DispatchRays call | PASS |
| A8 | Private/Image/FImageDump.cpp:27 | v41 std::clamp alpha-encoder | PASS |
| A9 | Private/Renderer/Shader/GI/GIPathTracing.hlsl:604 (Private master) | v17 case 7u | PASS |
| A10 | Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:604 (data-dir) | v17 case 7u | PASS |
| A11 | Private/Renderer/Shader/GI/GIPathTracing.hlsl:694 (Private master) | v28 alpha sentinel | PASS |
| A12 | Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:694 (data-dir) | v28 alpha sentinel | PASS |

12/12 Part A static probes PASS.

## Part B — Parent-driven runtime verification (terminal-blocked in cron, PENDING)

8 staged tests — ALL require parent-driven terminal access (tirith blocks cron terminal probes):

| # | Test | Command | Expected result | Parent-action? |
|---|------|---------|-----------------|----------------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | exit 0, no warnings on v22 binding-layout changes | Yes |
| B2 | Default-mode run | `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | stderr.log shows 8 [RGI] Render() entry + 8 [RGI] FGIPass::DispatchRays() entry lines; 8 [RGI] FGIPass::WriteConstants: DebugMode effective= lines from v38 | Yes |
| B3 | Alpha classification | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` on newest dumps/2026-07-28-* group | alpha line A: stats; v40-alpha verdict line per frame | Yes |
| B4 | Validator | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | 4/4 PASS (includes v37 check_alpha_sentinel) | Yes |
| B5 | Vision | open `display_frame8.png` with vision analyzer | recognizes non-uniform Sponza geometry with sane exposure | Yes |
| B6 | VUID-zero check | `grep VUID-VkDescriptorImageInfo-imageLayout-00344 stderr.log` | 0 (v22 binding-layout-split should eliminate this warning) | Yes |
| B7 | v38 closure-decoder | `python3 decode_v38_evidence.py --cerr-file stderr.log` | structured verdict (GO / FIX_ATOI / FIX_DOCS / FIX_CVAR / NO_CERR / MIXED / UNRECOGNIZED) | Yes |
| B8 | Mode-6 evidence | `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=6 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` then inspect `gi_raw` | per-pixel gradient `(R=0..3, G=0, B=0..2)` if v13 sentinel compiles + dispatch runs | Yes |

8/8 Part B runtime tests PENDING — terminal blocked.

## Part C — Cleanup (parent-driven)
- C1: parent should `rm -f /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp.placeholder` (0-byte placeholder from v24 dump_pixelstats.py cycle, no longer needed).

## Acceptance criteria (parent-driven; terminal blocked in cron): (a)-(f) all 6/6 UNVERIFIED

(a) Debug build cleanliness — UNVERIFIED.
(b) Fresh HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8 run — UNVERIFIED (no 2026-07-28 dump group yet; newest is 20260727_000706-000708, ~24h+ stale).
(c) No "Cannot open a command list that is already open" — UNVERIFIED.
(d) No Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 — UNVERIFIED.
(e) validate_restir_gi.py passes newest dump group — UNVERIFIED.
(f) Visual recognition of sane-exposure non-uniform Sponza geometry — UNVERIFIED.
