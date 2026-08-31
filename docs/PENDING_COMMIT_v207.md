# Pending Commit v207

- plan: docs/PENDING_PLAN_v207.md
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp,
  Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h
- source: no bundle — direct source analysis
- target: working tree (no commit, no push — per job instruction)
- task: card Q — u2 fallback bound a 1x1 dummy while the shader stores to it
  unconditionally at the raw dispatch coord; bind the mandatory output UAV
  instead, and pin the UAV extent contract at the `FGIPassDesc` declaration.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: 12th instance of the extent class; **first one in a UAV write rather
  than a guide read, and first found in `FGIPass`**, which no cycle had evaluated
  against this invariant.

## Diff

**`FGIPass.cpp:633-661` → `:633-648`.** The `if/else` that lazily created a 1x1
`FGIPass.DummyDirection` texture is replaced by a ternary onto
`Desc.OutputTexture`, with one unconditional `setTextureState`. **-29 / +18,
of which 12 are comment: 2 functional lines net.**

    nvrhi::TextureHandle DirectionUAV = Desc.OutputDirection
        ? Desc.OutputDirection
        : Desc.OutputTexture;
    CmdList->setTextureState(DirectionUAV, nvrhi::AllSubresources,
                             nvrhi::ResourceStates::UnorderedAccess);
    SRVBuilder.SetTextureUAV(2, DirectionUAV);

**`FGIPass.h:36-49`.** +14 comment, 0 functional. States the contract as a
**requirement on callers** (plan-gate FIX item 2), covers u0/u1/u2 separately
with the reason each is or is not exempt, and contrasts it against the guide SRVs
so a reader arriving from the v205/v206 sibling headers does not carry the wrong
rule across — *writes are pinned; reads are scaled.*

## Evidence the defect was real

- Shader stores `OutputDirection[pixel]` at `:645` with the **raw** dispatch
  coord and the comment "Done unconditionally". No extent guard anywhere in
  `RayGen`.
- `TestPathTraceGI.cpp` never sets `OutputDirection` (**0** hits, controlled by
  **7** for `Desc.OutputTexture` in the same file) and dispatches
  `CurrentFBInfo.width` x `.height` (`:438-439`) — so every thread but one stored
  out of bounds into a 1x1 texture.
- **The u1 contrast is the proof it was an oversight**: u1 has the identical
  dummy fallback, but its write is gated by `Params3.z`, set from exactly the
  condition that selects the real texture (`FGIPass.cpp:471` ↔ shader `:830`).
  u2 copied the fallback and dropped the guard.

## Plan Deviations

**None.** The patch is what the revised plan specified. Three points where I
checked rather than assumed:

1. **The ordering claim.** Re-derived independently of the plan gate: `:645`
   precedes `:819`/`:826`; the only `RayGen`-scope early return (`:538`) fires
   *before* `:645`, so the stray write does not execute on the sky path at all.
2. **Both HLSL copies byte-unchanged** — the fix is C++-only, so the v182
   dual-copy hazard is not engaged and the control's shader is untouched for the
   25th unbuilt cycle.
3. **LSP diagnostics on `FGIPass.h` are pre-existing, not introduced.** The tool
   reported 9 errors after the header edit. Every reported line maps to the
   **pre-edit** numbering (reported `:38` = old `nvrhi::BufferHandle
   LightsBuffer`, reported `:39` = old `uint32_t LightCount`), i.e. they are the
   same include-resolution errors shifted by the 14 comment lines I added. **A
   comment-only edit cannot introduce an undeclared-identifier error.** Recorded
   rather than silently dismissed.

## Known follow-up (carded, deliberately not absorbed)

`DummyDirectionTexture` is now dead: nothing assigns it. The member declaration
(`FGIPass.h:126`), the null-out in `Shutdown` (`:192`) and the creation block are
all still present. **Not deleted this cycle** — removing it would make this
cycle's "2 functional lines" row unverifiable, the same discipline v191-v206 each
applied. Opened as card R.

## What this cycle did NOT establish

That anything compiles, links, runs, renders or validates. Terminal is denied
categorically in this runspace — probed twice this tick, including a bare `true`,
both refused with `tirith:unknown`. **No acceptance gate is verified by this
cycle.**
