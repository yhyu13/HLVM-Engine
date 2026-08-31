# Pending Impl Review v206

- plan: docs/PENDING_PLAN_v206.md
- commit: docs/PENDING_COMMIT_v206.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-552)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan as amended by the plan gate. One deviation was
declared — dropping risk 2's investigation of the control — and it is **justified
and correctly attributed**: the gate directed it, and the impler independently
re-derived the card-L partition rather than citing the gate's word for it
(13 `ReBLURHistoryTexture` hits, creations inside the init block, none in the
resize block). That is the right handling: a deviation that removes work still
needs its own evidence, and it got it.

The plan gate's stronger requirement — enumerate all six operands from their
**creation sites**, not from pipeline position — was met, and the impler
explicitly declined to rest on the ordering argument the plan originally
offered, recording it as explanation rather than evidence. That distinction is
the substance of the gate's correction and it survived into the marker.

## Independent re-derivation

I re-derived the two rows the determination rests on.

**The diff is comment-only, verified structurally rather than visually.**
`nvrhi::TextureHandle` over the header → **8** hits: five `FDesc` members
(`:94-98`), the smoke-overload's two parameters (`:116`), and the two dummy
members (`:142-143`). v205 recorded 4 hits for this same query on
`FBilateralDenoisePass.h`; a different file with a different member set, so the
counts are not comparable and I did not treat them as such. What matters is the
partition: all five `FDesc` members present, in the original order, same types.
Nothing added, removed, renamed or reordered. **Layout, size and member set are
byte-identical**, so no translation unit can be affected — including the
known-good control, which includes this header.

**The `GB(` zero is sound.** Re-ran it: 0 on
`TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl`, 22 across the sibling kernels in
the same directory from the same query. Audit row 18 satisfied — the zero has a
same-shape positive and the query returned no `error` field.

## THE ROW THE IMPLER'S TABLE DID NOT CLOSE

The impler enumerated six operands **at one call site**. `ReBLURPass.Dispatch`
→ **2 hits tree-wide**: `TestReSTIR_GI_Temporal.cpp:1250` and
`TestCornellBoxGI.cpp:1466`. The `FDesc` overload therefore has exactly two
consumers, and **the invariant the impler wrote into the header is a claim about
both of them**, not about the one it enumerated.

I closed the second. It **satisfies** the invariant, and — this is the part that
matters — **it satisfies it for a different reason**:

| Operand | Control's value | Sized at | Extent |
|---|---|---|---|
| `CurrentRadianceTexture` | `HDRTexture` | `:876`, resize `:1233` | `GBufferWidth/Height` |
| `HistoryTexture` | `ReBLURHistoryTexture[0]` | `:932` only | **startup** extent |
| `DepthTexture` | `GBufferDepthTexture` | `:612`, resize `:1199` | `GBufferWidth/Height` |
| `NormalRoughnessTexture` | `GBufferNormalsTexture` | `:566`, resize `:1187` | `GBufferWidth/Height` |
| `OutputTexture` | `DenoisedHDRTexture` | `:894`, resize `:1237` | `GBufferWidth/Height` |
| `OutputWidth/Height` | `CurrentFBInfo.width/height` | `:1464-1465` | swapchain |

`GBufferWidth` is `Framebuffer->getFramebufferInfo().width` at init (`:521`) and
`CurrentFBInfo.width` on resize (`:1166`). So five of six operands **track the
swapchain**, and at any moment they all equal it — the invariant holds, by a
mechanism opposite to the primary's (which holds because everything is pinned to
a constant). **Two consumers, two different mechanisms, same invariant.** That
is what makes the invariant a real contract rather than an artefact of one
caller, and it is a stronger result than the impler claimed.

The sixth row is the exception and it is **card L**, as the impler said:
`ReBLURHistoryTexture` is created once at `:932`/`:934` and never recreated,
while the other five follow the window. On a resize the control violates the
invariant this cycle just documented — **so the header comment now states, in
source, the precise property card L's remaining half must restore.** The card
gains a written specification it did not have. Correctly not actioned here: card
L is build-gated and the file is the known-good control.

## Security scan

- [x] No hardcoded secrets — comment-only diff
- [x] No shell injection — no shell reachable
- [x] No eval/exec — N/A
- [x] No SQL — N/A

## Self-review checklist

- [x] Validation: determination rests on enumeration with controlled zeros
- [x] Error handling: N/A — no runtime path changed
- [x] Tests: no test files produced; `produces_test_files: no` is accurate, so
      `skip_impl_review` could have been `yes` under HARD INVARIANT #2. It was
      set to `no` and that was right — a negative determination plus a header
      edit is the combination v203 got wrong.

## Feedback for impler

None blocking. The second-consumer row is recorded here rather than sent back,
since it **strengthens** the marker's conclusion rather than contradicting it.
Carry it forward: the invariant is now known to hold across both consumers by
two independent mechanisms.

## Standing rule proposed

**When a cycle writes an invariant into a shared class's header, the invariant's
domain is every consumer of that class, and the cycle must close the consumer
set by query before the header edit is verifiable.** The impler enumerated one
consumer thoroughly and wrote a contract binding two. This is the same scope
error v202 caught in v200/v201 (single-`.cpp` scope) and v204 caught in v203
(single-class scope), now recurring at the level of *a header comment's
audience*. Three instances, one level apart each time.
