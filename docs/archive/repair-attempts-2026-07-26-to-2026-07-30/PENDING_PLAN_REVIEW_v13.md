# Pending Plan Review v13

- plan: docs/PENDING_PLAN_v13.md
- verdict: KEEP
- reviewer: plan-criticer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Design soundness

The plan adds a new debug-mode case (6u) to the GIPathTracing.hlsl raygen's existing debug-mode ladder. The new case writes a recognizable per-pixel constant to OutputTexture that bypasses all SRV reads, TraceRay, lighting math, and ClosestHit. This is the canonical "UAV write sentinel" probe from the gpu-rendering-bisect-debug skill's seven-step playbook (Step 3: constant-sentinel reads to separate data-corruption from transport-corruption, applied here to the dispatch body's write boundary). The probe is decisive: if mode=6 shows the per-pixel constant, the dispatch body is running and the UAV write is landing; if it shows 0, the dispatch body is not reached; if it shows garbage, the UAV write is being overwritten.

The choice of `case 6u` is correct: existing cases 1u-5u are taken (albedo/normal/primary-direct/indirect/hitDist), 6u is unused, the next-in-line free mode is 6u. The per-pixel constant `(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0)` produces a recognizable gradient at 800x600: R=0..3, G=0, B=0..2. The G=0 sentinel is the key signal: any "G is not 0" result means the dispatch wrote something other than 0.

The plan correctly identifies that this is a file-only fix with no C++ side change. The debug mode is selected via `HLVM_PT_DEBUG_MODE=6` env var, which is already wired through `FGIPass.cpp:446-449` to set `g_GI.Params5[0]`. The shader recompile takes ~0.6s with slangc; the test rebuild is the cost.

## Plan completeness

The plan covers:
- (a) the precise line range to patch (verified via read_file at offset 575-599 of GIPathTracing.hlsl)
- (b) the new case statement + 9-line comment
- (c) why mode 6u is the right value (existing cases 1u-5u taken)
- (d) why the per-pixel constant is recognizable in the dump
- (e) the early-return at line 466-469 still bypasses mode 6 for background pixels — acceptable because most Sponza pixels have valid worldPos
- (f) the NaN-safety fallback at line 568-571 is overridden by mode 6 — correct because the goal of mode 6 is to confirm the dispatch reaches the Output[pixel] write at line 600
- (g) slangc dead-strip risk is correctly assessed (debugMode is data-dependent, not constant, so the case branch cannot be folded)
- (h) per-channel normalization in the dump (DumpRGBA32FTexture at line 1712) is correctly handled (R=0..3 will normalize to R=0..1, gradient still visible)

The decision matrix is grounded in the actual on-disk log evidence (00:07 v1 log: gi_raw=0, command-list warning per frame, no v3 spdlog markers). The patch is a corrective probe — not a fix — but it is the maximally-informative next step given the source/binary mismatch hypothesis from v9-v12.

The plan does not address: what to do if the binary cannot be rebuilt (the structural block is acknowledged as "cron records honestly" rather than fabricating forward progress).

## Feedback for planner (FIX only)

None. The plan is well-grounded, the patch is small and bounded, the test is decisive, and the parent action items are clear. KEEP.

The single non-blocking observation: the plan could be stronger if it added a "mode 7: hard-coded `result = float3(0,1,0)` regardless of SRV reads" test that bypasses ONLY the SRV reads but keeps the rest of the code path. This would localize the bug further (SRV vs non-SRV). But this is a v13a+ refinement, not a v13 prerequisite. The v13 patch as specified is the maximally-informative single test.
