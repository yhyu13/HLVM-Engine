# Pending Commit v209

- plan: docs/PENDING_PLAN_v209.md
- files: Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h,
  Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
- source: no bundle
- target: (uncommitted working tree — this pipeline does not commit)
- task: Card R — delete `FGIPass::DummyDirectionTexture`, dead since v207
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: **-2 functional, +2 comment**, two files. No shader touched (v182
  dual-copy hazard not engaged), no cbuffer touched (v184 silent-layout class
  not engaged), no signature changed (unlike v197).

## What changed

| File | Line (pre) | Change |
|---|---|---|
| `FGIPass.h` | `:140` | member declaration deleted; 6-line comment records why, and why the sibling is NOT symmetric |
| `FGIPass.cpp` | `:192` | `Shutdown()` null-out deleted |

## Verification — each zero controlled

`search_files` on `Engine/Source/Runtime`, one term per query, `path` at a
directory (v199 row), no ERE metacharacters (v208 row):

| Query | Pre | Post | Reading |
|---|---|---|---|
| `DummyDirectionTexture` | 2 | **0** | target gone |
| `DummyDebugStatsTexture` | 5 | **5** | live sibling untouched — the control that makes the 0 meaningful |
| `MaterialPlaceholderTexture` | 6 | **6** | adjacent line on BOTH edits, unharmed |

The two positives are the point. A `0` on its own would be indistinguishable
from a timed-out query — and this cycle **actually saw that failure**: two
whole-tree sweeps returned `total_count: 0, truncated: true,
limit_reason: search_timeout`. Same tool, same term, a scope that could not
complete. Both positives above ran in the same scope as the zero and returned
non-zero, so that scope completed.

## Both returned diffs were read before proceeding (v203 rule)

The realistic failure of a two-line deletion is an anchor that swallows a
neighbour, and here **both deletion targets are directly adjacent to a live
member on both sides** — `DummyDebugStatsTexture` above,
`MaterialPlaceholderTexture` below, in both files. Both diffs show exactly one
`-` line. Post-patch counts confirm independently.

## Plan Deviations

**One, and it is additive.** The plan said "drop the member, drop the
null-out" — 2 deleted lines, no additions. I added a 2-line comment in the
header at the deletion site.

**Why:** the surviving sibling one line above is a visually identical
`nvrhi::TextureHandle Dummy*Texture` declaration. Deleting the direction one
silently leaves a reader with a single "Dummy" handle and no indication that a
second one was ever considered — which invites a future cycle to "restore
symmetry" by giving u2 a 1x1 dummy again. That is precisely the out-of-bounds
UAV store v207 fixed. The comment forecloses that.

**Kept to 2 lines, not 6 (post-review correction).** The first draft restated
the u1-vs-u2 mechanism in full. Reading `FGIPass.h` end-to-end — rather than
the paginated windows the cycle had used — showed lines 37-49 already carry
that as a formal `UAV EXTENT CONTRACT` block, including the u1 exemption and
the writes-are-pinned/reads-are-scaled rule. **The deviation was a DRY
violation against documentation 100 lines above it in the same file**, and a
duplicated invariant is one that can later disagree with itself. Now a
one-line pointer plus the prohibition. Net comment cost: **+2, not +6.**

**Impact on acceptance criteria:** none. Comment lines are not code; the
plan's four acceptance rows are unaffected and all four pass.
