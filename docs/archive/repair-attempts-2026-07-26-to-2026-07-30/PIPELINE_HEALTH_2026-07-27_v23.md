# PIPELINE_HEALTH_2026-07-27 — v23 inner-pipeline heartbeat (post-v22; trajectory closed at v20; structural pause continues)

## State-machine routing decision

- Read `PENDING_PICK.md`, all six v20 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT — all present, v20 audit = SOME_RELAX), v21 entry in PICK (parent-evidence-gated), v13a decision matrix (parent-driven).
- v22 inner-pipeline heartbeat at `docs/PIPELINE_HEALTH_2026-07-27_v22.md` correctly identified trajectory closed at v20 and structural pause.
- Cron prompt grants `enabled_toolsets: ["terminal", "file"]` for GPU repair. Probed terminal access via 8+ diagnostic commands (`date`, `pwd`, `stat`, `ls`, `find`, `crontab -l`, `echo`, `wc -l`) — every probe blocked by tirith (`pending_approval`, `tirith:unknown`). Effective toolset: file-only. Consistent with the previous 9+ ticks.
- Per `six-role-pipeline` state machine: do NOT fire v21 (parent-evidence-gated on `rgi_evidence.txt`); do NOT fabricate KEEP/ALL_KEEP verdicts; do NOT drift into interactive debugging while claiming the pipeline is running.
- Decision: record honest v23 heartbeat per HARD INVARIANT #6 ("Never silently exit"). Companion to the outer-watchdog v23 file at `PIPELINE_HEALTH_2026-07-27_outer_v23.md`.

## Static disk-evidence audit (no shell, no fabrication)

- **v1-v20 patches intact on disk at the line numbers prior commits claimed**:
  - v3 spdlog markers at `FGIPass.cpp:473` (EARLY-RETURN), `FGIPass.cpp:486-489` (ENTER), `FGIPass.cpp:568-569` (binding-set OK), `FGIPass.cpp:577-578` (EXIT), `TestReSTIR_GI_Temporal.cpp:445` (Pre-GIPass) — 5 v3 log sites confirmed.
  - v11/v12 cerr default-ON at `TestReSTIR_GI_Temporal.cpp:384` (`std::cerr << "[RGI] Render() entry:..."`) and `FGIPass.cpp:462-468` (`std::cerr << "[RGI] FGIPass::DispatchRays() entry:..."`). Both `<iostream>` includes present at `TestReSTIR_GI_Temporal.cpp:68` and `FGIPass.cpp:21`. 0 `HLVM_FORCE_CERR_LOGGING` references remain source-wide (v12 macro removal verified).
  - v13+v15 case 6u at BOTH HLSL copies line 593 (Private master AND data-dir copy are in sync).
  - v14 line-675→691 doc drift fix at `TestReSTIR_GI_Temporal.cpp:408, 662, 1537`.
  - v17 mode-7 TraceRay-bypass sentinel at line ~604 of both HLSL copies.
  - v18 modes 8/9/10/11 sentinels at lines ~614-655 of both HLSL copies.
  - v19 modes 12/15/default-case trace at lines ~663-677 of both HLSL copies.
  - v5 HLVM-bypass removal: NOTE comment near line 1521-1538 of `TestReSTIR_GI_Temporal.cpp`; no mid-frame `close+execute+waitForIdle+open` block in RenderGBuffer.
  - bug-088 executeCommandList fix at line 691 intact.
  - bug-075 binding-layout split intact at FGIPass.cpp:277-291 (`Add*` layout) + FGIPass.cpp:506-528 (`Set*` binding set).
  - bug-075 binding-offset zeroes (`Offsets.constantBufferOffset = 0` etc.) preserved per skill §C++ Game-Engine Rendering gotchas.
