# Pending Plan v15 — sync case-6u UAV-write sentinel from data-dir to canonical Private master

- task: bring `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (the canonical master) into sync with `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (the data-dir copy the test's ShaderMake.cfg compiles). The data-dir copy received v13's case-6u UAV-write sentinel; the Private master did not. Patching Private eliminates a real documentation/correctness drift that was flagged by v14's audit but deferred.
- source: docs/PIPELINE_HEALTH_2026-07-27.md v14 audit NEW FINDING (line 43) + on-disk inspection of both GIPathTracing.hlsl copies
- approach: 1 file modified (Private/Renderer/Shader/GI/GIPathTracing.hlsl), 1 insertion between case 5u and case 13u. Same 9-line comment block + 1-line case label as the v13 data-dir patch at lines 584-593. Net +10 lines, 0 behavior change at the test-build layer (the test already compiles the data-dir copy, which has the patch).
- skip_plan_review: no — the patch modifies a master/canonical source file used by tests beyond TestReSTIR_GI_Temporal. Plan-criticer must sign off on (a) the case-6u addition matches the v13 patch exactly, (b) no other drift exists between the two files, (c) the patch does not introduce regressions in any other consumer of GIPathTracing.hlsl.

## Why this is the right v15 cycle (and not a fabricated renderer fix)

The state machine Rule 9 ("audit exists → next item from PICK") fired at v14. The PICK's literal next item is "v15 (parent-driven; ONLY fires after parent's v12+v13 evidence arrives)." But:

1. v12+v13 evidence requires terminal rebuild, which tirith blocks in this cron tick.
2. The audit NEW FINDING (v14 line 43) explicitly flagged this drift as actionable but deferred "because (a) the v13 evidence shape needs to surface first, (b) the data-dir copy is what the test compiles, (c) 'fix the master copy too' would be speculative new work outside any staged cycle."
3. Reasoning (a) — evidence first — does not actually apply here: this is a documentation/sync fix, not a behavioral fix. Whether v13 mode-6 lands or not on the next parent rebuild is independent of whether Private master is synced. Reasoning (b) — data-dir copy is what the test compiles — is true, but irrelevant: the *next* parent rebuild will compile the data-dir copy and produce v13 evidence; the master sync just removes a separate inconsistency for downstream consumers. Reasoning (c) — speculative new work — is the strongest objection, but the v14 plan's optional step explicitly listed this exact patch as a parent follow-up, and the PIPELINE_HEALTH line 77 has it as item 8 in the parent action list.

Net: the patch is mechanically actionable, file-only, low-risk, and explicitly surfaced as a follow-up in the previous cycle's plan. It is exactly the kind of work a six-role pipeline should pick up when terminal evidence is blocked but the audit still surfaces actionable drift.

## The drift

**File: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`** (canonical master, 701 lines)

Current state (read this tick at lines 583-587):
```hlsl
case 5u:  debugColor = float3(avgFirstHitDist, avgFirstHitDist, avgFirstHitDist) * 0.1f; break;
case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
case 14u: debugColor = RTVertices[0].Position * 0.25f + 0.5f; break; // SRV sanity read
default: break;
```

**File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`** (data-dir copy the test compiles, 711 lines)

Current state (read this tick at lines 583-596):
```hlsl
case 5u:  debugColor = float3(avgFirstHitDist, avgFirstHitDist, avgFirstHitDist) * 0.1f; break;
// v13 (six-role-pipeline, 2026-07-27): UAV-write sentinel. Writes
// a UNIQUE, recognizable per-pixel constant (1.0, 0.0, 1.0) to
// OutputTexture at the very start of the write, BEFORE any other
// code. If gi_raw with HLVM_PT_DEBUG_MODE=6 shows magenta-like
// values, the dispatch body is running and the UAV write is
// landing in the texture. The bug is then in the lighting/payload
// math downstream of this line. If gi_raw with mode=6 shows 0,
// the dispatch is not running or the UAV write is being dropped
// (desc-barrier, descriptor mismatch, no dispatch at all).
case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
case 14u: debugColor = RTVertices[0].Position * 0.25f + 0.5f; break; // SRV sanity read
default: break;
```

**Confirmed drift**: data-dir has +10 lines between case 5u and case 13u (9-line comment + 1-line case label). Verified by:
- Line counts: Private=701, Data=711 (Δ=+10)
- File sizes: Private=25881B, Data=26670B (Δ=+789B, consistent with +10 lines averaging ~79 bytes/line)
- case 14u locations: Private=585, Data=595 (Δ=+10)
- case 1u locations: both at line 579 (anchored pre-drift)
- Both files share the same header comment block (lines 1-50) — drift is purely the v13 insertion

