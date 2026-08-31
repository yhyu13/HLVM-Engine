# Pending Impl Review v191

- plan: docs/PENDING_PLAN_v191.md
- commit: docs/PENDING_COMMIT_v191.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-538)
- timestamp: 2026-08-30

## plan_fidelity_check

The plan asked for exactly two functional substitutions plus an invariant-stating
comment, and forbade three specific scope expansions (float division, resizable
GBuffer, touching Cornell). The impl does exactly that. Re-read both sites myself:
`:1039` and `:1087` each read
`static_cast<float>(WIDTH / std::max(HalfResWidth, 1u))`. No shader file was
touched, so the v182 "patched a copy nothing compiles" trap is not engaged.

One deviation declared, and it is a **self-caught regression, not a shortcut**.
The impler's first draft cited source positions by line number and its own
18-line insertion invalidated all four before the cycle ended. That is the stale-
cross-reference defect v189 shipped and v190 eliminated — `// .*:[0-9][0-9]+` →
0 hits file-wide was v190's specific achievement, and v191 would have been the
first cycle to undo it. The impler detected this by re-querying after the edit
rather than trusting the draft, and replaced all four with symbol names plus one
grep anchor. **Correctly handled, correctly disclosed.** The right lesson is
recorded: line numbers in comments rot; symbol names do not.

I re-ran the residual query: `// .*:[0-9][0-9][0-9]` → 5 hits at `:894`, `:990`,
`:993`, `:994`, `:2305`. I checked each rather than accepting "pre-existing":
all five reference *other* files (`FReSTIRPass.cpp:393`,
`ReSTIR_Temporal_cs.hlsl:136/170/176`, `FGIPass.cpp:533`), which an edit to this
file cannot shift. The impler added none. Leaving them is correct scope
discipline.

## Independent re-derivation of the premise

I did not accept the plan's operand analysis. Both re-checked:

- `search_files pattern="FB\.width"` → 16 hits. Neither `:1039` nor `:1087` is
  among them; the two ReSTIR constants blocks are clean.
- `search_files pattern="const uint32_t W = WIDTH"` → the binding at `:1541`
  opens `CreateGBufferTextures`, which is the sole creator of `GBufferNormal`
  (`NmDesc = WpDesc`, `WpDesc.width = W`) and `LinearDepthTexture`
  (`CreateTexture2D(..., W, H, ...)`) — the two textures `GB()` indexes.
- `HalfResWidth` is set from `HalfW = W / 2` in that same function.

So both operands are fixed and `WIDTH` is the correct numerator. The shadowing
check also holds: 40 `WIDTH` hits, sole declaration at `:106`, every local
`W = WIDTH` confined to `CreateGBufferTextures` / `FillGBufferHardcoded` / `:1760`
— none in `Render()`.

## Severity claim verified against the shader

The comment asserts specific silent-failure behaviour, so I checked the consumer
rather than the comment. `ReSTIR_Spatial_cs.hlsl:52-56`:

```hlsl
int2 GB(int2 p) { int s = max(int(gConstants.GBufferScale), 1); return p * s + (s >> 1); }
```

`ReSTIR_Temporal_cs.hlsl:80` is the same form. The C++ division is
`uint32_t / uint32_t` inside the cast, so it truncates first. A 600-wide
swapchain therefore yields `s = 1` and `GB` becomes the identity — v183's fix
silently undone, which is precisely v184's failure mode reached by a different
route. The comment does not overstate.

## NET-NEW at this gate: a SIXTH instance of the same class, in the resolve pass

Nobody in this cycle checked the *other* `FB.width` consumers. I did.
`Resolve_cs` is dispatched at `:1138` `(FB.width + 7) / 8, (FB.height + 7) / 8`
and its constants carry `RC.RcpFullW = 1.0f / FB.width` (`:1109-1110`) — but its
outputs `FullResGIRaw` and `FullResSpatial` are created at `:1633-1638` from
`CreateTexture2D(NvrhiDevice, W, H, ...)`, i.e. the **fixed** `WIDTH`/`HEIGHT`,
and its guide textures `LinearDepthTexture`/`GBufferNormal` (`:1119-1121`,
`:1129-1130`) are the fixed-size GBuffer MRTs. Its shader reconstructs full-res
positions from `RcpFullW` and samples the guides at `fp = hp * 2 + 1`
(`Resolve_cs.hlsl:60`) — a **hardcoded** scale of 2, which is only correct while
the swapchain equals `WIDTH`.

So on a resize the resolve pass would dispatch a grid sized to the swapchain,
over fixed-size outputs, with a hardcoded 2x guide stride. Same class as v189
(variable extent → fixed-extent pass) and as this cycle.

**Not folded into v191** — different pass, three or four sites, and it interacts
with the `hp * 2 + 1` hardcode which is a genuine design decision rather than an
oversight. Bundling would break one-variable-per-experiment and would turn a
verified two-line patch into an unverifiable one. Opened as a card instead.

## Security scan

- [x] No hardcoded secrets — one arithmetic operand changed
- [x] No shell injection — nothing executed
- [x] No eval/exec — N/A (C++)
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: the `std::max(HalfResWidth, 1u)` divide-by-zero guard survives
      at both sites (`HalfResWidth` is a member default-initialized to 0 and set
      only in `CreateGBufferTextures`)
- [x] Error handling: no control flow changed
- [x] Tests: `produces_test_files: no`; static verification only, correctly, since
      no shell exists in this runspace

## On the v190 standing stop-condition

`PENDING_IMPL_REVIEW_v190.md:92-94` said the pipeline should stop if the next
cycle produced no functional change. **This cycle changes two functional lines**,
found by re-deriving from source rather than from the queue, so the condition is
satisfied on its terms rather than evaded. I am carrying the condition forward
unchanged for v192.

## Feedback for impler (FIX only)

None — KEEP.
