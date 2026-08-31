# Pending Plan v42 — structural standby + cumulative-patch audit (no source-code change)

## State-machine routing decision

- Read every `docs/PENDING_*.md` marker. v41 cycle is complete: `PENDING_PLAN_v41.md`, `PENDING_PLAN_REVIEW_v41.md`, `PENDING_COMMIT_v41.md`, `PENDING_IMPL_REVIEW_v41.md`, `PENDING_TESTS_v41.md`, `PENDING_TEST_AUDIT_v41.md` all present; final verdict `ALL_KEEP`. v41 modified `Engine/Source/Runtime/Private/Image/FImageDump.cpp` (+9/-1 lines) to read source alpha instead of hardcoding 255.
- Rule 9 (audit exists → next item from PICK) fires. Topmost unchecked item in `PENDING_PICK.md` is `v42 (parent-evidence-gated; ONLY fires after parent rebuilds with the v41 patch and reports the alpha-channel evidence shape from display_frame8.png)`. v42 is a decision matrix, not a mechanically actionable fix.
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 30+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset is file-only.
- Cron's prompt explicitly authorizes: "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop."
- Decision: file-only diagnostic-surface space is exhausted after v41. v42 is a structural standby tick that audits all v3-v41 patches at their documented sites, emits the canonical parent-triage recipe, and explicitly states that the file-only work space is exhausted. 0 source-code lines modified this tick.

## Why no v43 candidate exists

After v41, the diagnostic surface has 5 independent signals all wired to GPU-pipeline state:

| Signal | Source | Verification tool | Effective when |
|--------|--------|-------------------|----------------|
| v12 cerr per-frame (Render + FGIPass + WriteConstants) | FGIPass.cpp:487/503 + TestReSTIR_GI_Temporal.cpp:384 | `cat stderr.log` | Default ON |
| v22 binding-layout split (SRV vs UAV) | FGIPass.h:106 + FGIPass.cpp:183/311/612 + FRayTracingPipeline.cpp:357/361 | Build + run + log absence of "previous known layout" VUID | Requires rebuild |
| v28 alpha-channel sentinel | GIPathTracing.hlsl:694 (both copies) | dump_pixelstats.py + validate_restir_gi.py (v37/v40) | Requires rebuild + v41 encoder fix |
| v38 cerr DebugMode value | FGIPass.cpp:487 | `python3 decode_v38_evidence.py --cerr-file stderr.log` | Default ON |
| v13-v19 case 6u-15u sentinels | GIPathTracing.hlsl:593+ (both copies) | Run with `HLVM_PT_DEBUG_MODE=N` + vision | Requires rebuild |

All 5 are wired. v41 fixed the data flow (encoder now writes source alpha). v37/v40 made the alpha signal inspectable. v39 made the cerr signal decodable. There is no remaining file-only fix that advances the renderer without terminal access for build+run+dump inspection.

The cron's prompt states: "do not silently stop." This tick satisfies that by writing v42 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT + health append) and explicitly stating the file-only work space is exhausted. The pipeline remains in a parent-evidence-gated standby.

## Cumulative patch inventory (verified INTACT this tick via search_files + read_file)

| Patch | Site | Status |
|-------|------|--------|
| v3 spdlog diagnostic markers | FGIPass.cpp:445/473/555/568 + TestReSTIR_GI_Temporal.cpp:445 | INTACT |
| v5 HLVM-bypass removal | TestReSTIR_GI_Temporal.cpp:1521-1538 NOTE comment | INTACT |
| v7/v8/v14 doc drift cleanups | TestReSTIR_GI_Temporal.cpp:408/662/650-672/1537 | INTACT |
| v11/v12 cerr default-ON | TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487/503 | INTACT |
| v13 case 6u | GIPathTracing.hlsl:593 (BOTH copies verified) | INTACT |
| v15 Private sync | Private copy has case 6u (was missing per v15; synced per v15 itself) | INTACT |
| v17 case 7u | GIPathTracing.hlsl:604 (BOTH copies verified) | INTACT |
| v18 cases 8u/9u/10u/11u | GIPathTracing.hlsl (BOTH copies verified) | INTACT |
| v19 cases 12u/15u + default trace | GIPathTracing.hlsl (BOTH copies verified) | INTACT |
| v22 binding-layout split | FGIPass.h:106 + FGIPass.cpp:183/281/296/311/612 + FRayTracingPipeline.cpp:357/361 | INTACT |
| v23 dump-rotation | run_rgi_diagnostic.sh:126 (archive-after-run pattern) | INTACT |
| v24 dump_pixelstats.py | helper script present | INTACT |
| v28 alpha-channel sentinel | GIPathTracing.hlsl:694 (BOTH copies verified) | INTACT |
| v32 fresh-evidence-scan.sh | helper script present | INTACT |
| v37 validator alpha-check | validate_restir_gi.py:134 | INTACT |
| v38 cerr-write DebugMode value | FGIPass.cpp:487-491 | INTACT |
| v39 decode_v38_evidence.py | helper script present | INTACT |
| v40 dump_pixelstats alpha-stats | dump_pixelstats.py alpha block | INTACT |
| v41 encoder alpha preservation | FImageDump.cpp:19 (comment) + FImageDump.cpp:27 (code) | INTACT |
| bug-088 executeCommandList fix | TestReSTIR_GI_Temporal.cpp:691 | INTACT |
| bug-075 binding-layout split | FGIPass.cpp:183/311 + FRayTracingPipeline.cpp:357/361 | INTACT |

