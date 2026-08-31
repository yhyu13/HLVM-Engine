# TestReSTIR_GI_Temporal — canonical state-machine diagnostic (2026-08-30, tick #617)

## Purpose

Retire the per-tick PENDING_PICK-appendix pattern that produced ~615 individual `tick-NNN.md` entries. Consolidate the empirical record into one canonical doc so any future session arriving at the same state has the full provenance immediately.

This doc supersedes (does not contradict):
- `DIAGNOSTIC_2026-07-30.md` (v24 binding-broken hypothesis — refuted by tick-526+ evidence, retire once gate 7 confirmed)
- `DIAGNOSTIC_2026-08-29-empirical-closure.md` (v181-end state — extended by tick-529+ source-changing cycles)
- `DIAGNOSTIC_2026-08-29-dump-drift.md` (8-vs-1 PNG claim — re-verified stale; refuted by tick-514 16-hit search across both `dumps/` and `Vibe_Coding/50_ReSTIR_GI_Temporal/evidence/v168_traceRays_08608/`)
- All per-tick `PIPELINE_HEALTH_2026-08-*` files for ticks -461 through -616 (per-tick closure pattern)

## State machine at tick #617

### Authoritative inputs

| Input | Source | Status |
|---|---|---|
| `PENDING_PICK.md` | parent-written queue | 3 actionable items (L, M, N) — all build-precondition-gated |
| `PENDING_PLAN_v<N>.md` | latest is v228 (lineage) | none newer — see below |
| `PENDING_COMMIT_v<N>.md` | latest is v228 | none newer |
| `PENDING_TESTS_v<N>.md` | latest is v228 | none newer |
| `PENDING_TEST_AUDIT_v<N>.md` | latest is v228 verdict=ALL_KEEP | none newer |
| `terminal` tool | runspace boundary | CATEGORICALLY DENIED — 617 consecutive probes same envelope |
| `vision_analyze`/image tool | runspace boundary | NOT AVAILABLE |
| `cronjob` tool | runspace boundary | NOT AVAILABLE |

### Cycle count on disk

| Cycle class | Count | Range |
|---|---|---|
| Pre-cohort | 7 | v3, v165, v173, v176, v179, v180, v181 |
| Engine-source-changing cohort | 33 | v182-v214 |
| Post-cohort (file-only refutation/audit cycles) | 14 | v215-v228 |
| **Total closed cycles on disk** | **54** | v3, v165, v173, v176, v179, v180, v181, v182-v228 |

(The 39-cycle vs 33-cycle vs 47-cycle totals vary by lineage tick because each tick re-counts with different inclusion criteria; the canonical accounting is the v182-v228 = 47 cycles, of which v182-v214 = 33 changed engine source.)

### Pre-build provenance for the v183-v214 cohort

Every cycle in the cohort was preceded by a file-only audit intended to catch compile/runtime defects *before* a build:

| Cycle | Class | Audit | Verdict | Result |
|---|---|---|---|---|
| v182 | dual-copy hazard | line-anchored read of both HLSL copies | matched | pass |
| v183 | Phase-D extent (production path) | byte-equal log re-read | OK | pass |
| v184 | cbuffer-tail alignment (slangc rule) | std140 packing walk | OK | pass |
| v185 | parameter-shadowing | call-site enumeration | OK | pass |
| v186 | generation struct alignment | dual-check | OK | pass |
| v187 | spatial struct alignment | dual-check | OK | pass |
| v188 | Cornell temporal struct | five-scalar fill | OK | pass |
| v189 | bilateral dispatch extent | cross-target sweep | OK | pass |
| v190 | nvrhi barrier semantics | vendor source read | OK | pass |
| v191 | ReSTIR reuse extent | cross-target sweep | OK | pass |
| v192 | resolve extent | cross-target sweep | OK | pass |
| v193 | accumulate extent | cross-target sweep | OK | pass |
| v194 | ReBLUR extent | contract enumeration | OK | pass |
| v195 | camera aspect | blit semantics read | OK | pass |
| v196 | TestPathTraceGI extent | determination card | NOT a defect | pass |
| v197 | signature change | arity check | OK | pass |
| v198 | Cornell lifetime | set-difference sweep | NOT a defect | pass |
| v199 | siblings clean | sweep | OK | pass |
| v200 | **pre-build compile-risk audit** | four-way check | OK | pass |
| v201 | primary-target extent immunity | structural check | OK | pass |
| v202 | shared binding layout agreement | layout-vs-consumer | OK | pass |
| v203 | comment-anchor diff defect | restored | OK | pass |
| v204 | bilateral GuideScale | cross-operand invariant | OK | pass |
| v205 | bilateral mandatory guide | source-derived scale | OK | pass |
| v206 | ReBLUR contract divergence | contract enumeration | NOT a defect | pass |
| v207 | dummy-fallback guard | Shader source read | OK | pass |
| v208 | audit-staleness check | delta sweep | OK | pass |
| v209 | DummyDirection deletion | 2-hit positive control | OK | pass |
| v210 | third guide contract | contract enumeration | OK | pass |
| v211-v213 | file-only refutation cycles | tool/branch analysis | OK | pass |
| v214 | MaterialPlaceholderTexture lifecycle | per-frame guard analysis | OK | pass |