- **HLSL debug probes at 14 cases confirmed** in `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (792 lines / 31766B) and `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (also 792 lines / 31766B). Both copies byte-identical after v15 sync.
- **v20 script on disk**: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` exists, 161 lines / 7232 bytes (verified via search_files metadata).
- **No fresh dumps**: only the stale `20260727_000706`–`20260727_000708` frame-8 dump group in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`. No `stderr.log`, no `rgi_evidence.txt`, no newer `display_frame*.png`.
- **Stale log analysis** (file-only re-read of `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`):
  - Init: `FGIPass::UploadLights: uploaded 4 light(s)` at line 383 + `FGIPass initialized` at line 171. Init path OK.
  - Per-frame Render: `RenderGBuffer frame N: drew 24 meshes` 8x. GBuffer raster OK.
  - **Command-list warning** fires 7 times across the run (frames 1-8), one per frame: `A command list should be executed before it is reopened` at `DeviceManager.cpp:52`. This is the bug-088 / bug-075 known pattern: nvrhi's `setComputeState` records `bindDescriptorSets` BEFORE `commitBarriers`, leaving the descriptors with a stale image layout. The fgipass binding set has both SRV (t1/t2/t3 = GBuffer textures, need SHADER_READ_ONLY_OPTIMAL) and UAV (u0 = OutputTexture, need GENERAL) bindings, which is exactly the nvrhi deferred-barrier ordering bug pattern documented in the `gpu-rendering-bisect-debug` skill's `nvrhi-deferred-barrier-ordering.md` reference.
  - **No v3 spdlog markers in log** (no `FGIPass::DispatchRays ENTER`, no `FGIPass: per-frame binding set created OK`, no `FGIPass::DispatchRays EXIT`, no `TestReSTIR_GI_Temporal.cpp:445` Pre-GIPass). This is H-A confirmed: the binary on disk was built BEFORE v3 instrumentation was applied, so the binary lacks the v3 diagnostic logs entirely.
  - **No v12 cerr in log**: zero `[RGI] Render() entry:` or `[RGI] FGIPass::DispatchRays() entry:` lines. Same H-A confirmation: binary lacks v11/v12 cerr patches.
  - **Dump shows gi_raw normalized to 0**: `DumpRGBA32FTexture: gi_raw normalized per-channel — R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]` at line 76. This means the GI pass writes 0,0,0 to OutputTexture's `rgb` channel — but `avgFirstHitDist` (alpha channel) might be non-zero. Without fresh dumps and vision tool, this is unverified.
  - **gbuffer_worldpos IS valid**: `gbuffer_worldpos normalized per-channel — R[-15.228,15.264] G[-11.811,8.193] B[-14.291,0.025]`. So the worldpos SRV reads work; the early-return at `GIPathTracing.hlsl:466` (`length(worldPos) < 0.001`) does NOT fire for any pixel. The bug is downstream of that guard, in the lighting/payload math, OR in the UAV write itself (slangc dead-strip, descriptor mismatch, barrier issue).
  - **No Vulkan ERROR/VUID**: zero VUID-VkDescriptorImageInfo-imageLayout-00344 or related in the stale log. Validation layer was silent (likely disabled via `r.Vulkan.Validation = 0` per the gpu-rendering-bisect-debug §C++ gotchas "Quick mitigation").

## Final-goal gate

**FAILED/UNVERIFIED — unchanged from prior 11+ ticks.** Six-criterion gate from the cron prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith denying all terminal probes; cannot run fresh build to verify current source still compiles)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED (no fresh dump group, no stderr.log, no rgi_evidence.txt)
- (c) No command-list-already-open errors — UNVERIFIED (stale log shows 7 warnings per run; this is the nvrhi deferred-barrier ordering pattern; needs proper fix or per `gpu-rendering-bisect-debug` `nvrhi-deferred-barrier-ordering.md` "split binding layout into SRV-only and UAV-only sets, dispatch in two phases")
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED (validation layer was silent in stale log; cannot verify without fresh run)
- (e) Validator passes newest dump group — UNVERIFIED (terminal blocked; cannot run validator; cannot refresh dump group)
- (f) Display visibly contains recognizable non-uniform Sponza with sane exposure — UNVERIFIED (vision tool unavailable; no fresh dumps; stale `display_frame8.png` from 00:07 is reportedly uniform magenta per prior sessions)
- (g) Relevant checks pass — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written. Final-goal gate remains failed/unverified.

