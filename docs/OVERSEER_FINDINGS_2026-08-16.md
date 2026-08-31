# Overseer Findings — 2026-08-16 (card t_7b79c010)

## Toolset issue (EC-039)
The cron session cannot run any shell commands. `terminal` is blocked
by tirith (`pending_approval: tirith:unknown`). All file tools work.
Per EC-039, this is the canonical "declared ≠ actual" failure mode; the
cron's prompt body is structurally false for shell work. Parent session
must either (a) reconfigure, (b) restructure to file-only, or (c) pause.
This finding is written to docs/OVERSEER_HEALTH_2026-08-16.md.

## New actionable evidence for the worker (handle-identity ruled out)
The 2026-08-14 22:18:56 test run log contains NEW `[handle-id]` probes
that did not exist when DIAGNOSTIC_2026-07-30.md was written. The probes
were added by the worker since the diagnostic.

The probes show that GBufferMaterial, GBufferWorldPos, and GBufferNormal
handles logged by `RenderGBuffer` (raster pass) are **byte-identical**
to the handles logged by `FGIPass::DispatchRays` (GI ray-trace pass).
This is consistent across frames 0-7. There is no frame-to-frame drift.

**Conclusion: Option 4 of the 2026-07-30 diagnostic (stale handles
from mid-frame GBuffer recreation) is RULED OUT.**

The remaining hypotheses from the 2026-07-30 diagnostic are still open:
1. slangc compiled the debug-mode switch wrong (case 20/21/22 dead-stripped)
2. Image layout transition is wrong (silent, no VUID because nvrhi
   silently fixes layout to a valid one)
3. RHI silently drops the second binding set when both SRV + UAV bound
4. ~~Stale handles~~ **RULED OUT 2026-08-14**

## Recommended next probe (worker can do this)
Add a fourth debug-mode case 23 that reads `GBufferWorldPos.Load(int3(0,0,0))`
in the raygen shader (a single specific pixel, not the working pixel) and
writes a single magenta flag if the value is non-zero. If case 23 shows
magenta, the binding works for pixel (0,0); if it shows zero, the binding
is universally broken. This is the cheapest probe that distinguishes
"per-pixel binding issue" from "global binding issue".

Also verify with `spirv-cross --reflect GIPathTracing.spv` that the SPIR-V
actually has the SRV bindings at t1/t2/t3 (anti-pattern #7 from
`gpu-rendering-bisect-debug`: slangc dead-stripping case labels).

## What the cron cannot do this tick
- Cannot run Debug build (terminal blocked)
- Cannot run TestReSTIR_GI_Temporal (terminal blocked)
- Cannot invoke HLVM_PT_DEBUG_MODE=20 to inspect gi_raw output (terminal blocked)
- Cannot run validator on newest dump group (terminal blocked)
- Cannot vision-inspect display image (vision tool not available in cron)
- Cannot call `hermes kanban` (terminal blocked)
- Cannot comment on the card (terminal blocked)
- Cannot issue KEEP/FIX/DELETE verdict (Stage 2 fully blocked)

## What the cron CAN do (and did)
- File-level evidence gathering via read_file
- This file (health log + findings)
- Document the toolset discrepancy per EC-039 protocol

## Reschedule guidance
The next tick (15-min offset) will hit the same toolset wall. The cron's
prompt body structurally cannot make progress on this card without
terminal access. The parent session should investigate before
unpausing — either reconfigure the cron's toolset (verify with a manual
`terminal command="date"` invocation FIRST per EC-039), or pause this
cron and run the verification interactively.

## AUTO_RESOLVE_DO_NOT respected
Card body has `AUTO_RESOLVE_DO_NOT: yes`. Even if the cron had all
mechanical checks available, it would refuse to auto-resolve. The
honest position is: this card needs human/worker to keep iterating
on the binding bisect; the cron can only observe.

## Reference
- Skill: kanban-cron-overseer v2.4.0
- Hard rule #7: never silently exit (this file is the output)
- EC-039: declared-vs-actual toolset discrepancy
- Edge case registry row applies to this tick
