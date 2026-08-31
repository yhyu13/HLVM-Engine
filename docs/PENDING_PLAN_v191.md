# Pending Plan v191

- task: GBufferScale is derived from the RESIZABLE swapchain extent while both of
  its operands are fixed-size — fifth instance of the Phase-D omission class, and
  the second instance of the v189 "variable extent fed to a fixed-extent pass"
  defect specifically.
- source: no bundle — direct edit, derived from source this tick
- planner: agent_1_planner (tick-538)
- timestamp: 2026-08-30

## Why a cycle at all (Rule 9 vs the reviewer's standing stop-note)

`PENDING_IMPL_REVIEW_v190.md:92-94` set a standing condition: *"if the next cycle
also produces no functional change, the pipeline should stop and say so."* That
condition is respected, not evaded — **this cycle changes two functional lines.**
The queue had zero `- [ ]` cards, so Rule 9 had nothing to route; rather than emit
a 538th closure doc (`§Anti-patterns §6` drift), the planner re-derived from
source and found a defect no prior tick has recorded.

## The defect

`TestReSTIR_GI_Temporal.cpp:1023` and `:1069`:

```cpp
TC.GBufferScale = static_cast<float>(FB.width / std::max(HalfResWidth, 1u));
SC.GBufferScale = static_cast<float>(FB.width / std::max(HalfResWidth, 1u));
```

`FB` is `Framebuffer->getFramebufferInfo()` (`:749`) — the **swapchain** extent.
The window is created `Resizable = true` (`:2914`), and `BackBufferResizing()`
(`:1292-1295`) only clears the binding cache; it does **not** recreate the GBuffer.

But neither operand of the ratio this constant is supposed to express is the
swapchain:

- **numerator** — the textures the shader scales *into* are the GBuffer MRTs
  `GBufferNormal` / `LinearDepthTexture`, created in `CreateGBufferTextures()`
  from `const uint32_t W = WIDTH, H = HEIGHT;` (`:1523`, `:1533/1537/1562`), with
  `WIDTH = 800` a file-scope constant (`:106-107`). **Fixed.**
- **denominator** — `HalfResWidth = W / 2` (`:1578-1580`), also off `WIDTH`.
  **Fixed.**

So the correct value is the compile-time constant `WIDTH / HalfResWidth == 2`,
and it is being computed from a runtime-variable third quantity that is only
*coincidentally* equal to the numerator at startup.

## Two distinct failure modes, both silent

Integer division happens **before** the `static_cast<float>`, so:

| swapchain width | computed scale | shader `max(int(s),1)` | effect |
|---|---|---|---|
| 800 (startup) | 2 | 2 | correct |
| 1200 (widened) | 3 | 3 | `GB()` samples the 800-wide GBuffer at stride 3 → reads up to x=1197 → out-of-bounds `Load` returns 0 → depth/normal validation fails → **history rejected**, `M` collapses |
| 600 (narrowed) | 1 | 1 | **v183's fix becomes inert** — exactly the v184 failure mode, where `GBufferScale` degenerating to 1 made the half-res correction a no-op that still compiled and still looked right |
| 500 | 1 | 1 | same, and the truncation is invisible in the source |

The 600-width row is the serious one. v184's whole finding was that a
`GBufferScale` of 0/1 silently disables v183 while leaving code that reads as
correct. This line reintroduces the same end state through a different route,
and **it would do so during the very operator run that is supposed to test
v183/v184/v185** if that operator resizes or if the WM opens the window at
anything other than the requested extent.

## Why this is the v189 defect class, not a new one

v189: `Bd.OutputWidth = FB.width` fed a *variable* extent to a pass whose input
is a *fixed* 400x300 texture. Fixed by substituting `HalfResWidth`.

v191: `FB.width` feeds a *variable* extent into a ratio whose real numerator is
the *fixed* 800-wide GBuffer. Same substitution shape, same file, missed because
v189 was scoped to the bilateral block and stopped at `:874`.

Notably `search_files pattern="HalfResWidth"` → 17 hits was run at the v190
review gate and reported "all sixteen non-bilateral sites untouched" — correct,
but the query could not see this bug, because these two lines are wrong in their
*numerator*, and `HalfResWidth` is their denominator and is already right.

## approach

Replace `FB.width` with `WIDTH` at `:1023` and `:1069`. Two functional lines.
`WIDTH` is in scope (file-scope `static const`), is the exact width used to
create the textures being scaled, and makes the constant genuinely invariant.

Deliberately NOT doing:

- Not switching to float division. The shader truncates with `max(int(s),1)`
  regardless, so float division would change nothing at 800/400 = 2 and would
  disguise the fact that this must be an integer ratio.
- Not making the GBuffer resizable. That is a real design question, much larger
  scope, and would touch every pass in the frame.
- Not touching Cornell — `TestCornellBoxGI.cpp:1592/1645` assign `1.0f` literals
  derived from its own full-res call site (v187/v188). Correct and unaffected.

## diff_estimate

+2 / -2 functional, plus ~10 comment lines recording why the operand is `WIDTH`
and not `FB.width` so the next reader does not "helpfully" restore it.

## skip_plan_review

no — the impl is two lines but the *reasoning* about which extent is correct is
precisely what v187/v188/v190 each got wrong at the card stage. Gate it.

## test_strategy

File-only static verification (no shell available):

1. `FB\.width` count in the ReSTIR block after the patch — must not include
   `:1023`/`:1069`.
2. `GBufferScale` tree-wide — every assignment accounted for, Cornell's two
   `1.0f` literals untouched.
3. `HalfResWidth` count — must stay 17; the denominator is not being changed.
4. Confirm `WIDTH` is the same symbol `CreateGBufferTextures` uses.
5. Confirm the guard `std::max(HalfResWidth, 1u)` survives (divide-by-zero on the
   first frame before `CreateOutputTextures` runs).

## risks

- **`WIDTH` shadowing.** If a local `WIDTH` exists in `Render()`'s scope the
  substitution silently binds to the wrong thing. Must be checked, not assumed.
- **This cannot be run.** Same blocker as v183-v190. The patch is argued from
  source only; the falsifiable prediction is recorded below for the operator.
- **It cannot be the cause of the current `M mean=2.93`** — at the startup extent
  of 800 the computed value is already 2, i.e. correct. This is a latent defect
  and must not be reported as an explanation of the observed symptom. Recording
  that explicitly, because the temptation to claim it is exactly the mistake
  `§Anti-patterns` warns about.
