# Pending Tests v79

- plan: docs/PENDING_PLAN_v79.md
- commit: docs/PENDING_COMMIT_v79.md
- approach: 1 Part A static fresh-probe via read_file; 0 runtime probes (terminal-blocked)

## Part A — static fresh spot-checks (cron-driven; NOT by-reference to v78 audit)
- T-A1: v22 binding-layout-split dispatch site at FRayTracingPipeline.cpp:353-364 — PASS via read_file offset 350-374 (fresh this tick; confirms `State.addBindingSet(SRVBindingSet.Get())` at line 357 + `State.addBindingSet(UAVBindingSet.Get())` at line 361)

## Part B — runtime probes (parent-driven; tirith-blocked)
- T-B1 through T-B8: parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` + `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` + validates with `validate_restir_gi.py` + vision-analyzes `display_frame8.png` + reports B8 zero-VUID check on stderr.log

## Probe target rationale
The v22 split has THREE load-bearing sites that must coexist:
1. **FGIPass.h:106** — `UAVBindingLayout` member declaration (additive to BindingLayout)
2. **FGIPass.cpp:183/311/612** — `UAVBindingLayout` init / create / use (the resource side)
3. **FRayTracingPipeline.cpp:353-364** — `addBindingSet(SRVBindingSet)` THEN `addBindingSet(UAVBindingSet)` (the dispatch side)

If any one of these sites drifts, the v22 split silently regresses and bug-075 (VUID-00344) resurfaces. Sites 1 and 2 were re-verified intact at v72 audit (10-part probe set) and site 3 at v77 audit (1-part probe set). This v79 probe is the loop-closure re-verification at site 3 with a fresh read_file (NOT by-reference) — together with v78 audit's verification of v3 spdlog markers and v77 audit's verification of the FRayTracingPipeline addBindingSet pattern, this v79 probe closes the cumulative 22-patch inventory re-verification at the well-known anchor sites for this standby chain.
