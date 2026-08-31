# Per-card comment block for t_7b79c010 (kanban-cron-overseer)

> The cron cannot post this to the kanban card directly (terminal is
> tirith-blocked on this host — see `docs/OVERSEER_ESCALATION.md`
> EC-039). The parent session should paste the body of this file as
> a comment on card `t_7b79c010`.

---

**OVERSEER (kanban-cron-overseer) — tick 2026-08-12**

Shell-blocked tick (EC-039). Cannot run the build, the test, or the
validator; cannot `hermes kanban show`; cannot `git status`; cannot
`ls` the dumps directory. **All file-only** — read existing files,
write audit markers, exit clean per Hard rule #7.

Two findings from the existing files that the worker should know
about, because they change the bisect:

**1. Hypothesis #4 (stale texture handles) is RULED OUT.**
`Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (the
freshest log on disk, dated 2026-08-11 23:57-23:58) shows identical
handle IDs across `RenderGBuffer` and `FGIPass::DispatchRays` on
every frame:

  GBufferMaterial=0x2eaca0c8a40
  WorldPos=0x2eaca0c8880
  Normal=0x2eaca0c9680
  CommandList=0x2eacb5e1400
  OutputTex=0x2eaca0ca800

So the textures the GI shader sees ARE the same ones the raster
pass wrote to. Cross this off the list in `docs/DIAGNOSTIC_2026-07-30.md`.

**2. VUID-vkCmdTraceRaysKHR-None-08608 fires on every frame.**
8 occurrences on frames 0-7. The RT pipeline (handle
`0x84e20c0000000209`) does not declare
VK_DYNAMIC_STATE_VIEWPORT|VK_DYNAMIC_STATE_SCISSOR but the dispatch
calls dynamic viewport/scissor commands. The diagnostic's claim
"the latest log has no VUIDs at all" does not match the current
log — **this VUID was either absent before and is now firing, or
was missed in the diagnostic sweep**. Either way: this VUID is a
genuine new failure that the worker should address before claiming
acceptance.

Two additional notes:

- The 2026-08-11 log has no `HLVM_DUMP_RGI=1` or
  `HLVM_PT_DEBUG_MODE=20/21/22` runs. None of the diagnostic's
  `gi_raw_frame8.png`, `gbuffer_material_frame8.png`, etc. are
  verifiable from this log. The dumps dir is unreachable from cron
  (no `ls`).
- `stats gi_raw floats` for frame 8 shows mean ≈ (0.137, 0.137,
  0.152), std ≈ 0.043 — **non-zero, real-looking output**. This
  contradicts the diagnostic's "solid black" for mode 20 IF both
  were sampled from the same run. They may not have been.

**Acceptance criteria per the per-card protocol:**

| Criterion | Status |
|---|---|
| Debug build | NOT VERIFIED (terminal blocked) |
| No command-list errors | **FAIL — VUID-vkCmdTraceRaysKHR-None-08608 × 8** |
| No Vulkan VUID/ERROR in fresh log | **FAIL** |
| HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial SRV | NOT VERIFIED |
| Validator passes newest stamp group | NOT VERIFIED |
| Fresh display image shows recognizable Sponza | NOT VERIFIED |
| Worker claimed completion | UNKNOWN (kanban unreachable) |

**Verdict: NOT auto-resolved.** `AUTO_RESOLVE_DO_NOT: yes` on the
card body is honored. The card is left for the worker to keep
iterating. Suggest the worker (or the parent session, on the
worker's behalf) run the Debug build, re-capture a log that has
NO VUIDs and that contains `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=20`
runs, then re-run the validator. After those, post fresh evidence
on the card and the next cron tick will verify.

Full audit trail: `docs/OVERSEER_HEALTH_2026-08-12.md`.
Escalation: `docs/OVERSEER_ESCALATION.md`.