21 patches verified INTACT. 0 patches regressed.

## Files produced this tick

- `docs/PENDING_PLAN_v42.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v42.md` (new)
- `docs/PENDING_COMMIT_v42.md` (new)
- `docs/PENDING_IMPL_REVIEW_v42.md` (new)
- `docs/PENDING_TESTS_v42.md` (new)
- `docs/PENDING_TEST_AUDIT_v42.md` (new)
- `docs/PENDING_PICK.md` (modified — v41 marked [x], v42 staged as parent-evidence-gated)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v42 tick section)

## skip_plan_review: no
- This is a structural standby tick that explicitly states file-only work space is exhausted. Full audit trail preserved.

## produces_test_files: no
- Documentation-only tick. HARD INVARIANT #2 does NOT fire.

## skip_impl_review: no
- Pipeline invariants require full audit trail even for standby ticks.

## Test strategy

1. **Static tests (this tick, file-only)**:
   - 21/21 patches verified INTACT via search_files + read_file
   - All 6 PENDING_*_v42.md markers written with KEEP/ALL_KEEP verdicts
   - PICK updated; HEALTH appended
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent runs rebuild + `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`
   - Parent runs v39 decoder on stderr.log → structured verdict
   - Parent runs v40 helper on dump group → alpha verdict
   - Parent runs validator → 3/3 status
   - Parent vision-analyzes display_frame8.png → Sponza visibility verdict
   - Reports combined evidence back; cron routes via v42's 6-branch decision matrix

## Risks
- None for the file-only work itself (0 source-code changes). The pipeline acceptance criteria depend entirely on parent terminal access which is blocked by tirith.

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written. After v42, the file-only work space is structurally exhausted; subsequent cron ticks without parent terminal access will be identical-standby markers documenting the persistent terminal block.

## Canonical parent-triage recipe (consolidated)

After rebuild + run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`:

```bash
# 1. Verify v12 cerr fires (should see 8 Render + 8 FGIPass + 8 WriteConstants lines per 8-frame run)
grep -c '\[RGI\] Render() entry' stderr.log          # expect 8
grep -c '\[RGI\] FGIPass::DispatchRays() entry' stderr.log  # expect 8
grep -c '\[RGI\] FGIPass::WriteConstants' stderr.log # expect 8

# 2. Decode cerr text to structured verdict
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log
#   -> emits one of: GO / FIX_ATOI / FIX_DOCS / FIX_CVAR / NO_CERR / MIXED / UNRECOGNIZED

# 3. Inspect alpha channel via fast-first-look helper
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py
#   -> per-frame A: stats line + [v40-alpha] verdict (saturated / zero / mixed / low)

# 4. Run validator on newest dump group
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
#   -> 3/3 status (or specific failure mode)

# 5. Inspect mode-6 UAV-write sentinel
HLVM_PT_DEBUG_MODE=6 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>/dev/null
#   -> gi_raw should show per-pixel gradient (float(pixel.x)/256, 0, float(pixel.y)/256)

# 6. Vision-analyze display_frame8.png for Sponza geometry
#   -> recognizable non-uniform geometry + sane exposure
```

## Decision matrix (post-parent-rebuild)

| Evidence shape | Cron route |
|----------------|------------|
| cerr fires + mode-6 gradient + alpha saturated + validator 3/3 + display correct | **PIPELINE_GOAL_DONE** |
| cerr fires + gi_raw still 0 (mode-6 fails) | v22 binding-layout split insufficient; investigate downstream chain |
| cerr fires + v3 spdlog markers don't fire | spdlog config issue |
| cerr doesn't fire at all | stderr not reaching captured stream |
| Build fails | cascade-aware -Werror fix recipe |
| Parent cannot rebuild | identical-standby tick; pipeline remains gated |

## Recommendation

- KEEP. v42 cycle complete as a structural standby tick. File-only work space is exhausted. Pipeline remains parent-evidence-gated pending terminal access for build+run+dump inspection.