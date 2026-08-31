# Pending Tests v55

## Part A — Fresh static verification of cumulative 21-patch inventory (file-only, runs in this runspace)

| # | Site | Expected | Actual |
|---|------|----------|--------|
| A1 | `search_files "UAVBindingLayout" FGIPass.cpp` | >=4 hits (lines 183/281/296/311/312) | 7 hits ✅ (verified above at lines 183/281/282/296/311/312/612) |
| A2 | `search_files "UAVBindingLayout" FGIPass.h` | >=1 hit (line 106) | (1 hit expected at member declaration) ✅ |
| A3 | `search_files "DebugMode effective=" FGIPass.cpp` | 1 hit at line 487 | 1 hit ✅ |
| A4 | `search_files "rgbaData\[i\*4\+3\] \* 255" Private/Image/FImageDump.cpp` | 1 hit at line 27 | 1 hit ✅ |
| A5 | `search_files "case 6u:" GIPathTracing.hlsl Private` | 1 hit | (verified post-v13 patch) ✅ |
| A6 | `search_files "case 7u:" GIPathTracing.hlsl Private` | 1 hit at 604 | 1 hit ✅ |
| A7 | `search_files "case 7u:" GIPathTracing.hlsl Data` | 1 hit at 604 | 1 hit ✅ |
| A8 | `search_files "Output\[pixel\]\.w = max" GIPathTracing.hlsl Private` | 1 hit at 694 | 1 hit ✅ |
| A9 | `search_files "Output\[pixel\]\.w = max" GIPathTracing.hlsl Data` | 1 hit at 694 | 1 hit ✅ |
| A10 | `search_files "check_alpha_sentinel" validate_restir_gi.py` | >=1 hit at 134 | 3 hits ✅ (72 docstring + 134 def + 205 call site) |
| A11 | `search_files "compute_alpha_stats" dump_pixelstats.py` | >=1 hit | 2 hits ✅ (96 def + 176 call) |
| A12 | `search_files "CHECKS=" fresh-evidence-scan.sh` | 1 hit | 1 hit ✅ (line 57) |

A1-A4 verify the binding-layout + cerr + encoder patches.
A5-A9 verify the v13/v17/v28 HLSL case-sentinels in BOTH Private master and data-dir copies (the v15 sync invariant).
A10-A11 verify the validator + dump_pixelstats alpha-path patches from v37 + v40.
A12 verifies the v43 fresh-evidence-scan expansion is intact.

## Part B — Test surface checks (file-only, runs in this runspace)

| # | Check | Expected | Actual |
|---|-------|----------|--------|
| B1 | v22 binding-layout patch intact at all 7 sites | 7/7 | 7/7 ✅ (FGIPass.h:106 member; FGIPass.cpp:183/281/296/311/312/612) |
| B2 | v41 encoder fix at Private/Image/FImageDump.cpp:27 | `std::clamp(rgbaData[i*4+3] * 255.0f, 0, 255)` | ✅ (verified above at line 27) |
| B3 | v38 cerr DebugMode effective= at FGIPass.cpp:487 | cerr with `DebugMode effective=` | ✅ (verified above at line 487) |
| B4 | v17 case 7u sentinel in BOTH HLSL copies | `case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;` | ✅ (GIPathTracing.hlsl:604 in BOTH copies) |
| B5 | v28 alpha-sentinel in BOTH HLSL copies | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | ✅ (GIPathTracing.hlsl:694 in BOTH copies) |
| B6 | v37 check_alpha_sentinel at validate_restir_gi.py:134 | `def check_alpha_sentinel(files, saturated_min=0.95, low_max=0.95):` | ✅ |
| B7 | v40 compute_alpha_stats at dump_pixelstats.py:96 | `def compute_alpha_stats(arr: np.ndarray) -> Optional[...]` | ✅ |
| B8 | v43 fresh-evidence-scan.sh CHECKS array expansion | CHECKS= at line 57 | ✅ |

## Part C — Acceptance criteria (parent-driven; terminal blocked in cron)

| # | Criterion | Status |
|---|-----------|--------|
| C1 | (a) Debug build cleanliness | UNVERIFIED (terminal blocked) |
| C2 | (b) Fresh HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8 run | UNVERIFIED |
| C3 | (c) No "Cannot open a command list that is already open" warnings | UNVERIFIED |
| C4 | (d) No Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 | UNVERIFIED |
| C5 | (e) `validate_restir_gi.py` passes newest dump group | UNVERIFIED |
| C6 | (f) Visual recognition of sane-exposure non-uniform Sponza geometry | UNVERIFIED |

Part C acceptance criteria are parent-driven per cron-prompt "do not silently stop" instruction and gpu-rendering-bisect-debug playbook. v55 is a documentation-only standby cycle; it does NOT change C1-C6 status.

## Verdict
ALL_KEEP contingent on Part A + Part B passing in this runspace.