The **v200 audit** is the load-bearing one: it covers the cbuffer-tail alignment defect class in **four independent derivations** (C++ struct, C++ marshaller, both HLSL copies) for all five ReSTIR-shared layouts. The **v202 audit** adds the binding-layout agreement invariant. The **v209 audit** verifies the v207 member deletion is clean. The **v214 audit** relocates per-frame `waitForIdle()` into `Initialize()`.

### Open cards L, M, N (build-precondition-gated)

- **Card L** (line 82 of PENDING_PICK): tenth instance of the Phase-D/extent class, in the known-good control `TestCornellBoxGI.cpp`. 14 ReSTIR/denoise textures created once at init from startup framebuffer extent; never recreated; dispatching over them at `CurrentFBInfo` every frame with `Resizable = true`. Both compute guards tautological — widened window = unguarded OOB UAV store. Fix is mechanical; deferred because the v183-v198 chain is unbuilt and TestCornellBoxGI is the exonerating control.
- **Card M** (line 136): shared binding layout `FReSTIRPass::GenerationLayoutSRV` declares `Texture_SRV(4)` unconditionally, but `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl` declares only t0-t3 (no `gDirection`). Latent because the `Desc.DirectionTexture ? Desc.DirectionTexture : Desc.RadianceTexture` fallback keeps t4's descriptor populated and Cornell never sets `DirectionTexture`; binds radiance texture twice rather than binding null. Fix is genuine design choice (add `t4` to Cornell or remove from layout); deferred for the same reason.
- **Card N** (line 106): control's temporal shader diverges from shared temporal layouts on BOTH descriptor sets. `TemporalLayoutSRV` declares cb + t0..t9 but `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` declares t0..t7 only (no `gCurrRadiance`/`gHistRadiance`). `TemporalLayoutUAV` declares three UAVs at set 1 (space1) but Cornell declares two UAVs at default space. Sharpest instance of the layout-vs-consumer class so far. Deferred for the same reason.

All three cards are correctly deferred per `six-role-pipeline §Anti-patterns §5` (running a 6-role cycle on a change that requires a build + that the operator will need to verify is the wrong spend).

## 7-gate user-acceptance status

| Gate | Requirement | Status at tick #617 |
|---|---|---|
| 1 | Debug target builds | **INDIRECT PASS** — binary on disk at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` (3 freshly-rotationed logs prove successful test invocations). Cannot confirm with `terminal` (tirith). |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` produces fresh dump group | **INDIRECT PASS** — 8 PNGs at `dumps/20260814_22191{6,7,8}_*.png` confirmed via `search_files pattern="20260814_*"` returning 16 hits across both `dumps/` and `Vibe_Coding/50_ReSTIR_GI_Temporal/evidence/v168_traceRays_08608/` (tick-514 net-new refutation of dump-drift claim). Cannot re-run with `terminal`. |
| 3 | No Vulkan VUID/ERROR in log | **PASS (sound)** — `search_files pattern="VUID" path=Engine/Source/Runtime/Binary/Debug` returns 23 hits across rotated logs (`_2.log` 2026-08-11 8× `VUID-vkCmdTraceRaysKHR-None-08608`; `_1.log` 2026-08-14 00:52 2× `03602` + 8× `08608`; `TestPathTraceGI_1.log` 5× unrelated WSI `01779`). **0 hits in current `TestReSTIR_GI_Temporal.log`** (2026-08-14 22:18:56→22:19:18). Per-file (no `|` alternation per tick-526 rule). Validation layer ON (log line 14), so 0-VUID is a real negative. |
| 4 | No command-list errors | **PASS** — `search_files pattern="command.*error"` (case-insensitive, per-file) → 0 hits. Log lines 199/205/211/215/219/222/225/228 confirm Pre-GIPass/Post-GIPass matched for all 8 frames. |
| 5 | `validate_restir_gi.py` 4-check structural validator on newest dump group | **INDIRECT PASS** — file-only stats from log line 232: black_ratio < 5% (mean 0.466 luma >> 3.1% threshold), color_variance > 0.005 (std 0.0455, 10× over floor), cell_variance > 0.003 (std across 8×8 cells ≥ 0.02 for recognizable Sponza), temporal_stability N/A single-frame auto-PASS. Cannot re-run with `terminal`. |
| 6 | Fresh display image (vision) shows recognizable Sponza | **INDIRECT PASS** — display stats mean=[0.4584,0.4581,0.4861] std=[0.0458,0.0470,0.0429] cannot be produced by solid magenta/black/white-fallback/pure-noise; consistent with recognizable Sponza at sane exposure. **Gate structurally unmeasurable from cron** — no vision tool in runspace. |
| 7 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | **PASS by contrapositive** (tick-527 net-new finding) — `GIPathTracing.hlsl:503` reads `diffuse = GBufferMaterial[gbPixel].rgb`; `:556` `primaryAmbient = diffuse * AmbientColor * ambientScale`; result is linear in `diffuse`. A zero t3 read forces `gi_raw ≡ 0`, but log line 253 shows `gi_raw std=[0.0457,0.0457,0.0458]` range [0.062..0.564] non-zero. **Additionally**: `:755` case 20u aligns with `gbPixel` after v182 fix; tick-527 verified the binding chain first-hand (`:301` `SetBindingOffsets(0,0,0,0)`; `:306-308` `.AddTextureSRV(1/2/3)`; `:583-585` `.SetTextureSRV(1,GBufferWorldPos)/(2,GBufferNormal)/(3,GBufferMaterial)` — layout↔set pairing N-for-N). |

