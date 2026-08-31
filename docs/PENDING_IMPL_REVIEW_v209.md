# Pending Impl Review v209

- plan: docs/PENDING_PLAN_v209.md
- commit: docs/PENDING_COMMIT_v209.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-555)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan's two edit sites exactly. **One deviation was
declared** — a 6-line explanatory comment the plan did not budget for — and I
find it **justified, and more than justified: I verified its factual content
rather than its reasonableness**, which is the check v190 exists to enforce
after a comment was once accepted that stated a false mechanism as its own
justification.

The comment asserts *"u1's write is guarded by a constant, so a 1x1 target is
safe there; u2's write is unconditional."* Both halves confirmed in
`TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`:

- **u1** — `:104` declares `DebugStatsTexture : register(u1)`, and its only
  store is `:831` `DebugStatsTexture[pixel] = ...`, sitting inside
  `:830 if (g_GI.Params3.z > 0.5f && debugTotalSamples > 0.0f)`. `Params3` → 5
  hits; `:67` documents `z=DebugStatsEnabled`, matching the C++ guard at
  `FGIPass.cpp:606`. The claim is true **and understated**: the store is
  additionally wrapped in `#if GI_DEBUG_STATS` (`:829`/`:837`), so it is
  compile-time excluded as well as runtime-guarded.
- **u2** — `:101` declares `OutputDirection : register(u2)`; its store at
  `:645` `OutputDirection[pixel] = float4(firstSampleDir, 1.0);` has **no
  enclosing conditional and no `#if`**. Unconditional, as stated.

So the comment records a real asymmetry, and it is the asymmetry that makes
the deletion safe to perform on one member and unsafe on the other. Without
it, the header shows two visually identical `Dummy*Texture` declarations
reduced to one, and the natural repair is to restore the second — which would
reinstate exactly the unguarded out-of-bounds UAV store v207 removed.

**Note for the record:** while checking this I ran
`pattern="DebugBounceStats"` and `pattern="DebugStats\["` against that shader
and got **0 and 0**. Both are misleading — the shader-side symbol is
`DebugStatsTexture` (the C++-side name is `DebugBounceStats`), and the second
zero is the v208 BRE/ERE trap, `[` needing no escape but the query shape being
wrong for this engine. I did not conclude anything from either; I found the
real declaration via `register(u1)`. Recording it because two uncontrolled
zeros in a row is precisely the shape that has produced false findings in this
lineage.

## Security scan

- [x] No hardcoded secrets — deletion of a texture handle
- [x] No shell injection — no shell constructs
- [x] No eval/exec — N/A (C++)
- [x] No SQL injection — N/A

## Self-review checklist

- [x] **Validation:** post-patch `DummyDirectionTexture` → 0 in
      `Engine/Source/Runtime`, controlled in the same scope by
      `DummyDebugStatsTexture` → 5 and `MaterialPlaceholderTexture` → 6.
      Re-derived by me, not read from the impler's table.
- [x] **Error handling:** unchanged. The member was never read, so no branch
      loses a case. Any reference the analysis missed is a **compile error**,
      not a silent miscompile — the loud failure mode.
- [x] **Tests:** none required; no test file touched, `produces_test_files: no`.

## Adjacency check — the realistic failure mode

Both deleted lines sat between two live members, in both files. Read the
post-patch source in place rather than trusting the diff:

- `FGIPass.h:139` `DummyDebugStatsTexture` present; `:146`
  `MaterialPlaceholderTexture` present; comment occupies `:140-145`.
- `FGIPass.cpp:191` `DummyDebugStatsTexture = nullptr;` and `:192`
  `MaterialPlaceholderTexture = nullptr;` now adjacent, both intact.

Class structure intact: closes at `:150`, namespace at `:151`, `LastFrameStats`
and `bIsInitialized` unmoved.

## Feedback for impler (FIX only)

None. KEEP.
