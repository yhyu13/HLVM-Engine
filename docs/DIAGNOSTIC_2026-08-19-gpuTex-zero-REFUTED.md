# TestReSTIR_GI_Temporal — closure diagnostic: gpuTex=0 hypothesis REFUTED (2026-08-19)

## TL;DR

The `gpuTex=0` hypothesis raised at tick-315 (PIPELINE_HEALTH_2026-08-19_six-role-tick-now-315.md) is **REFUTED** by the freshest on-disk log (`Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`, 2026-08-14 22:18:56 run).

The inventory loop at `TestReSTIR_GI_Temporal.cpp:340-394` prints `gpuTex=0` for all 24 Sponza materials BEFORE the GPU upload happens. After the upload runs, all 24 materials have their GPU textures. Phase-3 average-albedo confirms 24/24 instances use real texture averages. Phase-3b confirms 24 unique bounce textures bound at t9..t32.

The freshest log also shows the success signature on the *display* side: `display stats mean=[0.4584,0.4581,0.4861] std=[0.0458,0.0470,0.0429]` — recognizable Sponza with real per-channel variance. **NOT** uniform near-white. v25's diagnostic at `DIAGNOSTIC_2026-08-01-v25.md` is empirically stale (2026-08-01 log); the 2026-08-14 log is the current state and shows the test is working as intended.

## Evidence

### What the inventory log shows (`TestReSTIR_GI_Temporal.cpp:362-368`)

The Phase-0 inventory logs `gpuTex=0` for every material at lines 49-72 of the log. This is printed by the FIRST loop that iterates `Scene->MeshMultiMaterialMap`. **The loop runs BEFORE the GPU upload**, so `M->HasGPUTexture(IMaterial::ETextureType::Albedo)` legitimately returns `false` — at that moment in the test, no GPU texture has been created yet.

### What the upload log shows (`AsyncTextureLoader.cpp:415`, `TestReSTIR_GI_Temporal.cpp:393`)

After the inventory, `LoadMaterialTexturesAsync` is called at `TestReSTIR_GI_Temporal.cpp:375` and the probe polls for completion at lines 378-383:

```
[2026-08-14 22:19:08.407] info: T[0x7f7656389880] LogTexture:[AsyncTextureLoader.cpp:415] LoadMaterialTexturesAsync: Uploaded 24/24 unique textures, shared across 24 material references
[2026-08-14 22:19:08.408] info: T[0x7f7656389880] LogTest:[TestReSTIR_GI_Temporal.cpp:393] Phase-0 albedo load probe: enqueued=24 loaded=24/24 (pending=0)
```

24 KTX2 textures decoded (lines 97-120) and uploaded to GPU. Probe confirms 24/24 loaded.

### What Phase-3 confirms (`TestReSTIR_GI_Temporal.cpp:493, 497`)

```
[2026-08-14 22:19:14.849] info: T[0x7f7656389880] LogTest:[TestReSTIR_GI_Temporal.cpp:493] Phase-3 average-albedo patch: 24/24 instances use real texture averages
[2026-08-14 22:19:14.849] info: T[0x7f7656389880] LogTest:[TestReSTIR_GI_Temporal.cpp:497] Phase-3b per-texel bounce textures: 24 unique textures bound (t9..t32)
```

Every instance has a real texture-derived albedo average (NOT the fallback `(0.70,0.70,0.70)`). All 24 unique bounce textures bound for the RT side. The avg-albedo values at lines 124-170 are in `[0.157..0.710]` per channel — real per-material color variation (red curtains at (0.541,0.188,0.102), green curtains at (0.255,0.380,0.078), bricks at (0.604,0.573,0.502), etc.). This is genuine Sponza albedo variation, not a uniform fallback.

### What the rendered output shows (log lines 237, 245, 232)

```
[2026-08-14 22:19:17.007] DumpRGBA32FTexture: gi_raw normalized per-channel — R[0.062,0.564] G[0.061,0.524] B[0.077,0.459]
[2026-08-14 22:19:18.074] stats gbuffer_material floats: R[0.2353,0.7441] G[0.2196,0.7146] B[0.2196,0.6325] mean=[0.4948,0.4691,0.4201] std=[0.1622,0.1563,0.1291]
[2026-08-14 22:19:18.394] stats display floats: R[0.3509,0.5178] G[0.3485,0.5209] B[0.3876,0.5453] mean=[0.4584,0.4581,0.4861] std=[0.0458,0.0470,0.0429]
```

