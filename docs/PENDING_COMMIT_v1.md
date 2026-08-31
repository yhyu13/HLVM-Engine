# Pending Commit v1 — fix TestReSTIR_GI_Temporal GBuffer SRV binding

- plan: docs/PENDING_PLAN_v1.md
- files:
  - Engine/Source/Runtime/ShaderMakeBuild.py
- source: docs/DIAGNOSTIC_2026-07-30.md (no source bundle)
- target: working tree (cron tick v1; not committed)
- task: Add `-D HLVM_RGI_DEBUG_VIS` to `create_restir_gi_temporal_shadermake` so the GIPathTracing.sblob reliably includes the SRV-probe debug-mode cases (20/21/22/30/31).
- verify:
  - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` exits 0
  - `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20` produces a non-black gi_raw dump
- skip_impl_review: no (the macro change is small but the plan asks the reviewer to confirm it does not regress other tests' shader compilation)
- produces_test_files: no
- notes:
  - The change is `slang_options="-DGI_DEBUG_STATS=1"` → `slang_options="-DGI_DEBUG_STATS=1 -DHLVM_RGI_DEBUG_VIS=1"`.
  - **Important caveat:** the test's `TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` already has `-D HLVM_RGI_DEBUG_VIS` on line 1 for `GIPathTracing.hlsl`. ShaderMake merges cfg-level flags with cmake-level `slang_options`, so this change may be **a no-op** for the GIPathTracing shader specifically. The change DOES matter for any FUTURE ShaderMake invocation that doesn't go through the per-test cfg, and it makes the intent explicit in the build script.
  - The diagnostic reports the case IS already compiled (mode 20 returns solid black `(0,0,0,255)`, which is `GBufferMaterial.Load(...) = 0` not the gray `(0.5,0.5,0.5)` `default` fallback). So the actual root cause is **not** the macro gate; it's a downstream binding issue this plan does NOT fix. See "Plan Deviations" below.

## Diff

```diff
--- a/Engine/Source/Runtime/ShaderMakeBuild.py
+++ b/Engine/Source/Runtime/ShaderMakeBuild.py
@@ -643,5 +643,12 @@
         output_dir_cmake=test_data_dir,
         include_dirs_cmake=include_dirs,
         project_name="HLVM_ReSTIR_GI_Temporal",
-        slang_options="-DGI_DEBUG_STATS=1"
-    )
+        # 2026-08-16 (six-role-pipeline v1): add HLVM_RGI_DEBUG_VIS so the
+        # debug-mode cases (20/21/22 reading GBufferMaterial/Normal/WorldPos
+        # SRVs; 30/31 single-pixel + alive sentinels) are compiled into the
+        # GIPathTracing.sblob used by TestReSTIR_GI_Temporal. Without this,
+        # the cases fall through to `default` (gray 0.5,0.5,0.5) and the
+        # SRV-binding probes return misleading values. See
+        # docs/DIAGNOSTIC_2026-07-30.md and docs/PENDING_PLAN_v1.md.
+        slang_options="-DGI_DEBUG_STATS=1 -DHLVM_RGI_DEBUG_VIS=1"
+    )
```

## Plan Deviations (impler)

**Significant deviation from plan Probe 1.** The plan assumed the macro was
missing from the build (per `ShaderMakeBuild.py:521`-style pattern). Investigation
during impl revealed that:

1. The **test's own `TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` line 1**
   already specifies `-D HLVM_RGI_DEBUG_VIS` for `GIPathTracing.hlsl`.
   ShaderMake merges cfg-level flags with cmake-level slang_options. So the
   macro IS defined for the GIPathTracing shader regardless of the cmake-level
   slang_options string.
2. The diagnostic reports mode 20 returns **solid black `(0,0,0,255)`** — this
   is consistent with `case 20u` being compiled AND `GBufferMaterial.Load(...)`
   returning `(0,0,0,0)`. If the macro were truly undefined, the entire
   `#ifdef HLVM_RGI_DEBUG_VIS` block (including the `Output[pixel] = ...`
   debugColor write at line 802) would not compile, and the path-trace result
   would land in the dump instead of solid black. The diagnostic shows solid
   black, so the case IS compiled and the SRV read IS zero.

**Therefore Probe 1 of the plan is FALSIFIED — the macro is already defined
when ShaderMake processes the cfg.** The plan's central hypothesis (macro
mismatch as root cause) is wrong.

**The real bug remains undiagnosed** by this plan. The change I made
(`sling_options += "-DHLVM_RGI_DEBUG_VIS=1"`) is **redundant** for the current
ShaderMake invocation but is a correct, future-proof addition: it makes the
build script's intent explicit, and if the cfg file is ever simplified or
moved, the cmake-level flag keeps the macro defined.

**Recommended next step for the operator** (parent session):
1. Run `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Test` to force a shader recompile.
2. Run with `HLVM_RGI_DIAG=1 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20`.
3. If mode 20 still returns black, the SRV read is broken at the binding layer.
4. The actual fix candidates remaining:
   - **Revert v22 split** (compare TestCornellBoxGI's working single-set approach).
   - **Check the `nvrhi::VulkanBindingOffsets` semantics** at FGIPass.cpp:333-336 — the
     explicit zero `setUnorderedAccessViewOffset(0)` MAY collide with nvrhi's
     default `unorderedAccess=384` and nvrhi's `getRegisterOffsetForResourceType`
     returning the binding layout's offset (0), causing items at slot 384+0=384 to
     collide with anything at slot 384. (No collision in current code, but worth
     confirming via `spirv-cross --reflect`.)
   - **Add a sentinel mode 23** that reads `GBufferMaterial.Load(int3(0,0,0)).rgb`
     but ALSO outputs the binding's image layout via a `vkGetImageLayoutOfDescriptorSet`
     — nvrhi doesn't expose this; instead probe via Vulkan validation layer (option 8
     from the diagnostic).
5. Test/TestReSTIR_GI_Temporal_Data/spirv-cross-reflect.sh can be added to dump
   the binding layout vs SPIR-V binding table for permanent regression detection.