# Pending Commit v208

- plan: docs/PENDING_PLAN_v208.md
- files: **none — zero source files modified** (audit only)
- source: no bundle — direct source audit
- target: working tree (no commit, no push — per job instruction)
- task: pre-build compile-risk audit of the v201-v207 delta, which v200's audit
  predates and no cycle has swept.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: **The headline is not the audit result. It is that the lineage's query
  tool was diagnosed rather than catalogued, and the diagnosis invalidates a
  documented rule that 28 ticks have been following.**

## RESULT 1 — the v201-v207 delta is compile-coherent in both demonstrated classes

**(a) Arity/signature.** No cycle in this delta changed a signature. v207 changed
an initialisation expression inside a function body; v202/v203/v205/v206 changed
comments and one operand. Nothing to break a caller.

**(b) Cbuffer layout — the silent class.** Only v204 touched a cbuffer. Checked in
**all four** expressions per v200's rule, not two:

| Expression | Evidence | Slot 5 |
|---|---|---|
| C++ writer | `FBilateralDenoisePass.cpp:189` `ConstantsData[5] = GuideScale` | ✅ |
| Buffer size | `:122` `byteSize = 256`; `:156` `float ConstantsData[64]` = 256 B | ✅ fits |
| HLSL (primary) | `TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl:21` `float GuideScale` | ✅ |
| HLSL (control) | `TestCornellBoxGI_Data/…:26` `float GuideScale_Unused` | ✅ same offset |

Both copies place it after `float2 TexelSize` + 3 floats = **offset 5**, matching
the writer. The v184 rule (never an array in the tail) holds: tail is
`Pad1`/`Pad2` scalars in both. The control names the slot without consuming it,
which keeps the layout legible — the right call, and it means the field cannot
silently shift if the control is edited.

**v203's near-miss is verified undone-correctly.** That cycle's third `patch`
deleted three live binding items and restored them; had the restore been
imperfect, the damage would be in the tree now and the first build would blame
any of twenty-six cycles. Re-derived independently: `SpatialLayout` `:325-333`
= 7 items in correct order; `TemporalLayoutSRV` `:236-248` = cb + t0..t9 = 11;
positive control `BindingLayoutItem::` → 29 tree-wide. **Intact.**

**v207's fallback operand cannot be null**: `FGIPass.cpp:645-650` feeds
`DirectionUAV` to `setTextureState`, and `Desc.OutputTexture` is enforced by a
real early return, not merely documented. Confirmed still present.

## RESULT 2 — THE FALSE-ZERO MECHANISM IS DIAGNOSED, AND ONE STANDING RULE IS WRONG

Twenty audit rows catalogue query shapes that "silently return 0". No tick ever
asked **why**. The plan required it. The answer is one mechanism:

> **`search_files` interprets its pattern as POSIX BRE (basic regex), not ERE.**

In BRE, `|`, `+`, `?`, `(`, `)`, `{`, `}` are **literal**; the escaped forms
`\|`, `\+`, `\(` are the metacharacters. This is the exact inverse of ERE, which
every rule in the lineage assumed.

**Controlled experiment, one file, same tool:**

| Pattern | Hits | Explains |
|---|---:|---|
| `static_assert\|FReBLURConstants` | **4** | alternation works — **escaped** |
| `static_assert\|FReBLURConstants` → bare `\|` form | **0** | tick-526's finding |
| `sizeof(FReBLURConstants)` | **1** | parens are **literal** in BRE |
| `sizeof\(FReBLURConstants\)` | **0** | v192's "false failure" |
| `offset++` | 19 | literal `+` |
| `offset\+\+` | 29 | `\+` = one-or-more |
| `static_assert\(sizeof` | **error** | `grep: Unmatched ( or \(` — proves BRE |

The last row is decisive: the tool reported a **BRE-specific error string**.

**Consequences — this is why it matters more than the audit:**

1. **tick-526's standing rule is WRONG.** It reads *"never use `|` in a
   search_files pattern; one query per term."* The correct rule is *"use `\|`."*
   Alternation is available and has been avoided by 28 ticks for no reason.
2. **v192's "false FAILURE" and tick-526's "false pass" are the same bug**, not
   two phenomena. `\+`/`\(` were being read as escapes-of-literals in the
   author's ERE mental model, and as metacharacters by the engine — and
   vice-versa for bare `|`.
3. **Which recorded zeros are vacuous is now decidable rather than a worry.** A
   zero is sound iff its pattern contains no unescaped ERE metacharacter. Plain
   substring queries — the overwhelming majority in this lineage, e.g.
   `GBufferMaterial`, `DummyDirection`, `createTexture` — are **unaffected and
   sound.** The vacuous ones are specifically those using ERE `|`, `+`, `(`.
4. Reproduced against two live claims: `WIDTH + 7` → 2 hits but `WIDTH \+ 7` → 0;
   `hp * 2 + 1` → **0 while the line provably exists** at `Resolve_cs.hlsl:60`
   (`hp \* 2 + 1` → 1). v192 recorded both as inexplicable; they are now explained.

## Plan Deviations

**None.** The plan asked for the mechanism and got it. Two points checked rather
than assumed:

1. I did **not** take the BRE conclusion from the hit counts alone — counts alone
   are consistent with several explanations. The `Unmatched ( or \(` error string
   is what makes it a diagnosis rather than an inference, since that phrasing is
   emitted only by a BRE engine.
2. I verified the underlying text exists for every zero I called false, rather
   than assuming the file was as a prior marker described it.

## What this cycle did NOT establish

That anything compiles, links, runs, renders or validates. Terminal is denied
categorically — probed twice this tick including a bare `true`. **26th
consecutive unbuilt cycle. No acceptance gate is verified by this cycle.**
