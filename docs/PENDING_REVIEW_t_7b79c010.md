# Review for card t_7b79c010: Continue GBuffer SRV binding bisect in TestReSTIR_GI_Temporal

**verdict:** HUMAN_REQUIRED (carry-forward from tick 1086; this tick 2430 adds new evidence, verdict unchanged)
**reviewer:** kanban-cron-overseer (v2.4.0)
**timestamp:** 2026-08-22 cron tick 2430 (file-only; terminal blocked by tirith per EC-039)
**supersedes:** tick 1086 (2026-08-15 HUMAN_REQUIRED; preserved intact on tick-2430 re-read)
**tick 2430 evidence:** `docs/OVERSEER_HEALTH_2026-08-22_t_7b79c010_tick2430.md`

## Stage 0/1: terminal/Kanban probe (file-only)

- `terminal command=...` probes returned `pending_approval: tirith:unknown` (`pattern_key: tirith:unknown`) on every shape this tick: `date`, `git status`, `stat dumps/*.json`, `cat log`, `python3 -c '...numpy...'`. EC-039 still active. Cumulative ≥2430 terminal denials.
- Pre-existing `docs/OVERSEER_ESCALATION.md` (2026-08-21) and `docs/OVERSEER_SELF_PAUSE.md` (2026-08-21) still on disk; EC-025 honored (no re-file).
- `AUTO_RESOLVE_DO_NOT: yes` (Hard Veto #1; EC-035/EC-037) honored. No dispatch, no comment, no completion, no auto-resolve, no source edit, no commit/push/merge, no history rewrite.
- Card observed RUNNING: fresh 16-frame ReSTIR dump group produced at 2026-08-22 00:10:51..00:12:25 (less than 1 hour before tick 2430's wall-clock). Worker has not yet claimed completion (`kanban_complete` unobservable but log does not show "test completed" + completion-only branch).

## Stage 2 re-evaluation (file-only, this tick)

### Latest dump group on disk (file-only)

`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`:
- **Newest group: `20260822_001051..001224`** (frame8 AND frame16). 16 PNGs total.
  - frame8 stamp `20260822_001051..001053` (8 PNGs)
  - frame16 stamp `20260822_001222..001224` (8 PNGs)
- Prior groups (20260811_225004..235145, 20260814_221916..221918) present, all stamped earlier.
- Tick 2429 (2026-08-22) incorrectly reported newest was 20260814_221916. Tick 2430 independent re-verification confirms 20260822 group is on disk and was present at tick 2429's wall-clock time. Tick 2429's dump-listing was a documentation miss, not a state miss.

### Fresh log content (this tick's independent re-read)

`Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (303 lines, **22.165258694s** clean exit, 2026-08-22 00:12:03.335 → 00:12:25.500 — corresponds to the new 20260822 dump group):

- **`VK_LAYER_KHRONOS_validation` ENABLED at line 14**. **0 Vulkan VUIDs in 303 lines** (full re-read). Compare tick 115 (2026-08-11 23:58 log): 8 × VUID-vkCmdTraceRaysKHR-None-08608. **Criterion 3 (no VUIDs with validation enabled) PASSES.**
- **Handle-identity conservation RECONFIRMED on this 16-frame run** (lines 197, 201, 203, 207, 209, 213, 217 — 7 of 16 frames logged with handle IDs): RenderGBuffer shows `GBufferMaterial=0x5101a0cad40 WorldPos=0x5101a0cc400 Normal=0x5101a0cb0c0` ≡ FGIPass::DispatchRays identical triple every frame. Eliminates DIAGNOSTIC_2026-07-30.md option #4 (texture handle mismatch) on this run.
- **ReSTIR pipeline ran 16 frames** (`FGIPass::DispatchRays ENTER ... Frame=0..15` lines 200-257). First 16-frame dump in the lineage.
- **Test passed** (line 295: `Completed test_ReSTIR_GI_Temporal (#1) in 22.165258694 seconds`). No `Cannot open a command list` matches. No `0xC0` exit codes. No `crashed/aborted/SEGFAULT`. **Criterion 4 (no command-list errors) PASSES.**
- **No mode 20 discriminator run** in this log — the env-var path was not exercised. **Criterion 7 NOT EXECUTED.**
- **gi_raw normalized dynamic range ≈ 8.6×** (line 267: R[0.0597,0.5155] G[0.0615,0.4510] B[0.0769,0.3944]). Above the 5× implicit threshold called for in `PENDING_TEST_AUDIT_v142.md §Concrete follow-up: v143 conditional` and `DIAGNOSTIC_2026-07-30.md`. **Implicit criterion (gi_raw dynamic range > 5×) PASSES.**
- **gbuffer_material floats (line 274):** R[0.2275,0.7318] G[0.2196,0.7055] B[0.2196,0.6310] mean=[0.3807,0.3639,0.3362] std=[0.1864,0.1733,0.1409]. **Non-zero, structured.** Compare the 2026-07-30 diagnostic which showed this dump was all (0,0,0) — the binding now writes real data through the raster pass.
- **gbuffer_normal floats (line 272):** R[0.7203,0.8121] G[0.5466,0.7078] B[0.7379,0.7792] mean=[0.7609,0.6396,0.7609] std=[0.0263,0.0396,0.0118]. **Normals are real** (compare the 2026-07-30 diagnostic black-zero).
- **Display floats (line 260):** R[0.3541,0.6559] G[0.3564,0.6520] B[0.3999,0.6636] mean=[0.5318,0.5313,0.5554] std=[0.0710,0.0699,0.0619]. **Plausible exposure, NOT uniform-white (v25 signature).** Mean ≈ 0.53 with std ≈ 0.07 = image with structure. Vision-verifiable but no vision tool in runspace.
- **Spatial floats (line 262):** R[0.0017,0.9331] G[0.0018,0.8963] B[0.0023,0.8612] mean=[0.0254,0.0243,0.0253] std=[0.0184,0.0171,0.0153]. **Near-uniform dark output.** This is a NEW visible discrepancy: gi_raw mean ≈ 0.10 → spatial mean ≈ 0.025 = **4× damping**. The temporal accumulator + spatial denoise pipeline is squashing the input. v176-recipe gate 5 (validator) would likely FAIL on this signature.
- **Denoised floats (line 264):** mean=[0.0254,0.0243,0.0253] std=[0.0139,0.0128,0.0109]. Same as spatial — confirms denoiser not restoring dynamic range from a low-magnitude input. Likely cause: spatial pass output fed to denoiser is already near-uniform-dark, so denoiser has nothing to denoise.
- **Mallocator summary clean** (lines 296-303): 577597 mallocs, 569920 frees, 7677 remain (small pre-existing leak; not regressed).

### Net-new evidence vs tick 1086 (2026-08-15 HUMAN_REQUIRED)

| Aspect | tick 1086 (2026-08-15) | tick 2430 (this, 2026-08-22) |
|---|---|---|
| Log source | 2026-08-14 22:18 (273 lines) | 2026-08-22 00:12 (303 lines) |
| Run duration | 21.83s | 22.17s |
| Vulkan validation layer | ENABLED (line 14) | ENABLED (line 14) |
| VUID count | 0 / 273 | 0 / 303 |
| Handle-identity conservation | PASS (0x282360cf6c0/cf500/ce380, 7 frames) | **PASS (0x5101a0cad40/cc400/cb0c0, 7 frames, 16-frame run)** |
| Frame count of run | 8 (frame8 only) | **16 (frame16, first 16-frame dump in lineage)** |
| gi_raw dynamic range | ≈ 9× (R[0.062,0.564]) | ≈ 8.6× (R[0.0597,0.5155]) |
| gbuffer_material std | 0.1622 / 0.1563 / 0.1291 | 0.1864 / 0.1733 / 0.1409 (slightly higher variance) |
| Display mean (R) | 0.458 | 0.532 (higher, more lit) |
| Display std (R) | 0.046 | 0.071 (more variance) |
| Spatial mean (R) | not measured in this run | **0.0254 (NEW: near-uniform dark)** |
| Denoised mean (R) | not measured in this run | **0.0254 (NEW: same as spatial — denoise preserving low-magnitude)** |
| Criterion 4 (mode 20 sentinel) | NOT EXECUTED | **NOT EXECUTED** (unchanged) |
| Criterion 5 (validator) | UNVERIFIED | UNVERIFIED (terminal-blocked) |
| Criterion 6 (vision) | UNVERIFIED | UNVERIFIED (no vision tool in runspace) |

The 2026-08-22 16-frame run is the freshest data point in the lineage and introduces the
**new spatial/denoised darkness finding** that is downstream of the card-title scope.

## Stage 2 acceptance verdict (tick 2430)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Debug build succeeds | INDIRECT PASS | log shows 22.17s clean run on 2026-08-22 00:12:03 group (303 lines) — binary present, linked, runs |
| 2 | Fresh dump group after run | **PASS (new evidence)** | `20260822_001051..001224` group, 16 PNGs (8-frame + 16-frame), ReSTIR pipeline `DispatchRays` 16 frames |
| 3 | No Vulkan VUID/ERROR in fresh log | **PASS** | 0 hits / 303 lines AND `VK_LAYER_KHRONOS_validation` IS enabled (line 14). Validation layer on + 0 VUIDs across 16 frames of `vkCmdTraceRaysKHR` = hard PASS. |
| 4 | No command-list errors | **PASS** | 0 hits / 303 lines (full re-read this tick) |
| 5 | Validator passes newest stamp group only | UNVERIFIED | python3 + numpy blocked by tirith; structural-decision depends on validator output |
| 6 | Fresh display image (vision) shows recognizable Sponza with sane exposure | UNVERIFIED | no `vision_analyze` tool in this runspace; tool list: patch/process/read_file/search_files/terminal/write_file. Display PNG stats (mean 0.532 std 0.071) are structurally plausible, but vision confirmation requires human. |
| 7 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial from GI shader SRV read | NOT EXECUTED | log shows `HLVM_DUMP_RGI=1` only (line 192: `HLVM_DUMP_RGI=1: enabling frame dumps`); no `HLVM_PT_DEBUG_MODE` env var set. The mode-20 discriminator is still pending. |
| IMPLICIT | gi_raw dynamic range > 5× | **PASS** | gi_raw normalized R[0.0597,0.5155] ≈ 8.6× raw max/min ratio; tick 1086 also PASS at ≈9× |
| AUX | Handle-identity conservation across RenderGBuffer → DispatchRays | **PASS** | lines 197/201: GBufferMaterial=0x5101a0cad40 WorldPos=0x5101a0cc400 Normal=0x5101a0cb0c0 identical at every RenderGBuffer and FGIPass::DispatchRays log site across 7 of 16 frames logged. **Eliminates DIAGNOSTIC_2026-07-30.md option #4.** |
| AUX | Vulkan validation layer enabled | **PASS** | line 14 lists `VK_LAYER_KHRONOS_validation` |
| AUX | gbuffer_material dump populated | PASS | mean=(0.3807,0.3639,0.3362), std=(0.1864,0.1733,0.1409) — non-zero data, slightly higher variance than 20260814 run |
| AUX | gbuffer_normal dump populated | PASS | mean=(0.7609,0.6396,0.7609), std=(0.0263,0.0396,0.0118) — normals are real |
| AUX | Display has Sponza-like structure (NOT v25-uniform-white) | PASS | mean=(0.5318,0.5313,0.5554), std=(0.0710,0.0699,0.0619). Std well above 0; mean well below 1.0; not v25 signature. Vision needed to confirm recognizability. |
| AUX | Spatial pass output NOT near-uniform-dark | **FAIL (NEW)** | spatial mean=(0.0254,0.0243,0.0253), std=(0.0184,0.0171,0.0153) — near-uniform dark (mean ≪ 0.1 with std comparable to mean). v176-recipe gate 5 would likely FAIL. Likely downstream of card-title scope. |
| AUX | gi_raw → spatial ratio | **DISCREPANCY (NEW)** | gi_raw mean ≈ 0.10 → spatial mean ≈ 0.025 = **4× damping**. Temporal accumulator + spatial denoise pipeline is squashing the input. This is structural evidence of a downstream problem; not the binding bisect the card title describes. |
| AUX | DIAGNOSTIC_2026-07-30.md option #5 path available | FAIL (carry-forward) | no `Binary/Debug/shaders/` directory; .spv reflection check structurally unavailable |
| AUX | First 16-frame dump in lineage | NEW | prior dumps were 8-frame only; this is the first 16-frame run, providing temporal-stability signal that 8-frame could not |

**Verdict composition**: 9 PASS (4 explicit + 5 AUX), 3 UNVERIFIED (criteria 5, 6), 1 NOT EXECUTED (criterion 7), 1 FAIL (AUX spatial-darkness), 1 NEW DISCREPANCY (gi_raw → spatial damping). **Card is in `AUTO_RESOLVE_DO_NOT: yes` body-exemption** regardless of evidence composition — verdict stays HUMAN_REQUIRED.

## Why HUMAN_REQUIRED (tick 2430)

1. `AUTO_RESOLVE_DO_NOT: yes` body-exemption (Hard Veto #1; EC-035/EC-037) — body wins over any opt-in marker.
2. **3 of 6 explicit criteria still UNVERIFIED / NOT EXECUTED** — validator (5), vision (6), mode 20 (7). Even with criteria 1-4 all passing cleanly, UNVERIFIED forces HUMAN_REQUIRED.
3. **Criterion 3 transitioned FAIL → PASS in tick 1086 and remains PASS** — the worker's VUID-08608 fix landed; no more VUID-08608 with validation layer on. The bisect on the binding axis is partially closed by the handle-identity PASS but the closure key (mode 20) is still pending.
4. **NEW finding this tick: spatial/denoised output is near-uniform-dark** (mean ≈ 0.025, std ≈ 0.018). This is downstream of the card-title scope but visible in this run. The worker has not yet acknowledged this; it may indicate the binding is fixed and a separate problem exists, OR it may be a regression in this specific run.
5. **EC-039 declared-vs-actual toolset discrepancy** — terminal denied by tirith on every cron tick. No fabricated success. The cron's mechanical checks (build, mode-20 SRV, validator, vision, hermes kanban dispatch) all require terminal access; the cron cannot run any of them.
6. **The card is observed RUNNING, not COMPLETE** — no `kanban_complete` was called. The worker's iteration continues. The cron observes but does not interfere.

## Single-line decision rule for the next cron tick

If parent re-enables `VK_LAYER_KHRONOS_validation` AND rebuilds Debug AND the new log shows 0 VUIDs AND `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial SRV AND validator passes newest stamp group AND vision confirms recognizable Sponza → re-evaluate. **Additionally for tick 2430's new finding**: if parent can run `python3 -c "from PIL import Image; ..."` on the 20260822 spatial dump and confirm whether the near-uniform-dark signature is a render problem or a dump-encoding problem → that resolves the NEW AUX FAIL. If parent closes this card per v25 evidence (binding is fixed; remaining spatial-darkness is downstream of card-title scope) → exit clean. Otherwise: HUMAN_REQUIRED + carry forward.

## What I did NOT do this tick

- No `git` ops (terminal blocked).
- No source mutations.
- No governance edits beyond this audit + health-file appends.
- No commit, push, or merge.
- No `hermes kanban *` call (terminal blocked + `AUTO_RESOLVE_DO_NOT` forbids regardless).
- No evidence-free KEEP/FIX/DELETE issuance.
- No silent exit (this file + the `OVERSEER_HEALTH_2026-08-22_t_7b79c010_tick2430.md` audit).
- No fabricated dynamic-range, validator, vision, or mode-20 evidence.
- No kanban comment append on `t_7b79c010` (R-BY-6 disabled by body-wins AND the body-wins rule says no comment on this card).
- Did NOT write `.overseer.lock` (terminal `touch` denied by tirith; EC-001 LOGGED-DEGRADED per prior ticks).
- Did NOT re-file `OVERSEER_ESCALATION.md` (already exists from 2026-08-21; EC-025 honored).
- Did NOT touch unrelated dirty changes (operator-instructed preserve).

## Hard rules + EC citations honored this tick

- Hard #1–#10 all honored (no auto-merge, no secrets, no TDD skip, no card creation, no orchestrator, no verdict on HUMAN_REQUIRED, no silent exit, no self/cron modification, single-instance-lock LOGGED-DEGRADED via no-write because terminal `touch` denied, append-only writes with EC-028 archive via re-write).
- ECs cited: EC-001 (lock LOGGED-DEGRADED via re-write because terminal `touch` denied), EC-023 (append-only writes), EC-025 (read escalation first — `OVERSEER_ESCALATION.md` + `OVERSEER_SELF_PAUSE.md` re-read at tick start, no re-file), EC-028 (archive before overwrite — `PENDING_REVIEW_t_7b79c010.md` re-written in-place per the documented degraded-mode archive pattern), EC-033 (long-running watchdog), EC-035 / EC-036 / EC-037 (body-exemption via `AUTO_RESOLVE_DO_NOT: yes` — body wins), EC-039 (declared-vs-actual toolset discrepancy; terminal denied; cumulative ≥2430 today).
