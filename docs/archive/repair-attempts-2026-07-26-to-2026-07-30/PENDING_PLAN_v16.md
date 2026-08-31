# Pending Plan v16 — correct pipeline's data-dir/Private-master understanding + stage v17 next-sentinel probe

- task: correct the pipeline's persistent misunderstanding about which GIPathTracing.hlsl copy slangc actually compiles into the test binary, and stage the next probe (mode 7 — bypass TraceRay entirely) so the parent has a cleaner evidence-collection path on the next rebuild/run.
- source: docs/PIPELINE_HEALTH_2026-07-27.md (this tick's audit) + on-disk inspection of `Engine/Source/Runtime/ShaderMakeBuild.py:612-622` and `Engine/Source/Runtime/CMakeLists.txt:1877`
- approach: 1) write this plan + plan-review + commit + impl-review + tests + test-audit + append health; 2) update `docs/PENDING_PICK.md` to mark v16 [x] and stage v17 as a planned-but-unfired candidate; 3) NO source-code changes in this cycle (file-only diagnostic cycle).
- skip_plan_review: no — this cycle updates the pipeline's understanding of which files matter; a self-critique would catch missed scopes (other Private-master/data-dir pairs to check).
- diff_estimate: +6 / -0 lines of source code; this is a doc-only cycle.

## Why this is the right v16 cycle

### The corrected understanding

**`Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` is the canonical file slangc compiles into TestReSTIR_GI_Temporal's binary.**

Verified via three independent sources:

1. **`Engine/Source/Runtime/ShaderMakeBuild.py:613`** — the `create_restir_gi_temporal_shadermake` factory's `shader_sources` list contains `gi_shader_dir + "/GIPathTracing.hlsl"` where `gi_shader_dir = "${CMAKE_SOURCE_DIR}/Private/Renderer/Shader/GI"`. This is the file passed to ShaderMake, which in turn invokes slangc.

2. **`Engine/Source/Runtime/CMakeLists.txt:1877`** — the `TestReSTIR_GI_Temporal_ShaderMake` `add_custom_target` lists `"${CMAKE_SOURCE_DIR}/Private/Renderer/Shader/GI/GIPathTracing.hlsl"` as a DEPENDS. CMake's CMake-time file list is what drives the build invocation.

3. **`Engine/Source/Runtime/Build/Debug/build.ninja:2476`** — the actually-generated build rule for `TestReSTIR_GI_Temporal_ShaderMake`:
   ```
   ShaderMake ... slangc ... /home/hangyu5/.../Private/Renderer/Shader/GI/GIPathTracing.hlsl
   ```
   Confirmed at the generated-ninja level.

The data-dir copy at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` is **never compiled**. It exists as a dead duplicate, discoverable via the include_dirs in ShaderMakeBuild.py:625 (test_data_dir is listed first), but no file in the data-dir `#include`s `GIPathTracing.hlsl` (verified by `search_files pattern="#include.*GIPathTracing"` — 0 matches). The data-dir copy is unreachable dead code.

### Why this matters

This finding rewrites the pipeline's understanding of the v3-v15 cycles:

| Cycle | File patched | Lands in binary on next rebuild? |
|-------|--------------|-----------------------------------|
| v3    | TestReSTIR_GI_Temporal.cpp, FGIPass.cpp (C++) | YES — C++ changes always land |
| v5    | TestReSTIR_GI_Temporal.cpp (C++) | YES |
| v7    | TestReSTIR_GI_Temporal.cpp (comment-only) | YES (no behavior change) |
| v8    | TestReSTIR_GI_Temporal.cpp (comment-only) | YES (no behavior change) |
| v11   | TestReSTIR_GI_Temporal.cpp, FGIPass.cpp (C++) | YES |
| v12   | TestReSTIR_GI_Temporal.cpp, FGIPass.cpp (C++) | YES |
| v13   | Data-dir GIPathTracing.hlsl (case 6u) | **NO** — never compiled |
| v14   | TestReSTIR_GI_Temporal.cpp (comment-only) | YES (no behavior change) |
| v15   | Private-master GIPathTracing.hlsl (case 6u) | YES — this is the canonical file |

**v15 was not cosmetic. v15 was load-bearing.** Without v15, the next parent rebuild would still produce a binary WITHOUT case 6u, regardless of v13 having been applied to the data-dir copy. The pipeline has been operating under a misapprehension about where patches land.

This also reinterprets the v9 "source/binary mismatch" finding: the binary is up-to-date with the Private master, but the pipeline was comparing against the data-dir copy. The mismatch wasn't source-vs-binary — it was wrong-source-vs-binary. The data-dir copy was being treated as if it mattered.

### The decision matrix update

The v13a decision matrix (in `docs/PENDING_PLAN_v13.md`) was correct in shape but its foundation was wrong: it assumed "mode-6 evidence shape" was a test of whether the data-dir-compiled binary ran. With the corrected understanding:

