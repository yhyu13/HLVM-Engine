# Pending Impl Review v198

- plan: docs/PENDING_PLAN_v198.md
- commit: docs/PENDING_COMMIT_v198.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-544)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan, including the plan's pre-commitment to produce no patch if the
sweep landed on a control. One deviation is declared — the deliberate non-action — and it is
not a deviation from the plan at all but the execution of a branch the plan wrote in advance
and the plan gate ratified with its own reason. **Declared, justified, and pre-authorised.**

I checked the thing this section exists to catch: whether the "don't patch a control"
principle was invoked *after* the finding, as a rationalisation for not doing the work. It
was not. It is in `PENDING_PLAN_v198.md` under `risks`, written before the sweep ran, and
the plan gate tested it independently. The ordering is verifiable from the markers.

## Independent re-derivation of the load-bearing claims

I re-derived the three claims the finding rests on rather than accepting the impler's table:

1. **The dispatches are swapchain-sized.** `GenDesc.OutputWidth` → 1 hit, `:1531`,
   `= CurrentFBInfo.width`. `:1514-1517` sets `GenConstants.OutputSize`/`RcpOutputSize` from
   the same quantity. Temporal `:1608-1609` and spatial `:1654-1655` likewise. Confirmed.
2. **The targets are startup-sized.** `Reservoir0Texture` → 9 hits; the only `createTexture`
   is `:968`, inside the `:954` init block, whose `Desc.width = GBufferWidth` resolves to the
   *function-local* `GBufferWidth` declared at `:521` from
   `Framebuffer->getFramebufferInfo().width`. `ReSTIROutputTexture = NvrhiDevice` → 1 hit,
   `:983`. `TemporalRadianceTexture` → 6 hits, one creation at `:985`. Confirmed: single
   creation site each, all at init.
3. **The resize block does not recreate them.** `createTexture` → 32 hits. Inside
   `:1160-1256` there are nine. **None of the nine is a reservoir, radiance, prev-depth,
   prev-normal or ReBLUR-history texture.** The set difference is real.

The `:1166` shadow is worth noting explicitly, because it is what makes the file read
clean: the resize block re-declares `uint32_t GBufferWidth = CurrentFBInfo.width` locally,
so the nine textures it *does* recreate are correctly sized. A reader checking extent
sources sees a correct assignment and moves on. **The file's correctness at every site it
touches is precisely what conceals which sites it fails to touch.**

## The finding I would add to the impler's

The impler ranked this alongside v193's tautological guard. I think the shape is worse in
one specific way that should be on record.

In every prior instance, the wrong extent was *present in the source as a wrong token* — a
`FB.width` where `WIDTH` belonged. A sufficiently careful sweep finds it, and nine cycles
did. **Here there is no wrong token.** The defect exists only as an absence, and absences do
not appear in any grep. The lineage has spent eleven cycles building confidence in a
detection method — enumerate the query shapes, control every zero, widen before believing —
and this instance is invisible to all of it.

That is the real retirement of card I. Not "the query shape list was incomplete" but **"the
class contains members that no query shape can express."** The detection method for those is
structural: for each dispatch, take the set of resources it writes and ask whether every
member is recreated on the same event that moves the dispatch extent. That is a per-pass
audit, not a sweep, and it is the per-pass operand invariant card I asked for — now with a
concrete procedure rather than an aspiration.

## Security scan

- [x] No hardcoded secrets — no source modified
- [x] No shell injection — no source modified
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: every count re-derived with a single-term query, `path` at a directory
      (tick-526); every zero controlled — `v198` → 0 in `Engine/Source/Runtime/Test` against
      `v198` → 4 in `docs/`, same tool, same shape.
- [x] Error handling: N/A, no code changed.
- [x] Tests: none produced; `produces_test_files: no` is accurate, so HARD INVARIANT #2 is
      not engaged and the reviewer gate ran anyway (`skip_impl_review: no`).

## On the zero-source-change outcome

This is the second such cycle. v196's was a determination that no defect existed; this one
holds a reproducible defect and declines to fix it on sequencing grounds. **Those are
different outcomes and the marker distinguishes them properly**, which matters because a
future reader scanning for "cycles that changed nothing" would otherwise merge them and
conclude the pipeline stalled twice.

Card L is well-formed: it states the remedy, states the precondition for actioning it, and
names two further targets to check with the *correct* query shape. Per the standing rule
established by cards E/G/H — **a card's description of code is evidence about its author,
not about the code** — whoever actions L must re-read `:954-1010` and the resize block
before trusting the table above, including the parts that read as settled fact.

KEEP.