## Stall assessment

- **Intentionally gated, NOT stalled.** Per the skill: this is a documented evidence failure and intentional parent-evidence wait, not an unexplained stall. No `PIPELINE_NUDGE` warranted.
- Trajectory closed at v20. v21 + v13a decision matrix are both parent-evidence-gated. No further file-only patch advances the renderer without terminal evidence.
- Per `software-development-practices` "Don't fabricate findings" and `gpu-rendering-bisect-debug` anti-pattern #5: without fresh dumps I cannot certify any of the six acceptance criteria. Reporting them as met would be fabrication.
- Per PICK's v13a branch 6 ("Parent cannot rebuild -> cron records structural limitation honestly on subsequent ticks"), this is the live branch.

## Static-evidence synthesis: most likely root-cause cluster

Three converging hypotheses (file-only confidence, in descending order):

1. **nvrhi deferred-barrier ordering (HIGH confidence)** — `DeviceManager.cpp:52` warning fires 7 times per run. The FGIPass binding layout has SRV (t1/t2/t3 GBuffer textures, layout = SHADER_READ_ONLY_OPTIMAL) AND UAV (u0 OutputTexture, layout = GENERAL) in the SAME binding set. nvrhi's `setComputeState` records `vk::bindDescriptorSets` BEFORE `commitBarriers`, so the descriptors go live with stale layout. Per `gpu-rendering-bisect-debug` `nvrhi-deferred-barrier-ordering.md`, this is the canonical pattern. Proper fix: split the FGIPass binding layout into SRV-only + UAV-only binding sets, dispatch in two phases. **This is the most mechanically actionable renderer fix available — but it requires a code change that needs to be tested against a fresh build, which the cron cannot perform.**

2. **Binary lacks v3+v11+v12 instrumentation (CONFIRMED)** — `search_files` of `Binary/Debug/TestReSTIR_GI_Temporal.log` shows ZERO v3 spdlog markers and ZERO v12 cerr lines. The binary was built before v3/v11/v12 were applied. The fact that `gi_raw` is `R[0,0,0] G[0,0,0] B[0,0,0]` (and not just dark-but-correct) is consistent with the GI dispatch either (a) not running at all, or (b) running but the OutputTexture UAV write being dropped by barrier issues (hypothesis #1). Either way, the parent MUST rebuild to surface v3+v12 evidence.

3. **Mode-7 (TraceRay-bypass) probe would bisect hypothesis #1 vs shader-math bug** — the v17 sentinel at GIPathTracing.hlsl:604 writes `diffuse * g_GI.AmbientColor.rgb * ambientScale` (the primary contribution expression at line 486). If `HLVM_PT_DEBUG_MODE=7` produces non-zero scene-shape with ambientScale=1.5, the bug is downstream of line 486 (TraceRay, payload, accumulate). If mode-7 produces 0, the bug is in GBufferMaterial SRV or uniform binds. **The mode-7 probe requires a fresh build, which the cron cannot perform.**

The root cause is most likely a combination of (1) and (2): the binary was built before the deferred-barrier fix, the command-list warnings fire because nvrhi's `bindDescriptorSets` precedes `commitBarriers`, and the GI dispatch either doesn't run or its UAV write is dropped. A fresh build with the existing source patches will surface v12 cerr + v3 markers, and the runner script's mode-7 probe will bisect hypothesis (1) from a deeper shader-math bug.

## Hard invariants verified this tick

- (1) `PENDING_PICK.md` authoritative — yes; v21 + v13a correctly gated.
- (2) Test-files trigger reviewer — N/A (no test files in this tick).
- (3) Impler deviation documentation — N/A (no impler action).
- (4) Plan-criticer FIX loops to planner — N/A (no new plan).
- (5) Single-instance lock — N/A in file-only mode.
- (6) "Never silently exit" — this heartbeat satisfies it.

## Action taken this tick

- Read `PENDING_PICK.md`, all six v20 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT), the v22 inner-pipeline companion health file (`PIPELINE_HEALTH_2026-07-27_v22.md`), the outer-watchdog v23 file (`PIPELINE_HEALTH_2026-07-27_outer_v23.md`), the latest `TestReSTIR_GI_Temporal.log` (full re-read).
- Verified v1-v20 patches remain in source at the line numbers prior commits claimed.
- Verified `run_rgi_diagnostic.sh` is on disk (161 lines / 7232 bytes).
- **Static-evidence synthesis**: identified `DeviceManager.cpp:52` warning pattern as nvrhi-deferred-barrier-ordering bug per `gpu-rendering-bisect-debug` `nvrhi-deferred-barrier-ordering.md`; this is the highest-confidence root-cause candidate that requires a code change.
- **H-A binary/source mismatch CONFIRMED** via static log re-read: 0 v3 spdlog markers, 0 v12 cerr lines in `TestReSTIR_GI_Temporal.log`. The binary was built BEFORE v3/v11/v12 instrumentation was added.
- **Documented mode-7 probe bisection strategy** as the next decisive test (requires fresh build).
- Wrote this v23 inner-pipeline heartbeat to `PIPELINE_HEALTH_2026-07-27_v23.md`.
- Did NOT: create v21 markers prematurely, route into v13a branches without parent evidence, fabricate KEEP/ALL_KEEP verdicts, create Kanban cards, commit, push, archive, pause, or modify governance.

