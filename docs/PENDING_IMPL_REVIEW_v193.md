# Pending Impl Review v193

- plan: docs/PENDING_PLAN_v193.md
- commit: docs/PENDING_COMMIT_v193.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-539)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly: three substitutions, accumulate block only,
shader untouched. **One deviation was declared and it is not functional** — a
grep anchor removed from a comment during implementation. I verified the
correction landed: reading `:1247-1249` directly, the comment now describes the
kernel's early-out in prose and contains no `search:` anchor and no `:NNNN`
reference. The impler caught this itself, which is the correct outcome.

The deviation is worth preserving in the record for its general form: **v190
banned line numbers because they rot under later edits; a grep anchor into a file
the cycle is not editing rots immediately.** The remedy prescribed by v191 was
applied without checking its target existed. That is a new failure mode for the
anchor convention, not a repeat of one.

## Independent re-derivation (I re-ran everything rather than reading the table)

Read `:1232-1275` in full — the whole patched block, not a grep window:

1. `AccC.Width = WIDTH;` (`:1256`), `AccC.Height = HEIGHT;` (`:1257`),
   `dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1)` (`:1274`). Three, as planned.
2. `AccC.FrameCount` (`:1237`) and `AccC.Exposure` (`:1258`) untouched — correct,
   neither is an extent.
3. The binding set (`:1261-1267`) is unchanged, so the resources the grid now
   covers are the same ones it covered before.
4. The blit block at `:1277+` still reads `FB.width`/`FB.height`. **This is the
   over-substitution risk the plan flagged and it did not occur.** The blit
   writes into the swapchain framebuffer, where the swapchain extent is the
   correct operand.
5. `= HEIGHT` → 18 hits: `:564` (a texture desc), `:1257` (this patch), `:1578`,
   `:1704`, `:1797` (three `const uint32_t W = WIDTH, H = HEIGHT;` locals) and
   `:2983` (device params). **All three locals are in other member functions,
   none in `Render()`.** No shadowing at the patch site. This is the check v191
   performed for `WIDTH` but not `HEIGHT`; done here for both.
6. `dispatch((WIDTH` → 2 hits: `:1156` (v192's resolve grid) and `:1274` (this
   one). v192's site is undisturbed.

All queries used plain substrings. No `|`, no `output_mode=count`, no escaped
metacharacters — the three known false-zero mechanisms on this runspace.

## The cycle's substantive finding is sound and I checked the part that carries it

The plan's claim is that the kernel's guard is keyed to the wrong extent. I
verified against the kernel rather than the marker: `GIAccumulate_cs.hlsl:60-64`
takes `SV_DispatchThreadID` and early-outs on `pixel.x >= Width || pixel.y >=
Height`, where `Width`/`Height` come from `cbuffer AccumConstants` (`:15-21`) —
the same two values the dispatch grid was computed from. The guard was therefore
a tautology.

**This refines v192's severity ordering rather than merely extending it.** v192
recorded "no extent guard at all" as the most severe form of the class. That is
now wrong: a guard parameterised by the wrong extent is more dangerous than no
guard, because it survives audit. An auditor sweeping these passes for the
v189-v192 pattern would see `:63`, tick the box, and move on.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection — no shell invoked (none available)
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] Validation: substitutions are type-exact (`uint32_t` field ← `static const
      uint32_t`), no narrowing, no signed/unsigned mix.
- [x] Error handling: unchanged; the patch alters no control flow.
- [x] Tests: file-only verification table, every row re-runnable.

## Scope check against the acceptance gates

The patch is a **no-op at 800x600**, deliberately. So it cannot move gate 5
(validator) or gate 6 (vision) on the pending operator run, and it cannot perturb
the v183/v184/v185 chain. What it does is remove a mechanism by which a window
resize would fail gate 5 with no diagnostic. Correct call: this cycle should not
be competing for signal with the chain that is already awaiting a single run.

## NEW CARD opened at this gate — eighth instance, and the class is not exhausted

I swept the remaining passes for the same shape and found one:

**Card G — ReBLUR denoise pass constants are swapchain-derived.**
`ReBLURConstants.OutputSize[0]/[1]` and `RcpOutputSize[0]/[1]`
(`TestReSTIR_GI_Temporal.cpp:1182-1185`) plus `ReBLURDesc.OutputWidth`/
`OutputHeight` (`:1203-1204`) are all set from `FB.width`/`FB.height`, while
`ReBLURDesc.OutputTexture` is `DenoisedTexture`, created from `W`/`H`. Its guides
`LinearDepthTexture`/`GBufferNormal` are the fixed GBuffer MRTs and its input is
the fixed `FullResSpatial`.

Two details make this **not** a mechanical repeat, verified before opening:

1. `FReBLURPass::Dispatch` has a fallback (`FReBLURPass.cpp:151-157`): if
   `Desc.OutputWidth` is zero it derives the extent from the output texture's
   descriptor. The call site passes a non-zero swapchain width, so **the fallback
   — which would be correct — is suppressed by the very field that is wrong.**
   The fix may be to pass `WIDTH`/`HEIGHT`, or to pass nothing and let the
   fallback do it. That is a real choice and needs its own cycle.
2. `DenoisedTexture` is dumped as `denoised` (one of the dump group), so this is
   the second instance on an inspected artifact — though note `PENDING_PLAN_v189`
   recorded that no gate currently *reads* the denoised dump, and the pass is
   skipped entirely under `HLVM_RGI_BYPASS` (`:1166`). Lower severity than card F.

Deliberately not bundled: different pass, different files (it reaches into
`FReBLURPass`), and bundling would make this cycle's "no swapchain extent left in
the accumulate block" row unverifiable.

## Feedback for impler

None. KEEP.
