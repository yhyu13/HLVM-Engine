# Pending Test Audit v205

- tests: docs/PENDING_TESTS_v205.md
- commit: docs/PENDING_COMMIT_v205.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-551)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — re-ran rows 2, 3, 5, 12, 14 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive — rows 12/10, 2/3
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No enumeration resting on a convenience wrapper (v201)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No no-op claim resting on two differently-named variables without
      closing their assignment sets (v204) — **row 14 is exactly this, and it
      is the row protecting the known-good control**
- [x] **No zero believed without reading the query's `error` field (v205, new)**

## Independent re-derivation

**Row 2 re-run, because it is the row that establishes the fix landed.**
`Desc.NormalTexture` → 1 hit, `:199`, and I read that line in place to confirm
it is `BindingSetItem::Texture_SRV(2, ...)` — the guide is still *bound*, only
no longer *measured*. A fix that unbound it would satisfy the same count and be
wrong. Controlled by `Desc.DepthTexture` → 3.

**Row 14 re-run.** The no-op claim rests on four assignments across two files.
Primary: `GBufferNormal =` → 1 assignment (`:1630`, from `WpDesc` whose width
is set at `:1620` to `W`); `LinearDepthTexture =` → 2 (`:1655` via
`CreateTexture2D(..., W, H, ...)`, `:1664` re-created from that texture's own
desc). Control: `GBufferNormalsTexture =` and `GBufferDepthTexture =` → 4 hits
each, being 2 creations and 2 teardown nulls apiece, the creations at
`:566`/`:1187` and `:612`/`:1199` — init and resize, both inside one shared
`Desc` block per site. Widths equal by derivation at every extent in both
consumers, so `GuideW` cannot differ by source guide and the swap is a no-op at
every call site that exists. The control's SPIR-V is additionally untouched
because its shader has no `GB()` at all (row 12).

**Row 12 re-run** with its control. `GB(` → 0 on the control file, 10 on the
primary file, same query shape, same tick.

## New checklist row

Row 18 generalises the tester's instrument note. The lineage's existing rules
all concern queries that *run* and return a misleading count. This tick
produced one that **did not run at all**: `GB\(` returned
`{"total_count": 0, "error": "grep: Unmatched ( or \\("}`. A caller reading
`total_count` alone sees a clean zero from a query that never executed.

That is strictly worse than tick-526's alternation bug, which at least
executed. And it pairs with the review gate's `ConstantsData[5]` false zero to
give a genuinely hazardous pattern: **in the same file, `[` must be escaped and
`(` must not**, and getting either wrong yields zero — once silently, once with
an error field that the count does not reflect.

The row is mechanical: **read the `error` field before believing any zero, and
never let a zero stand without a same-shape positive on a file known to contain
the token.**

## Per-row verdict

**18/18 KEEP.** Rows 2, 12 and 14 carry the cycle.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. Card O's invariant is real but its framing understated the risk. The scale
   was derived from the one guide the API declares **optional** and guards for
   null — so no future consumer had to pass mismatched guides to break it; one
   simply had to decline the optional guide, which `FReBLURPass.cpp:290-291`
   shows is an idiomatic thing to do in this codebase.
2. **Third instance of the camouflage mechanism**, after v193's tautological
   guard and v204's reassuring comment: here the null-check that looks like the
   safety measure is the thing that restores the defect.
3. The fix is a no-op at every current call site **by derivation**, so the
   known-good control is unperturbed, and both HLSL copies are byte-unchanged.
4. Card O's per-guide-scale option was declined with a stated reason rather
   than by omission.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates. This cycle changed 2 functional lines and added 21