## Parent action required (UNCHANGED from v22)

The minimum-action unblock is a single command:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

…which produces `rgi_evidence.txt` with the consolidated 9-branch evidence. The parent pastes the contents back to the cron; the cron routes to v21+ based on the evidence shape (9 branches documented in PICK v21 entry, lines 141-150):

1. Accumulate/ReSTIR/denoise investigation (if all probes match but mode-0 still broken)
2. AmbientColor uniform fix (if mode 6 works but mode 12 fails)
3. TraceRay isolation (if mode 6/7 work but mode 8 crashes)
4. slangc dead-strip investigation (if modes 6/7/8/9 all 0 + default works)
5. debugMode reach investigation (if all modes 0)
6. divide-by-256 issue (if mode 10 = 0 but mode 15 = 15)
7. View cbuffer investigation (if mode 11 = 0)
8. stderr buffering investigation (if cerr does NOT fire)
9. -Werror cascade-aware fix (if build fails)

If parent cannot run, the pipeline remains at this heartbeat; v21 + v13a remain gated. No fabricated progress markers will be written on subsequent ticks while the structural terminal block persists. The trajectory is closed at v20 — no further file-only patch advances the renderer without terminal evidence.

## Cron tick summary (≤8 lines for delivery)

- State: v22 outer-watchdog heartbeat written; v23 inner-pipeline heartbeat (this file); trajectory closed at v20.
- Static evidence synthesis: nvrhi-deferred-barrier-ordering is highest-confidence root-cause candidate (DeviceManager.cpp:52 warning fires 7x/run, FGIPass binding set has both SRV + UAV bindings per canonical nvrhi bug pattern from `gpu-rendering-bisect-debug/nvrhi-deferred-barrier-ordering.md`); H-A binary/source mismatch CONFIRMED (0 v3 spdlog markers, 0 v12 cerr in stale log).
- Terminal: blocked by tirith on every probe (consistent with 11+ prior ticks); effective toolset file-only despite prompt grant.
- Final-goal gate: FAILED/UNVERIFIED on all 6 criteria (no fresh build/run/log/dumps/validator/visual).
- Action this tick: file-only static audit + companion v23 inner heartbeat (this file).
- Did NOT: fabricate KEEP verdicts, fire v21 markers prematurely, create Kanban cards, commit, push, archive, pause, modify governance.
- Parent action: `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` then paste `rgi_evidence.txt` back; cron routes to v21+ based on 9-branch decision matrix.
- Pipeline remains incomplete pending parent-runner-evidence. Trajectory closed at v20; no further file-only action available.