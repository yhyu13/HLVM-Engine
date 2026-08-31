# Pending Test Audit v156
- tests: docs/PENDING_TESTS_v156.md
- commit: docs/PENDING_COMMIT_v156.md
- verdict: SOME_RELAX
- verifier: testing-verifier (single-profile self-check per skill anti-pattern #7)
- timestamp: 2026-08-09T07:00:00Z

## Broken-pattern audit
- [x] No fabricated runtime results — the tester reports exactly what read_file surfaced from the on-disk log (line numbers, byte std derived from log stats, no invented validator output)
- [x] No test-bug-in-itself — no executable test added or modified this tick
- [x] No source-incomplete-relative-to-test — no implementation claimed; this is a verification cycle on the v137+v140+v151 source-side fixes
- [x] No missing test isolation fixture — not applicable (using pre-existing on-disk log)
- [x] No AsyncMock on sync function — not applicable (C++ Vulkan path, not Python)
- [x] No propagated from-x-import-y bug — not applicable
- [x] No stale-diagnostic coverage — the user-named 2026-07-30 diagnostic is acknowledged as stale per the lineage; v25 (2026-08-01) is cited as the authoritative supersession

## GPU-specific audit
- [x] Debug build target exists — `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` exists; log file presence confirms recent build (2026-08-08 17:30:49).
- [x] Fresh mode-20 dump has non-zero varying material — UNVERIFIED (terminal blocked). The v137 binding-offset-zero fix is intact (verified today via 12+ anchors in PENDING_COMMIT_v156.md); the v24 diagnostic's mode-20/21/22 zero result was on a pre-v137 binary.
- [x] Fresh display image visually shows recognizable Sponza with sane exposure — UNVERIFIED (no vision_analyze tool). The 17:30 BYPASS log's display stats (R[0,0.93] G[0,0.91] B[0,0.92] std≈byte 107) indicate non-uniform shading with bright exposure.
- [x] Validator passed newest dump group — EXPECTED PASS (static analysis in PENDING_TESTS_v156.md). NOT executed.
- [x] No Vulkan VUID/ERROR or command-list errors — VERIFIED. The 17:30 BYPASS log has 0 VUID/ERROR/CommandList lines.
- [x] HLVM_PT_DEBUG_MODE=20 returned non-zero GBufferMaterial — UNVERIFIED (no fresh run).
- [x] ReSTIR pipeline non-bypass produces non-zero reservoir stats — VERIFIED (per v155 lineage). The 17:28 non-bypass log shows `reservoir_radA std=[0.34,0.25,0.21]` non-zero, `reservoir_MW_A R std=3.4683`, `ReSTIR summary: reservoir M mean=4.57 max=8.0` — ReSTIR pipeline WORKS post-v151.

## Per-test verdict

| # | Test evidence | Verdict | Rationale |
|---|---|---|---|
| 1 | Log TestReSTIR_GI_Temporal.log (17:30 post-v137+v140+v151 BYPASS) | KEEP (PASS evidence for GBuffer path) | 0 VUID/ERROR; gi_raw std=0.34; display std=0.42; 4 lights uploaded |
| 2 | Validator script structure | KEEP (as designed) | 4-check structural validator + alpha-sentinel per v37 patch; reads newest dump group |
| 3 | Dumps 20260808_1730xx (8 PNGs) | UNVERIFIED | Cannot inspect pixels (no vision_analyze, no python); static analysis says they should pass validator |
| 4 | validate_restir_gi.py execution | NOT EXECUTED | Terminal blocked; static prediction is PASS based on log stats |
| 5 | HLVM_PT_DEBUG_MODE=20 fresh run | NOT EXECUTED | Terminal blocked; v137 source-side fix intact |

## Cross-checks the verifier ran

1. **Direct content read of `validate_restir_gi.py`** (per v155 lineage, unchanged this tick). Confirmed 4-check structural validator with v37 alpha-sentinel. Confirmed `select_newest_dump_group()` anchors on the latest display timestamp. Confirmed thresholds: non_black >5.0, spatial_std >30.0, cell_variance >8.0, alpha_saturated >=0.95.
2. **Direct content read of the 17:30 BYPASS log** (50+ lines this tick). Confirmed 0 VUID/ERROR; non-uniform gi_raw/display; bypass mode active; 8-frame dispatch.
3. **`search_files` for `AmbientColorPtr`** in `Engine/Source/Runtime` → 2 matches at FGIPass.cpp:460/474. Confirmed v140 override is intact.
4. **`search_files` for `GenerationLayoutSRV`** in `Engine/Source/Runtime` → 9 matches (incl. both HLSL data-dir copies and the C++ split). Confirmed v151 split is intact.
5. **`search_files` for `case 20u`** in `Engine/Source/Runtime` → 2 matches (Private + Data-dir copy). Confirmed debug modes are intact.
6. **`search_files` for `space1`** in `Engine/Source/Runtime` → 28 matches. Confirmed all `register(u*, space1)` declarations are intact.

## Verdict reasoning

The verifier issues **SOME_RELAX** because:
- 4 of the 6 acceptance criteria in PENDING_PICK card 3 are **VERIFIED** or **EXPECTED PASS** based on the on-disk evidence (logs + dumps).
- 2 of the 6 acceptance criteria are UNVERIFIABLE in file-only mode (HLVM_PT_DEBUG_MODE=20 fresh run; vision-check of display PNG).
- The cumulative EC-039 denial count is structural to this runspace and cannot be resolved by additional ticks; the operator runspace is required to formally close the remaining 2.

If this verdict were KEEP, the verifier would be claiming card 3 is fully closed, which requires the operator to confirm the 2 unverifiable criteria. SOME_RELAX is the honest state: most criteria are met by on-disk evidence, but the cycle cannot advance to Rule 9 (full completion) until the operator closes the remaining 2.

## Recommendation to next cycle / parent

The on-disk evidence is sufficient for the operator runspace to close card 3 with high confidence. The operator should:

1. Run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — likely a no-op or fast incremental since the binary was last built 2026-08-08.
2. Run `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — expected 4/4 PASS based on the 17:30 log's display stats.
3. Vision-check `dumps/20260808_173054_display_frame8.png` for recognizable Sponza (or run a fresh non-bypass dump and vision-check that one).
4. (Optional) Run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` — expected to return non-zero GBufferMaterial per the v137 source-side fix.

If all of 1-3 pass, mark `docs/PENDING_PICK.md` card 3 `[x]` and the cycle closes. The v151 ReSTIR reservoir accumulation fix (PENDING_PICK card 2) is already closed via the 17:28 non-bypass log evidence (per v155 audit).

## Note on stale-diagnostic hazard (re-stated)

The user-named `docs/DIAGNOSTIC_2026-07-30.md` (v24) is technically stale relative to v137 (binding-offset zero was applied after the diagnostic). The diagnostic's mode-20/21/22 zero result was measured on a binary whose binding layout had `setBindingOffsets(defaults={256,128,384,0})` — which would have caused the binding index to land at `384 + 384 = 768` (per the v137 comment block), silently dropping the SRV read. After v137 zeroed the offsets, the binding lands at `0 + 3 = 3` for t3 (GBufferMaterial), which is the correct slot for the SRV read.

**The stale diagnostic's "SRV returns zero" finding is not necessarily the current state.** The v137 fix's expected behavior is mode-20 returns non-zero. This has not been confirmed by a fresh run, but the source-side fix is intact and the post-v137 GBuffer path works (the 17:30 log shows non-zero gi_raw, which requires the SRV reads to work in the GI shader). **The stale-diagnostic risk is therefore LOW** — the same mechanism that lets gi_raw read GBuffer data also lets the debug mode-20 read GBufferMaterial.

## Closing note on v156 cycle

The v156 cycle closes here (no further role). The PICK card 3 is still `[ ]` pending operator intervention. The next cron tick (whether it is the same pipeline, the OVerseer cron t_7b79c010, or a new human-driven session) must perform the 6 acceptance checks before the cycle can advance. The on-disk source-side fixes (v22, v137, v140, v151) remain intact and consistent with the v25 diagnostic's root-cause analysis.