- If parent runs default mode (mode 0) and gi_raw is non-zero → bug was in some downstream component, NOT in dispatch body
- If parent runs mode 6 (per-pixel gradient) and gi_raw shows per-pixel gradient → Private-master patch landed correctly AND dispatch body runs AND UAV write lands. Bug is in lighting math.
- If parent runs mode 6 and gi_raw is 0 → either (a) dispatch didn't run, OR (b) the Private master patch did not land (unlikely after rebuild from current source), OR (c) slangc dead-stripped the case

The mode 6 result is still the decisive test, but its interpretation is now grounded in actual build mechanics.

## What this cycle does NOT do

- Does NOT modify any source file. This is a doc-only cycle.
- Does NOT update `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (already in sync post-v15).
- Does NOT update `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (also in sync post-v15).
- Does NOT remove the dead data-dir copy. That's a future cleanup cycle (out of scope).
- Does NOT modify any C++ file. v3, v5, v7, v8, v11, v12, v14 patches are already correct and load-bearing.
- Does NOT preempt parent action. The v13a decision matrix is still correct in shape; v16 just corrects its foundation.

## What parent must do (unchanged from v15)

The parent action items are unchanged. v16 changes the pipeline's understanding but not the steps needed to verify:

1. Rebuild: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`.
2. Run: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`.
3. Run mode 6: same command with `HLVM_PT_DEBUG_MODE=6` and inspect `gi_raw` for the per-pixel gradient.
4. Validate: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (expect 3/3).
5. Vision-analyze `display_frame8.png` for recognizable, non-uniform Sponza geometry with sane exposure.
6. Verify v15 sync on next rebuild: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` should show 0 lines of difference outside header comments.
7. **NEW (v16):** Verify which file slangc actually compiled by inspecting the build log: `Engine/Source/Runtime/build_Debug.log` should contain a line like `(100.0%) HLVM_ReSTIR_GI_Temporal SPIRV GIPathTracing.hlsl {main} {}` — confirming the Private master was the source.

## v17 candidate (staged but not fired)

**v17 plan candidate: add `case 7u` to GIPathTracing.hlsl that bypasses TraceRay entirely.**

If mode 6 (per-pixel constant write) confirms the dispatch body runs and the UAV write lands, the next decisive probe is mode 7: dispatch the shader, do everything mode 6 does, BUT bypass TraceRay and the entire payload/read infrastructure — directly compute a known lighting result via the diffuse * AmbientColor * AmbientScale path.

If mode 6 produces per-pixel gradient AND mode 7 produces known non-zero result → the bug is somewhere in the TraceRay / payload / SRV-read chain (probably payload layout desync or slangc dead-strip, the classic from the 51 retrospective).

If mode 6 produces per-pixel gradient BUT mode 7 also produces garbage → bug is in something even more fundamental (framebuffer state, accumulate/denoise/ReSTIR passes, tonemap).

This is staged in PENDING_PICK.md as the next cycle AFTER v16 closes and parent supplies v15-build evidence. v17 is NOT fired in this tick because:

1. The "v13a decision matrix" is still the canonical parent-evidence-driven next step. v17 fires AFTER v13a evidence arrives.
2. v17 is dependent on mode 6 confirming dispatch body works. If mode 6 shows 0, v17 is the wrong probe (need v13a-2 to investigate debugMode cbuffer reach first).
3. v17 is also load-bearing on the Private master copy (mode 7 patch would go to Private master, not data-dir).

The decision to stage v17 in PICK without firing it preserves the parent-evidence-driven flow while making the next probe immediately available when the time comes.

## Honesty caveats

- v16 is documentation-only. It does NOT change the renderer.
- The corrected understanding doesn't change what parent must do — only what the pipeline expects to learn from each piece of evidence.
- "v15 was load-bearing" is a retrospective finding. Forward-looking, the pipeline should always check ShaderMakeBuild.py before assuming a data-dir HLSL patch lands.
- Single-head caveat applies: the planner, plan-criticer, and impler are the same head. KEEP verdicts are self-checks.

## diff_estimate

- 0 source code changes in this cycle
- `docs/PENDING_PLAN_v16.md`: new
- `docs/PENDING_PLAN_REVIEW_v16.md`: new
- `docs/PENDING_COMMIT_v16.md`: new
- `docs/PENDING_IMPL_REVIEW_v16.md`: new
- `docs/PENDING_TESTS_v16.md`: new
- `docs/PENDING_TEST_AUDIT_v16.md`: new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: append this tick's section
- `docs/PENDING_PICK.md`: append v16 [x] entry, stage v17 candidate as parent-evidence-gated

## test_strategy

- No new test files needed.
- Parent-driven test: verify the corrected understanding by inspecting build_Debug.log for the slangc invocation line. If the line shows `Private/Renderer/Shader/GI/GIPathTracing.hlsl`, the understanding is correct.

## risks

- The other tests (TestPathTraceGI, TestCornellBoxGI, TestFewBounceGI) also use Private-master GI shaders. None of them were patched in any v3-v15 cycle, so this finding does not introduce regressions elsewhere.
- Future patches to any data-dir HLSL must first verify (via ShaderMakeBuild.py) whether the data-dir copy or the Private master is the canonical source. The v15 plan already flagged this risk; v16 makes it explicit.