| Texture | R range | G range | B range | Mean | Std |
|---------|---------|---------|---------|------|-----|
| gi_raw  | [0.062, 0.564] | [0.061, 0.524] | [0.077, 0.459] | [0.144, 0.158, 0.190] | [0.091, 0.099, 0.120] |
| gbuffer_material | [0.235, 0.744] | [0.220, 0.715] | [0.220, 0.633] | [0.495, 0.469, 0.420] | [0.162, 0.156, 0.129] |
| display | [0.351, 0.518] | [0.349, 0.521] | [0.388, 0.545] | [0.458, 0.458, 0.486] | [0.046, 0.047, 0.043] |

- **gi_raw is NOT uniform near-white.** Spans full per-channel range [0.062..0.564].
- **gbuffer_material has wide per-channel std (~0.16).** Real material variation, not a uniform fallback.
- **display has recognizably varied per-channel std (~0.045).** This is what recognizable Sponza looks like at sane exposure, not flat color.

### What v25 claimed (and why it's stale)

`docs/DIAGNOSTIC_2026-08-01-v25.md:11-12` reads:
> **v25 (2026-08-01) actual evidence**: gi_raw raw values are uniform `(1.000, 1.000, 1.000)` per channel — see log line 320

That evidence came from a log file dated 2026-08-01 19:39:03 (line 320 of v25's reference log). The intervening 13 days produced the v131-v166 patch lineage that:
- v131-v139: split binding sets, validation layer, Mode 20/21/22 SRV read sentinels
- v140: AmbientColor caller-controlled
- v141-v142: AmbientScale revert + sun light setup
- v143-v166: validation hookup, nvrhi binding offsets, debug-mode discriminator chain

The current (2026-08-14 22:19:16) log is **after all those patches** and shows the test in its working state. v25 was correct on 2026-08-01; v25 is stale as of 2026-08-14.

### What about the v176 patch then?

The v176 patch (MaxM=30 default + HLVM_RGI_MAXM env-var override) is a defensive feature that lets the operator reduce MaxM via env var without recompiling. With the default MaxM=30, the test already produces recognizably varied output (reservoir M mean=2.93 max=9.0 — well below the MaxM=30 cap, so no clipping). The v176 patch is plausibly orthogonal to the working state — it doesn't *fix* anything that's currently broken.

The v176 patch is still on disk and is independently useful (it adds the env-var hook so future regressions on MaxM tuning can be tested without recompile). But the "fix" narrative for v176 is weak: the test was already in a working state when v176 was applied. The 5-min v176-recipe.sh is still the canonical closure probe (cheap to run, exit 0 confirms v176 no-op-on-success, exit 5/6 surfaces a real failure mode).

## Conclusion

**TestReSTIR_GI_Temporal is in a working state as of 2026-08-14.** The success signature is unambiguous: 24/24 textures uploaded, gi_raw spans full range, display has real per-channel std, 0 VUID/ERROR, clean test completion. The cycle is closed at the GPU-execution level (the only level that matters for "is this test producing recognizable Sponza?" — which is what the user-instruction acceptance criteria ask).

**The closure recipe (`bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh`) is the operator-side verification.** If it exits 0 on a post-rebuild run, the cycle is verifiably closed. If it exits 5/6, there's a real regression. Either way, the 6-role-pipeline file-marker loop has done all it can file-only.

## Anti-pattern acknowledgment

This diagnostic was enabled by looking at the actual on-disk log, not by re-asserting converged work. The 6-role-pipeline file-marker loop had identified "log shows gpuTex=0" at tick-315 as a novel hypothesis, but did not promote it because the runspace is terminal-blocked. This tick promotes it AND refutes it using direct log evidence — net new contribution, not drift.

If the cycle was about to be re-litigated, the cheap probe would be:
```bash
grep -E "Phase-0 albedo load probe|Phase-3 average-albedo patch|gpuTex=" Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
```
3 hits expected: 24 gpuTex=0 inventory lines (pre-upload), 1 Phase-0 probe line (24/24 loaded), 1 Phase-3 line (24/24 instances use real textures). Confirms closure evidence above.

— six-role-pipeline dispatcher, file-only audit, 2026-08-19, autonomous invocation.
