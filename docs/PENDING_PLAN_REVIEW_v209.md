# Pending Plan Review v209

- plan: docs/PENDING_PLAN_v209.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-555)
- timestamp: 2026-08-30

## Design soundness

The plan solves the stated problem and, unusually for this lineage, its
central argument is **structural rather than enumerative**. Every prior cycle
that claimed "nothing else references X" rested on a grep count, and this
lineage has catalogued four mechanisms by which such a count can be wrong
(BRE-vs-ERE metacharacters at v208, `path`-at-a-file at v199, comment hits at
v200, convenience wrappers at v201) plus a fifth that appeared **in this very
cycle** — the whole-tree query returning `total_count: 0` with
`limit_reason: search_timeout`. The plan noticed that zero was an artefact and
did not use it. That is the v205 row applied correctly under live conditions.

**I re-derived the access-control claim myself rather than accepting it:**

- `class FGIPass` opens at `FGIPass.h:95`, `public:` at `:97`.
- `private:` → **exactly 1 hit** in the directory, at `:116`.
- The member is at `:140`, the class closes at `:145`. So it is inside the
  one private section, and there is no later `public:` to reopen access.
- `friend` → **0 hits** in that directory. This zero is *controlled*: the
  same directory, same tool, answered `private:` with 1 hit in the same
  breath, so the query completed rather than timing out.

Therefore no translation unit other than `FGIPass.cpp` can legally name the
member, and the deletion cannot break a caller that a grep failed to find.
This is strictly stronger than the enumerations that closed v191-v207.

## Plan completeness

Complete. The two edit sites are named with line numbers and both were
verified in place. The risk section names the one genuinely dangerous move
(deleting the live sibling) and the plan's own evidence disproves the
symmetry: `DummyDebugStatsTexture` has an assignment at `:625` and a live
null-test at `:614`, `DummyDirectionTexture` has neither.

One addition I required and the plan already satisfies: the acceptance rows
must include `MaterialPlaceholderTexture`. It is declared at `FGIPass.h:141`,
**the line immediately after the deletion target**, and nulled at
`FGIPass.cpp:193`, immediately after the other. The realistic failure of a
two-line deletion is an `old_string` that swallows an adjacent line — which is
exactly the near-miss v203 produced when its anchor matched into a neighbouring
braced list. The plan carries that row.

## What I checked and found nothing wrong with

- No shader file is touched, so the v182 dual-copy hazard is not engaged.
- No cbuffer is touched, so the silent v184 layout class is not engaged.
- No signature changes, unlike v197 — this is a member deletion, and any
  missed reference is a **compile error**, not a silent miscompile. Per v197's
  own reasoning, that makes it among the safest changes to make while the
  v183-v208 chain is unbuilt: the failure mode is loud and immediate.

## Feedback for planner (FIX only)

None. KEEP.
