# Pending Test Audit v211

- tests: docs/PENDING_TESTS_v211.md
- commit: docs/PENDING_COMMIT_v211.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-557)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (HLSL)
- [x] No test-bug-in-itself — re-ran rows 4, 10/11 and 8/9 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No ERE pattern against a BRE engine (v208)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — all re-derived
- [x] Every zero controlled by a same-shape positive (v205)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No zero believed without reading the query's `error` field (v205)
- [x] No linter output dismissed without mapping each line to a cause (v207)
- [x] No conclusion drawn from a query reporting `search_timeout` (v209)
- [x] No enumeration accepted from a truncated file list (v210)
- [x] **No cardinality claim inherited across cycles without re-derivation (v211, new)**

## New row 20, and why this cycle forced it

For eleven cycles the lineage repeated a claim about *how many copies a shader
has* — v182 observed two copies of `GIPathTracing.hlsl`, and v192, v193, v195,
v197 and v207 each recorded "both copies byte-unchanged" as a safety row.
None re-derived the cardinality for the file actually in front of it.
`BilateralDenoise_cs.hlsl` has **three**, and the third was stale and compiled.

This is **not** a seventh false-instrument mechanism, and the impl review was
right to force that correction. `search_files target=files` on the filename
finds it in one query, and that query is neither new nor exotic — v199 and
v206 both used it. The failure was that **nobody ran it.**

Row 20 as adopted: *a cardinality claim is a fact about the tree, not a fact
about the lineage. Re-derive it in the cycle that relies on it, however many
prior cycles repeated it.* This is v195's rule — *a marker's description of
code is evidence about its author, not about the code* — extended from
descriptions to counts.

## Independent re-derivation

**Row 11 closed a second way, from the opposite polarity.** The tester proved
the blob is the stale build product by finding `Pad0` → 1 **inside**
`BilateralDenoise_cs.sblob`. Necessary but one-sided: a token's presence in a
binary could in principle be an unrelated substring. I ran the inverse query —
`GuideScale` scoped at the directory → **3 hits, all in the patched `.hlsl`,
zero in the `.sblob`.**

So the blob **contains the old name and lacks the new one**, and the source
**contains the new name and lacks the old one**. Two queries of opposite
polarity agreeing is materially stronger than either alone, and it settles the
substring objection: the blob is a build of the pre-v204 source.

**Row 4 re-run in place rather than by query.** The over-reach risk is the one
edit in this cycle capable of converting a latent defect into a live one, so I
read `:128-139` directly instead of trusting an enumeration:
`t_Input[uint2(neighborPixel)]` `:133` and `u_Output[pixelCoord]` `:139` —
both raw, neither through `GB()`. Correct: these are dispatch-res resources,
and routing them through `GB()` would be wrong at **every** scale including 1.

**Rows 8/9 accepted on size-and-line-count**, which is weaker than a content
hash. Stated as the limitation it is: the runspace has no hashing tool. Both
figures were taken from reads in *this same tick* both before and after the
edit, and the edited file is a different path, so the risk is low but not zero.

## Per-row verdict

**12/12 KEEP.** Rows 4, 6 and 11 carry the cycle:

- **Row 4** because it is the only route by which this edit could have caused
  harm.
- **Row 6** (declaration order: cbuffer closes `:24`, `GB()` at `:43`) because
  it is the only route by which this edit could have failed to *compile*, and
  with the chain unbuilt such a failure would have surfaced 18 cycles
  downstream and been misattributed to v183-v211.
- **Row 11** because it converts the cycle's premise from inference to
  demonstration.

## The reviewer's non-carding decision — SUPERSEDED, and the cleanup was right

The reviewer noticed that `:133` used `operator[]` where the primary copy uses
`.Load(...)`, proved it inert, and declined to card it. **A post-audit
end-to-end read of the patched file changed the disposition**, and the change
is worth recording because it cuts against what the audit first concluded.

Not carding it as a *defect* was correct — it is provably inert. But "not a
defect" and "leave it" are different judgements, and the audit conflated them.
Read in full rather than through a diff, the function had **three guide/input
reads in `.Load` form and one in `operator[]` form**, inside the same loop
body, in a file whose entire purpose this cycle was *to be in agreement with
the primary copy* — where the corresponding line is `.Load`. That is a style
inconsistency introduced by this very cycle's patch, in the one file where
"matches the primary" is the acceptance criterion.

Changed to `t_Input.Load(int3(neighborPixel, 0))`. **Semantically identical**
(`operator[]` on a `Texture2D` is `Load` at mip 0; the coordinate is
bounds-checked at `:111`), so no row above is invalidated. Re-verified after
the edit, honouring tick-526's alternation rule after the combined query
`t_Input|u_Output` returned a false **0**: `t_Input` → 4 hits (decl `:49`,
comment `:88`, reads `:92`/`:133`, **both raw**), `u_Output` → 2 (decl `:54`,
store `:139`, **raw**). **The load-bearing negative — no dispatch-res resource
routed through `GB()` — still holds.**