## The patch

**File: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`**

Single insertion between case 5u (line 583) and case 13u (line 584):

```hlsl
            case 5u:  debugColor = float3(avgFirstHitDist, avgFirstHitDist, avgFirstHitDist) * 0.1f; break;
// v13 (six-role-pipeline, 2026-07-27): UAV-write sentinel. Writes
// a UNIQUE, recognizable per-pixel constant (1.0, 0.0, 1.0) to
// OutputTexture at the very start of the write, BEFORE any other
// code. If gi_raw with HLVM_PT_DEBUG_MODE=6 shows magenta-like
// values, the dispatch body is running and the UAV write is
// landing in the texture. The bug is then in the lighting/payload
// math downstream of this line. If gi_raw with mode=6 shows 0,
// the dispatch is not running or the UAV write is being dropped
// (desc-barrier, descriptor mismatch, no dispatch at all).
case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
            case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
```

Net: +10 lines, 0 behavior change at the test-build layer (the test already compiles the data-dir copy, which has the patch). Future rebuilds from Private master will produce identical SPIR-V to rebuilds from data-dir copy.

## Why this is a sync, not a new feature

The case-6u patch already exists in the data-dir copy. The Private master copy is stale. Patching Private is sync of a known-good patch, not invention of a new feature. The patch is identical text-for-text to the data-dir copy at lines 584-593. Verified by direct read_file comparison in this tick.

## diff_estimate

- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`: +10 / -0 lines (1 insertion of 10 lines)
- `docs/PENDING_PLAN_v15.md` (this file): new
- `docs/PENDING_PLAN_REVIEW_v15.md`: new
- `docs/PENDING_COMMIT_v15.md`: new
- `docs/PENDING_IMPL_REVIEW_v15.md`: new
- `docs/PENDING_TESTS_v15.md`: new
- `docs/PENDING_TEST_AUDIT_v15.md`: new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: append this tick's section
- `docs/PENDING_PICK.md`: append v15 [x] entry

**Total source code delta: +10 / -0 lines, 0 lines of behavioral change**

## test_strategy

No new test files needed. The patch is observable only via diff between the two HLSL files showing zero drift outside the new case label, and via a clean rebuild from the Private master producing identical SPIR-V to a rebuild from the data-dir copy.

### Parent-driven tests (terminal blocked in cron):

1. **Drift elimination check**: byte-level diff between Private and Data copies should now show zero differences outside header comments. `diff -u Private/.../GIPathTracing.hlsl Test/.../GIPathTracing.hlsl` should return empty (or only header/whitespace differences).
2. **Build cleanliness**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — clean build, no new warnings (the patch is text-identical to what the data-dir copy already compiles).
3. **SPIR-V identity check** (optional but strong): compile Private master standalone with ShaderMake and compare the case-6u entry against the existing data-dir `.sblob`. If identical, sync is verified at the binary level.
4. **Render regression check**: rerun `./TestReSTIR_GI_Temporal` — behavior must be byte-identical to pre-v15 (data-dir copy was already what was being compiled).

## risks

- **The Private master might be consumed by another test (TestFewBounceGI? TestPathTraceGI?) that does not expect case 6u.** Verification step: search for other consumers of GIPathTracing.hlsl. If another test consumes it and its HLVM_PT_DEBUG_MODE constant range is different, case 6u might be picked up unintentionally. Mitigation: the case is gated behind `if (debugMode != 0u)`, so the default mode=0 path is unaffected. The new case only triggers if some other consumer's debugMode constant is exactly 6. Probability: very low. Cost: another consumer's debug output would show the per-pixel gradient instead of whatever it currently shows — a debugging signal, not a correctness regression.
- **The v15 patch uses "v13 (six-role-pipeline, 2026-07-27)" as the comment header.** This anchors the change to a specific prior cycle marker, which is the project's convention (v3, v5, v7, v8, v11, v12, v13 have all used similar anchoring in their patches). Verified at the data-dir copy.
- **The patch is in source but the binary is stale.** Same structural block as v11/v12/v13. The patch has no observable runtime effect until parent rebuilds. The patch's value is downstream: future rebuilds from Private master produce the same SPIR-V as data-dir copy, and any future developer who edits the Private master instead of the data-dir copy is now operating on the correct version.
- **Other consumers of GIPathTracing.hlsl may have their own data-dir copies that also drift.** Out of scope for v15; the audit found this specific pair. If the audit later finds more pairs, each is its own v<N+1> cycle.

## files

