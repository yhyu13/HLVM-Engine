# Pending Plan v186

- task: close the queued `FReSTIRConstants.Pad` C++/HLSL kind-mismatch card
  (PENDING_PICK line 119), opened by tick-531 and deferred by tick-532
- source: no bundle — direct edit
- approach: `FReSTIRPass.h:28` declares `TFP32 Pad[2]` while
  `ReSTIR_Generate_cs.hlsl:22` declares `float2 Pad`. Replace both with two
  plain scalars, the same treatment v184 applied to the temporal struct after
  the array-packing rule desynced it. Two files, one line each.
- diff_estimate: +6 / -2 lines
- skip_plan_review: no
- test_strategy: file-only static verification (role #5). No build/run is
  available this tick; the tester must state that plainly rather than infer a
  runtime result.
- risks:
  - **Scope risk.** This is a ~2-line inert patch and
    `six-role-pipeline §Anti-patterns §5` warns against spending a 6-role
    cycle on one. Mitigation: tick-532 already deferred this card once;
    deferring again with no action is the drift anti-pattern (§6). Land it and
    close it.
  - **Correctness risk: is it actually inert?** Verified both directions
    this tick rather than inherited: the generation marshaller
    (`FReSTIRPass.cpp:354-365`) increments `offset` to 9 and stops at
    `DebugVis`, so floats 9/10 are never written; and
    `ReSTIR_Generate_cs.hlsl` (full 72-line read) never mentions `Pad`. So the
    field is write-never/read-never and the patch cannot move a pixel.
  - **A note the card gets slightly wrong.** The card says the two sides
    "would desync under the same HLSL array-packing rule that v184 fixed."
    They would not, today: `float2` is not an array, so HLSL packs it at
    floats 9/10, and the C++ `TFP32 Pad[2]` also sits at 9/10 — they agree by
    coincidence. The real hazard is narrower and worth stating accurately: the
    two declarations differ *in kind*, so the next appended field inherits
    whichever packing rule the reader did not have in mind. Fixing it is still
    right; the justification is "remove an inconsistency before it is load
    bearing", not "repair a live desync."

## Explicitly out of scope (carried to PICK as a new card)

A net-new live defect found while verifying the above — the bilateral denoise
dispatch is sized from the full-res framebuffer while its input texture is
half-res. It is NOT patched in this cycle. See `PENDING_COMMIT_v186.md`
§Deferred finding for the evidence and for why patching it blind would be
irresponsible.