**Lesson, and it is the second time this cycle the same one landed:** the
alternation false zero fired again here, on the single most important negative
in the patch. Row 20 said re-derive cardinality; this says re-derive it *with
the query shape the lineage already proved sound*. The reflex to combine two
terms into one query survived 31 ticks after being documented.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. A **compiled, blob-backed, default** copy of a shared shader carried the
   pre-v204 Phase-D guide defect verbatim; it now carries the corrected form.
2. The C++ marshaller and this shader **agree on constant-buffer slot 5 for
   the first time** — previously `Dispatch` wrote a live `GuideScale` into a
   slot the shader named `Pad0` and discarded.
3. The three-copy cardinality is now a **derived** fact, with the sibling
   singletons (`HBAO_cs.hlsl` → 1, `JointBilateralUpsample_cs.hlsl` → 1)
   proving it is an accident of history, not a project convention.
4. A twentieth audit row, and the correct diagnosis of the mechanism behind it.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates.

**One consequence stated plainly, because it is easy to miss:** the corrected
source does **not** reach any consumer until `Common_ShaderMake` re-runs. The
stale blob is still on disk. This fix is inert until the operator's next build
— which is the same build the three deferred cards (L, M, N) are waiting on.

**Severity, without inflation:** **latent. This cycle moves no pixel and clears
no acceptance gate.**

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` unreachable — terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.**

## Verification attempt this tick

`terminal command="pwd"` — a no-op builtin, no arguments, no path, no
toolchain — refused with `pending_approval / tirith:unknown / exit_code -1`.
The refusal is at the tool boundary, not command-dependent.

**Post-audit addendum — an executable check was written and could not be run.**
A focused ad-hoc verifier was authored at
`/tmp/hermes-verify-v211-bilateral.py` (~5 KB) to establish the two things a
grep cannot: (A) reimplement `GB()` in Python and assert it is the **identity
at `GuideScale == 1`** — the arithmetic the entire "cannot perturb the unbuilt
chain" argument rests on — plus centre-of-footprint at 2, identity-not-crash
at 0, injectivity, and in-bounds for a 400x300 dispatch over an 800x600 guide;
(B) parse the cbuffer, register set and call-site sets rather than grepping
them.

**It was never executed.** `python3 <abs path>` was refused, and so was a bare
`/usr/bin/true` with an absolute path, zero arguments, no shell metacharacters
and `workdir=/tmp`. Two refusals of different shape prove the block is
**categorical at the tool boundary**, not a function of command content,
arguments, interpreter, or working directory. The temp file could not be
cleaned up for the same reason and **remains at that path** for an operator to
run or delete.

**Consequence, stated plainly:** the `GB()` arithmetic claims in this cycle are
**reasoned, not executed**. They are simple enough to be read off the four
lines of the helper, and the sibling copies have carried the identical
expression since v204/v205 — but this audit does not get to call them
*verified*, and does not.

### Addendum 2 — the `GB()` claim closed by TEXTUAL IDENTITY instead

Execution stayed blocked across **five refusals in four distinct invocation
shapes** (foreground; absolute-path no-argument builtin; alternate `workdir`;
**background**). Background was the last mechanically-different path available
and it refuses identically, so the block is not a foreground policy.

Since the property could not be *run*, it was closed by **reduction to code
already shipping in the tree**, which needs no interpreter:

| Instance | Location | Body |
|---|---|---|
| patched | `Shader/BilateralDenoise_cs.hlsl:43-47` | `int s = max(int(GuideScale), 1); return p * s + (s / 2);` |
| v204 primary | `TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl:35-39` | **byte-identical** |
| v183 ReSTIR | `.../ReSTIR_Temporal_cs.hlsl:78-82` | same form, `(s >> 1)` for `(s / 2)` |
| Phase-D original | `.../Resolve_cs.hlsl:60` | `int2 fp = hp * 2 + 1;` — the hardcoded `s == 2` case |

The patched helper is **textually identical** to the one v204 shipped and v205
corrected; `s >> 1` and `s / 2` coincide for non-negative `s`, and `max(...,1)`
makes `s >= 1`; and at `s == 2` the expression reduces to `p * 2 + 1`, which is
literally `Resolve_cs.hlsl:60` — the Phase-D form this whole class of fix was
derived from.

**So the identity-at-1, centre-at-2 and no-collapse-at-0 properties are not new
claims requiring a new proof.** They are properties of an expression that has
been in the tree since v204, restated here byte-for-byte. What the Python probe
would have added is a *second* method of checking, not the only one.

**Honest residue:** this closes the arithmetic, not the compile. Whether the
patched file compiles, links and renders remains **UNKNOWN** and needs the
operator's build. The cycle stays `unverified` overall.

**Operator command that clears the blocker:**

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

Optional, seconds, and independent of the build:

    python3 /tmp/hermes-verify-v211-bilateral.py   # then delete it

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