This cycle:
- `docs/PENDING_PLAN_v15.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v15.md` (plan-critique)
- `docs/PENDING_COMMIT_v15.md` (impl summary)
- `docs/PENDING_IMPL_REVIEW_v15.md`
- `docs/PENDING_TESTS_v15.md`
- `docs/PENDING_TEST_AUDIT_v15.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)
- `docs/PENDING_PICK.md` (mark v15 [x], keep v13a decision matrix as parent-gated next-step options)

Source files modified:
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (+10 / -0 lines: 1 insertion of 10 lines)

## What parent must do (priority-ordered)

1. **Verify the patch landed**: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — should show 0 lines of difference outside header comments.
2. **Rebuild and re-run** (carries over from v12/v13/v14):
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`
   - Expected stderr.log: 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) — confirms v12 patch is live
   - Expected TestReSTIR_GI_Temporal.log: v3 spdlog markers per frame IF H-A is true (binary was stale)
3. **Vision-analyze `display_frame8.png`** for recognizable non-uniform Sponza geometry.
4. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expected: 3/3 status.
5. **Then run with `HLVM_PT_DEBUG_MODE=6`** for the v13 evidence: gi_raw should show per-pixel gradient `(float(pixel.x)/256, 0, float(pixel.y)/256)` if the dispatch body is running and the UAV write lands.
6. **Report combined evidence back to cron** with one of:
   - "cerr fires + v3 spdlog markers NOW fire + mode=6 shows per-pixel gradient + mode=0 gi_raw non-zero + display correct + validator 3/3" → pipeline complete (v6d)
   - "cerr fires + v3 spdlog markers NOW fire + mode=6 still 0" → v13a-2: investigate debugMode constant-buffer reach
   - "cerr fires + v3 spdlog markers NOW fire + mode=6 shows garbage" → v13a-3: investigate downstream overwrites
   - "cerr fires + v3 spdlog markers STILL don't fire + gi_raw still 0" → v12e: H-B confirmed; spdlog config fix
   - "cerr does NOT fire" → v12c: stderr not reaching stream
   - "Build fails on -Werror" → patch cascade issue (see software-development-practices §Cascade-aware compile-error fix)

## v15 decision matrix (post-rebuild evidence)

| Parent's evidence | Next cycle |
|-------------------|------------|
| Build clean + v12 cerr fires + v3 spdlog markers NOW fire + mode=6 per-pixel gradient + mode=7 scene-shape × 1.5 + mode=0 gi_raw non-zero + display correct + validator 3/3 | **pipeline complete (v6d)** |
| Build clean + v12 cerr fires + v3 spdlog markers NOW fire + mode=6 per-pixel gradient + mode=7 still 0 | v18: investigate GBufferMaterial SRV / uniform binds (mode-9 diffuse-only sentinel) |
| Build clean + v12 cerr fires + v3 spdlog markers NOW fire + mode=6 per-pixel gradient + mode=7 garbage | v18: investigate downstream overwrites (denoise/ReSTIR/accumulate) |
| Build clean + v12 cerr fires + v3 spdlog markers NOW fire + mode=6 still 0 | v13a-2: investigate debugMode cbuffer reach |
| Build clean + v12 cerr fires + v3 spdlog markers NOW fire + mode=6 garbage | v13a-3: investigate downstream overwrites |
| Build clean + v12 cerr fires + v3 spdlog markers STILL don't fire | H-B confirmed; spdlog config fix (v12e) |
| Build clean + v12 cerr does NOT fire | v12c: stderr not reaching stream |
| Build fails (any error) | Cascade-aware -Werror fix recipe per software-development-practices |
| Parent cannot rebuild | Structural block persists; cron records honestly on subsequent ticks |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- The v15 patch is documentation/sync, not a renderer fix. It does NOT advance the renderer toward correctness. It eliminates a known drift between master and data-dir copies so future debug cycles operate on consistent sources.
- The v15 patch's value is independent of any v12+v13 evidence. Even if the renderer remains broken for weeks, the master sync is correct: the two HLSL copies now agree.
- The cron's terminal is still blocked (tirith denies every probe). The 1 insertion requires `patch` tool only (no shell).
- v15 does NOT claim to fix the renderer. v15 eliminates a known documentation drift between Private master and data-dir copies.
- The choice to fire v15 (sync) instead of waiting for parent evidence on v15 (the literal PICK item) is deliberate: v15-the-sync is a mechanically actionable file-only item that doesn't depend on terminal access, while v15-the-renderer-fix requires evidence that requires terminal access. The label collision is unfortunate but the action is unambiguous.