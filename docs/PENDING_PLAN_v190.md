# Pending Plan v190

- task: card D — replace the bilateral denoise dispatch with the barrier flush it exists for
- source: no bundle — direct edit
- approach: see below. **The card's prescribed fix is REJECTED as written.**
- diff_estimate: +0 / -0 functional (see "Verdict" — this plan recommends NO code change)
- skip_plan_review: no
- test_strategy: source-level discriminators; the decisive claim is falsifiable from nvrhi's own source
- risks: see "Why the card's fix is wrong"

## The card

Card D (opened by tick-536 at the v189 plan gate) says:

> The pass exists solely for a side effect: [...] its execution forces nvrhi to
> emit pending layout transitions before the ReSTIR binding sets are created.
> **The correct replacement is one line — `CommandList->commitBarriers()` — the
> exact idiom this same file already uses for this exact purpose at `:1157`.**

And it declares itself build-gated:

> it genuinely **does** need a run to confirm no VUID-00344 appears. Blocked on a
> build, and this time that is a fact, not a claim: the acceptance evidence
> (absence of a validation-layer message) exists only at runtime.

Per the lesson this lineage has now produced three consecutive times (v187, v188,
v189: *a card's stated reason for being blocked is a claim, not a fact*), the
planner's job is to test both assertions before accepting either.

## Finding 1 — the card's premise is FALSE, and nvrhi's own source says so

The card assumes the *dispatch* is what flushes the barriers, so that removing
the dispatch would require re-adding the flush manually. **It is not the
dispatch. It is `setComputeState`, and it already calls `commitBarriers()`
itself.**

`Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-compute.cpp:112-152`,
`CommandList::setComputeState`, read verbatim this turn:

```
120|        if (m_EnableAutomaticBarriers)
121|        {
122|            insertComputeResourceBarriers(state);   // queues requireTextureState
123|        }
...
139|            bindBindingSets(...);                   // descriptors go live
...
145|        commitBarriers();                           // <-- the flush
```

`dispatch()` (`:166-171`) does **not** call `commitBarriers`; it only calls
`updateComputeVolatileBuffers()`. So the flush the comment at
`TestReSTIR_GI_Temporal.cpp:837-844` describes happens at `:185`
(`setComputeState`) inside `FBilateralDenoisePass::Dispatch`, one line *before*
`:186` (`dispatch`).

This is the same fact v189 used to prove grid-independence — but it has a second
consequence nobody drew: **the barrier flush is not attached to the bilateral
pass at all. It is attached to "the next `setComputeState` on this command
list."**

## Finding 2 — therefore the flush is already redundant, three times over

Between the bilateral block (`:845-870`) and the ReSTIR temporal dispatch that
the VUID-00344 comment names, the frame does:

| Site | Call | Contains `setComputeState`? |
|---|---|---|
| `:904` | `ReSTIRPass.DispatchGeneration` | yes — `FReSTIRPass.cpp:400`, immediately after `createBindingSet` at `:384`/`:391` |
| `:1019` | `ReSTIRPass.DispatchTemporal` | yes — the pass the comment is about |

And ahead of ReSTIR generation, `:924-944` issue seven explicit
`setTextureState` calls for exactly the reservoir/depth textures at issue.

So the ordering the comment worries about — *descriptors captured before pending
transitions land* — is handled by `setComputeState` at each consuming pass in
turn. The bilateral dispatch contributes a flush at `:185`, but so does
generation at `FReSTIRPass.cpp:400`, before temporal's binding sets are ever
created at `:481`/`:489`.

**Note the asymmetry with the one site that genuinely needs a manual flush.**
`TestReSTIR_GI_Temporal.cpp:1169`'s `commitBarriers()` sits between
`:1126-1133`'s `setTextureState` calls and `ReBLURPass.Dispatch` — and the
comment there (`:1163-1168`) states the real hazard: nvrhi binds descriptor sets
*before* pending barriers land **within a single `setComputeState`**. That is a
different problem from the bilateral case, and it is why `:1169` is not evidence
for the card's claim: it is a flush placed to precede a binding-set creation that
happens inside the very next call, not a flush standing in for a deleted pass.

## Finding 3 — the card is RIGHT that the pass is incoherent, and the diagnosis is worth recording

v189 sized the dispatch to its half-res input, so every `Load` is in-bounds. But
`Bd.DepthTexture`/`Bd.NormalTexture` (`:849-850`) are full-res, and
`Bd.OutputTexture` (`:851` → `:1582-1584`, created at `W x H`) is full-res. The
pass filters the top-left quadrant with mis-strided guides into the top-left
quadrant of a full-res texture. This is real, and it is confirmed dead on all
three control paths (v189's audit, re-derived: `AccumInput` has exactly two
assignments, `:1111` and `:1180`, and `:1180` executes only after ReBLUR fully
overwrote `DenoisedTexture` at `:1170`).

## Verdict: RECOMMEND NO CODE CHANGE THIS CYCLE

Three options were considered:

**Option A — do what the card says** (delete dispatch, insert `commitBarriers()`).
Rejected: the replacement line is a no-op by Finding 1/2, so this is "delete a
pass and add a placebo." If deleting the pass is safe, the line is unnecessary;
if it is unsafe, the line does not make it safe. Shipping it would encode the
card's incorrect mental model into the source permanently.

**Option B — delete the dispatch outright**, with no replacement. This is the
*honest* form of the card's intent, and Findings 1-2 argue it is safe. **Still
rejected for this cycle**, for one reason that is a fact rather than a claim:
`FBilateralDenoisePass::Dispatch` also issues `writeBuffer` (`:164`) and
`createBindingSet` (`:176`), and `insertComputeResourceBarriers` at `:122` calls
`requireTextureState` on `Bd.InputTexture` = `OutputTexture` (half-res gi_raw)
and `Bd.OutputTexture` = `DenoisedTexture`. `DenoisedTexture` is next touched at
`:1037` (`setTextureState(... ShaderResource)`) and `OutputTexture` at `:876`
(bound as an SRV by generation). Those are explicit transitions and should
subsume it — **but "should" is exactly the word this lineage has been burned by.**
The acceptance evidence for a deletion is the *absence* of a validation message,
and absence-evidence is the one class that genuinely cannot be established from
source. Card D was right about that much, for the wrong reason.

**Option C — record the finding, change nothing, and hand the operator a
decisive one-run experiment.** Chosen.

## What this cycle delivers instead of a patch

1. A refutation of card D's prescribed remedy, from nvrhi's own source, with line
   numbers a reviewer can check in under a minute.
2. A correction to the standing comment at `:837-844`, which currently teaches
   the wrong mechanism ("its execution forces nvrhi to emit the pending layout
   transitions"). **Comment-only** — the impler must not touch a functional line.
3. A precise operator experiment: delete `:845-870` entirely, rebuild, run, and
   grep for `VUID-VkDescriptorImageInfo-imageLayout-00344`. One run decides
   Option B. If clean, the pass goes; if not, the comment at `:837-844` becomes
   true again and the pass stays, permanently justified.

## Risks

- **The impler must not "improve" the pass while editing its comment.** The only
  permitted change is comment text. Any functional edit is a FIX verdict.
- Stale line-number cross-references inside the comment are the defect v189's own
  verification pass caught in itself. The replacement comment must be symbolic
  (name functions, not line numbers) — with one deliberate exception, the nvrhi
  file reference, which is external and stable.
