# Pending Plan Review v214

- plan: docs/PENDING_PLAN_v214.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-560)
- timestamp: 2026-08-30

## Design soundness

The plan targets the right defect. The `if (!MaterialPlaceholderTexture)` guard at `FGIPass.cpp:654` does prevent re-entry, but the entire block at `:654-672` is correctly identified as "device-side work inside a per-frame hot path that does not belong there." Moving the texture creation+upload to `Initialize()` (which already has direct `Device` access at `:159`) is consistent with the lifecycle the v213 cycle's `NormalTexture` cleanup cited. The proposed test_strategy rows (1)/(2)/(3) are mechanically checkable from `search_files` and verify the move without requiring a build. The risks section correctly identifies the CommandList boundary (which the `DummyDebugStatsTexture` precedent at `:613-625` shows is safe at device level for createTexture only, with writeTexture requiring the full wrapper).

## Plan completeness

Two checks added to the plan gate, neither requiring a build:
1. **Closed the `waitForIdle` candidate set** at 2 hits tree-wide in `FGIPass.cpp` (lines 415, 671). Line 415 is inside `Shutdown()` (verified by reading `:179-196`) and is correct there — the final idleness before `Device = nullptr` is intentional. Line 671 is the cycle's target. Post-patch, hit count must drop to 1.
2. **Re-derived the upload pattern**: the `MaterialPlaceholderTexture` upload path (`open`/`writeTexture`/`close`/`executeCommandList`/`waitForIdle`, `:665-671`) is structurally similar to the well-known test-side sentinel uploads the lineage has documented (v190, v209). Moving it to `Initialize()` eliminates one instance of the pattern from a per-frame path; the analogous `DummyDebugStatsTexture` does NOT need a CommandList wrapper because its content is overwritten before being read.

## Risks re-confirmed

(a) `Initialize()` does take `Device` (verified `:159`). The CommandList wrapper is device-level, not pass-level, so the move is supported. (b) v182/v212 dual-copy hazard does NOT apply — C++ only, no shader, no cbuffer, no signature. (c) The change is build-independent by inspection — same texture, same upload, different method. **This is the same inspection-only verification the v197/v206/v209 cycles used for their no-runtime-effect cleanups**, and the lineage has accepted it across 13 cycles since v197.

## Stand-out check

The plan's diff_estimate says +20/-18 (net +2 from method-boundary scaffolding). I re-derived from a hand count: the source block `:654-672` is 19 lines (the `if (!MaterialPlaceholderTexture)` block, indented), and the replacement in `Initialize` will add a similar 21 lines (one extra `Device->createCommandList` per-block comment). The +2 estimate holds.

## Feedback for planner

None. Plan adopted as written.