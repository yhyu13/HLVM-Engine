# Pending Test Audit v213

- tests: docs/PENDING_TESTS_v213.md
- commit: docs/PENDING_COMMIT_v213.md
- verdict: **ALL_KEEP**
- verifier: agent_6_testing_verifier (tick-559)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — re-ran rows 1, 2, 6 and 12 myself, from different queries
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
- [x] No cardinality claim inherited across cycles without re-derivation (v211)
- [x] No domain treated as swept without partitioning the enumeration (v212)
- [x] **No control-flow edit accepted without closing the function's exit set (v213, new — row 22)**

## Row 22 — adopted, and it is what this cycle's verification was missing

Every prior row governs *queries*. This cycle added a `return` — the first new
control flow in several cycles — and neither the tester nor the reviewer closed
the **exit set**. They verified the guard is positioned before the upload
(true), but "the guard is in the right place" and "no *other* exit was
introduced or displaced" are different claims, and only the first was checked.

> *When a patch adds or moves a `return`/`break`/`continue`, re-derive the
> enclosing function's complete exit set and account for every member. Position
> of the new exit is necessary and not sufficient.*

**I ran it**: `        return;` → **exactly 3** in the file, at `:138`
(null CmdList/Pipeline/ConstantBuffer), `:152` (invalid dimensions), `:174`
(the new guard). Two pre-existed, one was added, none displaced, and `Dispatch`
falls through to `dispatch()` on every other path. The exit set is closed.

This is the row-21 lesson (a set, not a count) applied to control flow rather
than to a domain.

## Independent re-derivation of the carrying rows

**Row 12 (zero cbuffer change) closed from a direction the tester did not use.**
The tester reasoned from `[5]` being "still the last assigned slot". I enumerated
instead: `ConstantsData\[` → **7 hits** — the declaration `[64]` at `:178`, then
`[0] [1] [2] [3] [4]` at `:180-184` and `[5]` at `:215`. A contiguous 0-5 with
no gaps and no `[6]`/`[7]`. That is the whole set, not its last member, so a
slot inserted *in the middle* — the failure mode that actually breaks HLSL
packing (v184/v200) — is excluded. The tester's form would not have excluded it.

**Row 6 re-queried**: `GuideScale = static_cast` → 1 hit at `:213`, inside
`if (Desc.DepthTexture)` at `:210`, assigned to `ConstantsData[5]` at `:215`.
v205's fix is intact. This was the cycle's real hazard — the patch rewrote the
comment block immediately above that branch, which is precisely v203's
near-miss geometry — and it is clean.

**Row 2 confirmed by the exit set above** rather than by reading line order.

## The tester's limitation #3 is the most honest line in the cycle

*"The new branch is never taken by any code in this repository. Its correctness
is by inspection only, and its log message has never been emitted."*

That is exactly right and I want it on the record rather than smoothed over.
This cycle adds an unreachable-in-practice branch. Its value is entirely
contractual: it converts an affordance the header advertised and nothing could
honour into an explicit, attributable rejection. **If a future cycle "verifies"
this guard by observing that tests still pass, it has verified nothing** — the
tests cannot reach it.

## Ruling on the reviewer's forwarded narrowing

The reviewer flagged "all three copies of BilateralDenoise_cs.hlsl" as a
cardinality written into the tree, declined to require a change, and asked the
audit to rule. **I uphold the reviewer's own stated view: state the property,
not the census** — but I do **not** require the edit this cycle.

Reason: the sentence's load is carried by "with no gate", which stays true for
any number of copies; the count is corroborating detail in a comment that also
names the file, so a reader who finds a fourth copy is directed to it rather
than misled. Editing it now would mean a third pass over a file whose diff is
already the thing under review, for a sentence that is true. **Recorded as a
standing preference, not a defect.**

## Per-row verdict

**13/13 KEEP.** Rows 6, 9 and 13 carry the cycle:

- **Row 6** because disturbing v205's `GuideScale` derivation is the only way
  this cycle could have caused real harm, and it would have been invisible —
  a wrong scale produces wrong bilateral weights with no VUID, the exact
  signature this lineage has chased for thirty cycles.
- **Row 9** because if either consumer did *not* set the guide, this patch would
  convert a working test into an early-out — turning a latent contract fix into
  a live regression on the acceptance path.
- **Row 13** because it is the claim that makes "latent" true rather than
  asserted.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `FBilateralDenoisePass::FDesc::NormalTexture` was documented **optional** and
   is **required** at all four levels that could honour the word — layout item,
   binding set, and all three shader copies' ungated `Load`s. Corrected at every
   site, plus the two rationales that rested on the false claim.
2. The optional-resource domain is **enumerated and partitioned**: 4 sites,
   3 with fallbacks (`FGIPass` u1 dummy, `FGIPass` u2 → `OutputTexture` per
   v207, `FReBLURPass` `EnsureDummyTexture`), **1 with none**. This class was
   the only one promising an affordance it could not honour.
3. **Fourth instance of the camouflage mechanism** (after v193's tautological
   guard, v204's reassuring comment, v205's optional-guide branch): v205
   reasoned *about* the optionality and, in fixing `GuideScale`, removed the
   class's last `if (Desc.NormalTexture)` while writing the false premise into
   the tree twice more.
4. The remedy chosen was to **withdraw the promise, not to fake it** — a dummy
   normal would have made the word true while degrading the filter to
   depth-only with no diagnostic.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates.

**Severity, without inflation: LATENT. This cycle moves no pixel and clears no
acceptance gate.** Both live consumers pass a real guide, so runtime behaviour
is byte-identical.

## Acceptance gates vs the job instruction: 0 of 7

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` unreachable — terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**Two orthogonal structural blockers**: (a) `terminal` refused at the tool
boundary, probed twice first-hand this tick including on a bare `echo hi`;
(b) no vision/image tool exists in this runspace at all, so gate 6 is
unreachable even with full shell.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