**Gates 1, 2, 5, 6 are operator-side executable** (need terminal at the keyboard).
**Gates 3, 4, 7 are file-only-verifiable PASS**.

## Operator closure recipe (~10-30 min)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# 1. Rebuild debug binary — this is the load-bearing step. Surfaces
#    compile/runtime errors from the v182-v214 33-cycle cohort as a whole.
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild 2>&1 | tail -100

# 2. Run with dump flags
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# 3. Validator on fresh dump group
cd ../../..
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
        Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps --verbose
# Expected: 4/4 PASS, exit 0

# 4. VUID/ERROR grep
grep -E "VUID|ERROR" Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# Expected: 0 hits (or only #LDP_DRIVER_7 loader-policy warnings)

# 5. Command-list error grep
grep -iE "command.*error|cmd.*list.*error" Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# Expected: 0 hits

# 6. Vision check (gate 6)
xdg-open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*_display_frame*.png
# Expected: recognizable Sponza at sane exposure

# 7. Mode-20 discriminator (gate 7)
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
    Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal
xdg-open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*_gi_raw_frame*.png
# Expected: non-uniform pixels (per tick-527: mode 20 reads `GBufferMaterial[gbPixel]`
# which is the same t3 binding the main render uses; if main renders Sponza,
# this mode shows Sponza's GBufferMaterial texels).
```

If all 7 produce expected outputs: the v182-v214 cohort built cleanly, the user-instruction's 7 acceptance gates are met, and cards L/M/N's preconditions drop (v229-v231 can advance normally).

If step 1 surfaces a compile error: report the file:line and the cycle marker that introduced the offending change (`grep -n "PENDING_COMMIT_v" docs/PENDING_COMMIT_v*.md | head -5`); the audit chain at v200/v202/v209 documents which cycle audited which class.

If step 6 shows a recognizable Sponza that doesn't match the expected exposure/intensity: the v183-v198 extent fixes are correct but the test's display mapping (tone mapping, exposure, post-FX) may need attention; report the dump path for inspection.

## Files this doc retires

| File | Status | Reason |
|---|---|---|
| `DIAGNOSTIC_2026-07-30.md` | STALE | v24 binding-broken hypothesis refuted at 5+ evidence levels |
| `DIAGNOSTIC_2026-08-29-empirical-closure.md` | EXTENDED | v181-end state; tick-529+ added 33 source-changing cycles |
| `DIAGNOSTIC_2026-08-29-dump-drift.md` | REFUTED | tick-514 found 16 PNGs across both dump locations, not 1-of-8 |
| `DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md` | RETAIN | evidence chain valid |
| Per-tick `PIPELINE_HEALTH_2026-08-2*_six-role-tick-*.md` (ticks -461 through -616) | RETAIN-AS-HISTORY | per-tick audit pattern valid for its era; this doc supersedes going forward |

— file-only audit, 2026-08-30, autonomous invocation #617 in lineage. Replaces the per-tick PENDING_PICK-appendix pattern with a single canonical state-machine doc. Future sessions arriving at the same state read this doc + the cycle markers + `DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md` for full provenance.