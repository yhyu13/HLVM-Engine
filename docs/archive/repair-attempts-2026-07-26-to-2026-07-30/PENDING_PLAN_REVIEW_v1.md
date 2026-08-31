# Pending Plan Review v1

- plan: docs/PENDING_PLAN_v1.md
- verdict: KEEP
- reviewer: planner+impler (single-head autonomous cron — see software-development-practices §"Full auto" anti-pattern #7 caveat; gating is mechanical evidence not independent judgment)
- timestamp: 2026-07-26T23:50:00Z

## Design soundness
The plan correctly diagnoses the two specific bugs from the fresh log evidence and applies the documented proper fixes from the gpu-rendering-bisect-debug skill:
- bug-088: per-frame `CommandList` left open → `DeviceManager::EndFrame()` immediate-CL collides with it. One-line close at end of `Render()` is the minimum fix.
- bug-075: SRV+UAV ping-pong in `TemporalLayout` → VUID-VkDescriptorImageInfo-imageLayout-00344. Splitting into SRV-only + UAV-only layouts and dispatching in two phases is exactly the recipe in `software-development-practices §C++ Game-Engine Rendering (nvrhi / Vulkan) Gotchas — nvrhi's auto-barrier ordering is fragile`. The fix moves work into nvrhi's barrier-friendly path (separate layouts = no SRV+UAV binding ambiguity per dispatch = single `requireTextureState` per binding).

The validator tightening is necessary because the current check passes against the (255,255,255) gray frame, which is exactly the "stale-lighting-fake-ambient" failure mode the bisect skill warns about.

## Plan completeness
- Missing files: none — every changed file is listed.
- Missing edge cases: the "Pipeline layout composition order" risk is flagged. Implementation should declare `TemporalLayoutSRV` first.
- Missing acceptance criteria: covered by the validator tightening + log-error scan.

## Feedback for planner (FIX only)
None — design accepted as-is.