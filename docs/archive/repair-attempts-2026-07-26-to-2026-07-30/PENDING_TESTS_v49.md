# Pending Tests v49
- commit: docs/PENDING_COMMIT_v49.md
- files: N/A
- source: no bundle — file-only
- target: N/A
- task: v49 structural standby tick tests
- notes: 0 source-code lines modified; no test surface introduced. v49 is documentation-only and inherits all prior test surface from v25-v48 (which is itself inherited from v22/v37/v38/v39/v40/v41).

## Tests (parent-driven; terminal blocked in cron)

### Part A: Static integrity check (file-only, executable in cron)
- [x] A1. `search_files pattern="UAVBindingLayout"` finds v22 split header member in FGIPass.h + Shutdown clear in FGIPass.cpp + split-doc comments + grep entry in fresh-evidence-scan.sh (3 file hits)
- [x] A2. `search_files pattern="case 7u:"` finds v17 mode-7 sentinel in BOTH GIPathTracing.hlsl copies (Private + Data-dir)
- [x] A3. `search_files pattern="DebugMode effective"` finds v38 cerr value-log at FGIPass.cpp (5 grep-context hits at lines 485-489)
- [x] A4. `search_files pattern="check_alpha_sentinel"` finds v37 alpha-check in validate_restir_gi.py + dump_pixelstats.py + fresh-evidence-scan.sh (7 grep-context hits across 3 files)
- [x] A5. `search_files pattern="std::clamp\(rgbaData\[i \* 4 \+ 3\]"` finds v41 alpha-encoder fix at FImageDump.cpp (4 grep-context hits: R/G/B/Alpha clamps at lines 16/17/18/27)
- [x] A6. Full debug-switch range inspection of GIPathTracing.hlsl:575-704 — confirms BOTH copies are byte-identical with cases 1u-15u + default + v28 sentinel at line 692

### Part B: Runtime evidence capture (parent-driven; terminal blocked in cron)
- [ ] B1. Build: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` succeeds clean
- [ ] B2. Run default: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` produces `display_frame8.png`
- [ ] B3. Run mode-6: `HLVM_PT_DEBUG_MODE=6 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` produces per-pixel gradient in `gi_raw`
- [ ] B4. Vision: `display_frame8.png` shows recognizable non-uniform Sponza geometry with sane exposure
- [ ] B5. Validator: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` returns 4/4 PASS (3 original checks + v37 alpha-check)
- [ ] B6. v38 cerr: `cat stderr.log | grep "DebugMode effective"` shows 8+ lines (one per frame)
- [ ] B7. v28 alpha sentinel: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` shows `A:` stats line with `saturated` verdict
- [ ] B8. v22 binding-layout-split verification: build with v22 patch active produces zero `VUID-VkDescriptorImageInfo-imageLayout-00344` Vulkan validation warnings (the specific warnings the v22 split was designed to eliminate); if warnings persist, v22 fix was incomplete and v21b/c/f is the next step

### Part C: Helper-script bootstrap (parent-driven; terminal blocked in cron)
- [ ] C1. `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` returns exit 0 (all 21+ patches present)
- [ ] C2. `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` returns structured routing verdict