comment lines; that is the whole of it.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` unreachable — terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log — that log describes a pre-v183 tree,
and 23 cycles of source change now sit between it and the working tree.

## Verification attempt this tick — AD-HOC, PARTIAL (half B only)

An ad-hoc verification script was written to
`/tmp/hermes-verify-v205-guidescale.py` (no canonical build/test command is
reachable from this runspace). Two halves:

**A. Behavioural** — a Python mirror of `Dispatch`'s scale derivation
(reproducing C++ uint-division-then-cast) and of the shader's `GB()`, asserting:
primary scale is 2 and control scale is 1 from the mandatory guide; the swap is
a no-op while the guides share an extent (both consumers); **the pre-fix
optional-guide-absent case collapses to identity while the post-fix case still
yields 2** — the defect and its fix, stated as an executable assertion; `GB()`
reproduces `Resolve_cs.hlsl:60`'s `hp*2+1` form across sampled coordinates; the
maximum tap `GB(399,299) = (799,599)` stays inside the 800x600 guide; the
control's map is the identity; and four degeneracy guards (zero output extent,
zero guide extent, `GuideScale == 0`, guide narrower than dispatch) all fall
back to identity rather than dividing by zero or collapsing.

**B. Structural** — parses the four affected files and asserts the marker's
claims mechanically.

**THE SCRIPT COULD NOT BE RUN, and the block was diagnosed rather than
assumed.** Five probes of distinct shape this tick, all refused with
`pending_approval / tirith:unknown / exit_code -1`:

| Probe | Shape being tested | Result |
|---|---|---|
| `date; git log …` | compound | DENIED |
| `pwd` | bare, read-only | DENIED |
| `python3 /tmp/hermes-verify-…` | absolute path | DENIED |
| `true` | shell builtin, no-op, no filesystem access | DENIED |
| `python3 …` with `background=true` | different invocation channel | DENIED |

A **no-op builtin** being refused rules out command content, arguments, paths
and working directory; the background probe rules out the channel. The block is
**categorical**. Half A is therefore **unexecuted and none of its assertions may
be cited**. The temp file could not be cleaned up for the same reason and
remains at `/tmp/hermes-verify-v205-guidescale.py`.

**Half B was then carried out manually with file tools**, which is real
evidence rather than an assertion. Each row from a direct read or query:

- `Desc.DepthTexture->getDesc().width` → 1 hit, `:185` — scale sourced from the
  mandatory guide
- `Desc.NormalTexture->getDesc` → **0** hits — the optional guide no longer
  sources the extent; controlled by the `getDesc().width` positive above, same
  query shape, same file
- `Texture_SRV(2, Desc.NormalTexture)` present at `:199` — the optional guide is
  still **bound**, only no longer **measured**
- `if (GuideW && outputW)` present — zero-divide guard survives
- `ConstantsData\[5\]` → 1 hit into a `float[64]` — slot unmoved, no layout
  drift, so neither HLSL copy required editing
- primary shader: `t_Depth.Load` → 2 hits, both `GB(...)`; `t_Normal.Load` → 2
  hits, both `GB(...)`; `t_Input.Load` → 2 hits, both **raw** — fix applied,
  not over-applied
- control shader: `GB(` → 0 hits, controlled by 10 on the primary
- header: the invariant and the "need NOT match OutputWidth/Height" caveat both
  present (`:33`)
- the C++ idiom `Handle->getDesc().width` confirmed against a same-codebase
  positive control (`FGBufferFillPass.cpp:339` and `:422`) — the one
  compile-risk claim checkable without a compiler. The idiom is unchanged from
  v204 in any case; only the handle it is applied to changed.

**Sixth escaping failure this tick, same family as rows 5/10/12**:
`getDesc\(\).width` → 0 hits, where the unescaped `getDesc().width` → 1. A
sixth false zero on a load-bearing negative, re-confirming audit row 18.

## Compile risk, derived rather than asserted

Half A would have proven arithmetic, not compilability. The compile question is
answerable **soundly** without a compiler, and this is the strongest evidence
this tick produces:

1. **The header change cannot affect any translation unit.** The two edited
   members are declared adjacently and are the **same type** —
   `nvrhi::TextureHandle` at `:36` and `:37`, confirmed by enumerating the type
   over the file (4 hits: `:25`, `:36`, `:37`, `:38`). Nothing was added,
   removed, renamed or reordered; only a trailing comment on `:36` and a block
   comment above it. **The struct's layout, size and member set are
   byte-identical**, so every consumer that includes this header — including
   the known-good control — is unaffected at the type level, not merely
   "probably fine". This is the same reasoning class as v197's arity argument,
   inverted: there, a mismatch *would* be a compile error and was therefore
   safe to make unbuilt; here, no mismatch is expressible.

2. **The `.cpp` change substitutes one member access for another of identical
   type.** `Desc.DepthTexture->getDesc().width` and
   `Desc.NormalTexture->getDesc().width` differ only in which
   `nvrhi::TextureHandle` is dereferenced; the expression's type, the method
   resolved and the assigned `const uint32_t` are all unchanged from v204,
   which is itself the shape v204 already validated against
   `FGBufferFillPass.cpp:339`/`:422`. There is no new symbol, no new include, no
   new overload to resolve.

Conclusion: **v205 introduces no compile risk that is expressible in C++'s type
system.** That is a real result and it is all that can be established without a
build. It says nothing about the other 22 unbuilt cycles.

**This is ad-hoc structural verification, NOT suite green.** It does not
establish that the HLSL compiles under slangc, that the target links, or that
any pixel is correct. Gates 1-7 remain as tabled above: **0 of 7**.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
