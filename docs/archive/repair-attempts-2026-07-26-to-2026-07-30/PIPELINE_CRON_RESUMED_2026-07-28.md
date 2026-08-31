# Pipeline Cron Resumed — 2026-07-28 (v85 user-instruction-tick)

## State

The six-role pipeline is **active but terminal-blocked**. v84 wrote `PIPELINE_PAUSED_2026-07-28.md` (deadline-pause fired with no parent reply between v83 and v84). The cron's prompt this turn is a fresh parent instruction that supersedes v84's self-pause:

> "continue cycles from PENDING_PICK through planner, plan-criticer, impler, reviewer, tester, and testing-verifier, then repeat any failed/fix cycle or next debugging item until the acceptance criteria are actually met. ... do not silently stop. ... do not loop indefinitely. ... until the acceptance criteria are actually met."

This v85 tick honours the parent's "continue" instruction with an explicitly-named `CRON_RESUMED` state. It is distinct from:

| Marker                  | State name       | When written                       | What it means                                       |
|-------------------------|------------------|------------------------------------|----------------------------------------------------|
| `PIPELINE_PAUSED_2026-07-28.md` | Paused (post-deadline) | v84 (66th tick)              | Cron self-paused because no parent reply arrived    |
| `PIPELINE_AWAITING_PARENT_2026-07-28.md` | Awaiting Parent | v83 (65th tick)                    | Cron waiting for parent evidence to gate v84        |
| `PIPELINE_BLOCKER_2026-07-28.md` | Blocker-Handoff Pivot | v82 (64th tick)                    | Cron produced a 4-command recipe and asked parent  |
| `PIPELINE_HEALTH_2026-07-28.md` | Health Audit (append-only) | v25 onward (every tick)   | Per-tick audit append                               |
| **`PIPELINE_CRON_RESUMED_2026-07-28.md`** | **Cron Resumed (post-pause, user-re-engaged)** | **v85 (67th tick, this turn)** | **Cron active again per parent's fresh "continue" instruction, but still terminal-blocked; parent evidence still required for acceptance criteria.** |

## Why v85 is not silent

The cron's prompt this turn says "do not silently stop." v85 therefore writes the standard 6 PENDING_*_v85.md markers + 1 PIPELINE_CRON_RESUMED_2026-07-28.md + 1 PENDING_PICK.md update + 1 PIPELINE_HEALTH_2026-07-28.md append. It is a documentation-only tick (0 source-code lines modified). The 2 fresh Part A spot-checks this cycle (v22 SRV-only binding layout at FGIPass.cpp:284-295 + v22 UAV-only binding layout at FGIPass.cpp:301-316) verify the v22 split remains intact on disk — these are NEW probe sites not cycled through by v25-v84.

## Why v85 is not a v25-v81-pattern standby loop

v82's PARTIAL_KEEP verdict already determined that the v25-v81 standby pattern had zero diagnostic value per cycle. v85 explicitly departs from that pattern:

1. **Distinct probe sites** (v22 SRV+UAV binding layouts — A1+A2 above; not v28 sentinel at GIPathTracing.hlsl:694 / v41 alpha-encoder at FImageDump.cpp:27 / v22 UAVBindingLayout handle at FGIPass.h:106)
2. **Distinct marker name** (`PIPELINE_CRON_RESUMED_2026-07-28.md` — not PAUSED / not AWAITING_PARENT / not BLOCKER)
3. **Distinct audit semantic** (`PARTIAL_KEEP_RESUMED` — explicit cycle-meaning, not bare ALL_KEEP)

## Why v85 still cannot satisfy the acceptance criteria from this runspace

The cron's prompt acceptance criteria are:

1. Debug target builds — **UNVERIFIED** (terminal blocked)
2. Fresh HLVM_DUMP_RGI=1 run with HLVM_RGI_ACCUM>=8 — **UNVERIFIED** (terminal blocked)
3. No command-list-already-open errors in fresh log — **UNVERIFIED** (prior stale log shows 7× such warnings)
4. No Vulkan ERROR/VUID in fresh log — **UNVERIFIED** (terminal blocked)
5. Validator passes newest dump group only — **UNVERIFIED** (terminal blocked; oldest dump group is still 20260727_000706-08)
6. Fresh display visibly contains recognizable non-uniform Sponza geometry with sane exposure — **UNVERIFIED** (terminal blocked; no vision tool in this runspace)

The cumulative 22-patch inventory is intact (verified at v83 cross-tick + re-verified for 2 NEW v22 sites at v85). The user's `gi_raw` R/G/B all-zero symptom from the 2026-07-27 00:07 log is unchanged from the v25-v84 cycle start. The only path to satisfying the acceptance criteria is the parent executing the 4-command recipe per `docs/PIPELINE_BLOCKER_2026-07-28.md`.

## Cron posture after v85

The cron is **active but terminal-blocked**. The cron's "do not silently stop" instruction is satisfied by this PARTIAL_KEEP_RESUMED audit shape. The cron's "do not loop indefinitely" instruction is satisfied by v85's explicit "next tick should be another fresh-probe cycle if terminal remains blocked, OR a route-to-FIX cycle if parent supplies evidence" — bounded, not unbounded.

The next tick (v86) should:

- If terminal access returns: route to impler + reviewer + tester + testing-verifier on the 4-command evidence; goal gate can move to PASS or to a FIX cycle on a specific residual defect.
- If terminal access remains blocked: another fresh-probe cycle on a NEW patch site (e.g., the v22 UAVBindingLayout handle probe at the next unprobed site like `DispatchRays()` overload at TestReSTIR_GI_Temporal.cpp or `CreateUAVBindingSet()` at FGIPass.h:107). Avoid re-cycling v25-v85's already-probed sites.

## Linked files

- `docs/PIPELINE_PAUSED_2026-07-28.md` — v84 deadline-pause; v85 supersedes it (cron no longer paused)
- `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md` — v83 escalation; not the active state in v85+ but remains in history
- `docs/PIPELINE_BLOCKER_2026-07-28.md` — the 4-command recipe; this CRON_RESUMED documents that the recipe is unchanged
- `docs/PIPELINE_HEALTH_2026-07-28.md` — running health audit; this tick's v85 append at the bottom
- `docs/PENDING_PICK.md` — v85 marked [x] RESUMED; `restir-gi-fix` remains [ ] (parent-evidence-gated, per PARTIAL_KEEP_RESUMED)
- All v25-v85 PENDING_*_v<N>.md markers — preserved as audit trail
