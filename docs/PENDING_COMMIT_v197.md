# Pending Commit v197

- plan: docs/PENDING_PLAN_v197.md (revised to v197.2 after a FIX at the plan gate)
- files: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
- source: no bundle — direct edit
- target: no branch (no commit made; the job instruction forbids committing)
- task: Card K — remove the extent parameters `RenderGBuffer` never uses, and fix
  the log line that reports the wrong extent
- verify: `RenderGBuffer()` → **2 hits** (definition `:2179`, call `:804`);
  `RenderGBuffer(FB.width, FB.height)` → **0**; `MeshCount, WIDTH, HEIGHT` → **1**
- skip_impl_review: no
- produces_test_files: no
- notes: Three functional lines. The first two are the card; the third was added
  at the plan gate and is the more consequential of the two defects.

## What was implemented

| Site | Before | After |
|---|---|---|
| definition `:2179` | `void RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)` | `void RenderGBuffer()` |
| call `:804` | `RenderGBuffer(FB.width, FB.height);` | `RenderGBuffer();` |
| log `:2446` | `FrameCount, MeshCount, LastWidth, LastHeight` | `FrameCount, MeshCount, WIDTH, HEIGHT` |

**+3 / -3 functional, +14 comment.** No other file touched. No shader touched, so
the v182 dual-copy hazard is not engaged.

**Comment budget revised down after a self-review pass.** The first draft carried
29 comment lines for 3 code lines, restating the v195/extent rationale at all
three sites — a DRY violation in prose. Now stated **once**, at the definition
(the place a reader lands when asking "why no parameters?"); the call site and
the log carry two-line notes that point at it. Rationale unchanged, repetition
removed.

## Complete candidate set, re-derived (not inherited from the plan)

`RenderGBuffer(` across `Engine/Source/Runtime` → **2 hits**, before and after:
the definition and the single call. There is **no forward declaration, no second
call site, no virtual/override, and no function pointer** — it is a method on a
single struct — so the arity change has exactly two edit sites and cannot break a
caller not in this table. The remaining 18 bare `RenderGBuffer` hits across
`Engine/` are comment prose, log format strings, a recipe echo, one handoff doc,
and rotated log output; none is a call.

## Scope check performed before the log edit — the one that could have failed to compile

The log substitution is only safe if `WIDTH`/`HEIGHT` are in scope inside
`RenderGBuffer` **and** are format-compatible with the `LastWidth`/`LastHeight`
they replace. Verified rather than assumed: `:106-107`
`static const uint32_t WIDTH = 800;` / `HEIGHT = 600;` at **file scope**, and
`:2987` `uint32_t LastWidth = 0;` — **identical type**, so the `HLVM_LOG` format
arguments are unchanged in both arity and type. Had these been, say, `int`
constants, the `{}x{}` substitution would still have compiled but the check would
have been worth doing anyway.

**Note on a false zero I hit and corrected inside the cycle:** my first scope
query was `constexpr uint32_t WIDTH` → **0 hits**, which read as "WIDTH is not a
constant and may not be in scope." That was a wrong-shape query, not a finding —
the declaration is `static const`, not `constexpr`. `uint32_t WIDTH` → 1 hit gave
the real answer. This is a **fifth** false-zero mechanism for the running list,
and unlike the previous four it is not a tool quirk: **the query encoded an
assumption about the declaration form, and the zero read as a fact about the
code.** Same failure shape as the card-E/G/H lesson, expressed as a query.

## Plan Deviations

**None.** Both plan-gate FIX items were absorbed into v197.2 before implementation
and both are in the table above. Scope is exactly the revised plan's.

## What this cycle did NOT establish

Nothing was compiled, built, run, or viewed — `terminal` was probed this tick and
refused (`pending_approval`, `tirith:unknown`, `exit_code -1`). **The arity change
is the first edit in the v183-v197 chain whose failure mode is a compile error
rather than silent runtime behaviour**, which cuts both ways: it cannot hide, but
it also cannot be confirmed until the chain is first built.
