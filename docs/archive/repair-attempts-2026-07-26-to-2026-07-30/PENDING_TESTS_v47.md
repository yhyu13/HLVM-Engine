# Pending Tests v47

- commit: docs/PENDING_COMMIT_v47.md
- files: N/A
- source: no bundle — file-only
- target: N/A
- task: v47 structural standby tick tests
- notes: 0 source-code lines modified; no test surface introduced. v47 is documentation-only and inherits all prior test surface from v25-v46 (which is itself inherited from v22/v37/v38/v39/v40/v41).

## Tests (parent-driven; terminal blocked in cron)

### Part A: Static integrity check (file-only, executable in cron)
- [x] A1. `search_files pattern="DebugMode effective" path="Engine/Source/Runtime/Private/Renderer"` finds v38 cerr value-log at FGIPass.cpp
- [x] A2. `search_files pattern="case 6u:" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data"` finds v13 mode-6 sentinel
- [x] A3. `search_files pattern="case 7u:" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data"` finds v17 mode-7 sentinel
- [x] A4. `search_files pattern="check_alpha_sentinel" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data"` finds v37 alpha-check
- [x] A5. `search_files pattern="std::clamp(rgbaData\[i \* 4 \+ 3\]" path="Engine/Source/Runtime/Private/Image"` finds v41 alpha-encoder fix at FImageDump.cpp:27

### Part B: Runtime evidence capture (parent-driven; terminal blocked in cron)
- [ ] B1. Build: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` succeeds clean
- [ ] B2. Run default: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` produces `display_frame8.png`
- [ ] B3. Run mode-6: `HLVM_PT_DEBUG_MODE=6 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` produces per-pixel gradient in `gi_raw`
- [ ] B4. Vision: `display_frame8.png` shows recognizable non-uniform Sponza geometry with sane exposure
- [ ] B5. Validator: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` returns 4/4 PASS (3 original checks + v37 alpha-check)
- [ ] B6. v38 cerr: `cat stderr.log | grep "DebugMode effective"` shows 8+ lines (one per frame)
- [ ] B7. v28 alpha sentinel: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` shows `A:` stats line with `saturated` verdict