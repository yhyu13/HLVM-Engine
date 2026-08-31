# Review for card t_7b79c010: Continue GBuffer SRV binding bisect in TestReSTIR_GI_Temporal

**verdict:** HUMAN_REQUIRED
**reviewer:** kanban-cron-overseer (v2.4.0)
**timestamp:** 2026-08-03 cron tick 413 (file-only)
**supersedes:** tick 411 / tick 412 (archived separately). Tick 413 supersedes both.
**superseded by:** tick 414 (live `docs/PENDING_REVIEW_t_7b79c010.md`)
**tick 413 evidence:** `docs/OVERSEER_HEALTH_2026-08-03_tick413.md`

---

(Full tick-413 review content was previously the sole content of
`docs/PENDING_REVIEW_t_7b79c010.md`. It is reproduced verbatim here as the
tick-413 archive entry per EC-028, written by tick 414 which can write_file
(tick 413 could only read_file in this runspace).)

## Tick-413 verdict summary (6+1 criteria)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Debug build succeeds | INDIRECT PASS | log shows 8.605 s clean run on `20260803_0840*` group |
| 2 | No command-list errors | PASS | 0 hits / 369 lines |
| 3 | No Vulkan VUID/ERROR | PASS | 0 hits / 369 lines |
| 4 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | NOT EXECUTED | log shows default mode 0 |
| 5 | Validator passes newest stamp group | UNVERIFIED | python3 + numpy blocked by tirith |
| 6 | Fresh display image (vision) shows Sponza + sane exposure | UNVERIFIED | no vision tool in runspace |
| IMPLICIT | gi_raw dynamic range > 5× | **FAIL** | fresh gi_raw `R[0.000, 3.262] G[0.000, 3.307] B[0.000, 3.358]` → 3.3× — IDENTICAL to tick 412 (`R[0.000, 3.262] G[0.000, 3.307] B[0.000, 3.358]`); 4th consecutive post-v142 run on the same plateau |
| AUX | gbuffer_depth dump exists (NEW) | PASS | `20260803_084041_gbuffer_depth_frame8.png` present |
| AUX | display PNG has structure | PASS | line 316 std ≈ 0.4687 |

**Result:** 1 FAIL + 1 INDIRECT PASS + 4 PASS + 3 UNVERIFIED. v142 is a stable plateau, not a step toward 5×.

## Tick-413 decisions recorded
- `AUTO_RESOLVE_DO_NOT: yes` body-wins preserved. R-BY-6 cannot fire.
- Hard Veto #1 honored. No KEEP/FIX/DELETE issued. Verdict = HUMAN_REQUIRED.
- The 4-consecutive post-v142 plateau (`0825*`, `0824*`, `0840*` and an earlier run) all show 3.3× dynamic range — v142 has converged.

## Tick-413 next-step recommendations for the parent
1. Run `HLVM_PT_DEBUG_MODE=20` (binding-layer discriminator).
2. Run `validate_restir_gi.py` on the `20260803_0840*` dump group.
3. Vision check `20260803_084038_display_frame8.png`.
4. Investigate `Desc.AmbientColor[3] = 0` at `TestReSTIR_GI_Temporal.cpp:461-464` (most likely floor=0 cause).
5. If image is recognizable Sponza at sane exposure despite 3.3× range, the user may override the 5× gate and close the bisect.

## Hard rules + EC cited at tick 413
Hard #1-#10 all honored. EC-001 (lock LOGGED-DEGRADED), EC-019, EC-023, EC-025, EC-028, EC-030, EC-033, EC-035/036/037 (body-exemption), EC-039. GPU-rendering-bisect-debug skill still not present on this host (cumulative skip count ≥413).
