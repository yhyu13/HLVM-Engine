# Pending Plan Review v226

- plan: docs/PENDING_PLAN_v226.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-579)
- timestamp: 2026-08-21

## Design soundness

The plan solves a real and previously-unaddressed gap. v200 is the lineage's only pre-build compile-risk audit and it **is** scoped to v183–v199 — verified directly, not accepted: `PENDING_COMMIT_v200.md:7` reads *"Pre-build compile-risk audit of the unbuilt v183-v199 chain."* Cycles v206/v207/v209 modified engine source after that boundary and have never been audited as a chain. The acceptance criteria are testable by inspection (residual-reference counts, consumer enumeration, copy-divergence diff), and every one is source-decidable without a build — which matters because no build is reachable here.

## Plan completeness

Complete, with one strengthening required and supplied below rather than returned as a FIX, because it **confirms** the plan's approach rather than redirecting it.

The plan correctly identified its own highest-risk item: v207 verified its safety argument against line numbers, and its affected consumer compiles a *different* copy of the shader. That is the v182 dual-copy hazard in its subtlest form — not "do the copies differ?" but "was the argument made against the copy that matters?" I tested it:

- `TestPathTraceGI` does **not** have its own `GIPathTracing.hlsl`. `ShaderMake.cfg:1` names the file, but the source resolves via `ShaderMakeBuild.py:571` to `Private/Renderer/Shader/GI/GIPathTracing.hlsl` — the shared copy. This is a genuine trap: a `files`-scoped check of the test data dir shows a `.cfg` naming a file that is not there.
- Both copies are byte-identical in the load-bearing region: `Output[pixel]` at 537/819/822/826 in **both**; `OutputDirection[pixel]` at **645 in both**; sole RayGen-scope `return;` at **538 in both** (the only other is at 921/923, inside the closest-hit RR block, not RayGen).

**So v207's argument transfers to the copy its affected consumer actually compiles.**

## One correction to v207's stated reasoning (does not change its verdict)

`PENDING_COMMIT_v207.md:57-59` argues the stray u2 write is harmless because *":645 precedes :819/:826."* Read literally that is a claim about two sibling statements, which would be false — `:819` and `:826` are the two arms of an `#if HLVM_RGI_DEBUG_VIS / #else`, so they never both compile. The conclusion survives in the stronger form: **exactly one of them compiles, both are unconditional, and both follow `:645`**, so on every build configuration the aliased `Output` is fully overwritten after the stray direction write. The sky path returns at `:538`, before `:645`, so it never performs the stray write at all. Recorded because the lineage's standing rule is that a marker's description of code is evidence about its author, not the code — and this one was imprecise even though it was right.

## Feedback for planner

None blocking. Proceed to impl with the dual-copy resolution above treated as established.
