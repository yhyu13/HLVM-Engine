# Pending Commit v199

- plan: docs/PENDING_PLAN_v199.md
- files: **none — zero source files modified**
- source: no bundle — direct source read
- target: (no branch; determination-only cycle)
- task: Apply v198's set-difference procedure to card L's two deferred sibling targets
- verify: `search_files path=Engine/Source/Runtime/Test/TestRTReflections.cpp pattern=createTexture`
  → 15 hits; partition against the resize block bounds read at `:892` and `:984`
- skip_impl_review: no
- produces_test_files: no
- notes: Third zero-change cycle in the lineage (v196 determination, v198 determination +
  card, v199 determination). The deliverable is a determination that closes card L's
  second half.

## Determination: BOTH TARGETS CLEAN. No eleventh instance.

Stated plainly because the plan pre-committed to saying so: the sweep found nothing, and
that is the finding.

### Target 1 — `TestRTReflections.cpp`: clean, and it is the *positive control* the class never had

Set difference done by hand, not by query, per v198's checklist row.

**Every extent-sized creation site in the file (complete enumeration):**

| Site | Init block | Resize block (`:892-984`) | Contained? |
|---|---|---|---|
| GBufferDiffuse | `:407` | `:912` | yes |
| GBufferSpecular | `:422` | `:916` | yes |
| GBufferNormals | `:437` | `:919` | yes |
| GBufferEmissive | `:452` | `:922` | yes |
| GBufferWorldPos | `:467` | `:925` | yes |
| GBufferDepth | `:483` | `:931` | yes |
| GBufferFramebuffer | `:513` | `:940` | yes |
| GBufferPipeline | (init) | `:955` | yes |
| HDRTexture (UAV) | `:783` | `:965` | yes |
| StagingTexture | `:815` | `:980` | yes |
| PlaceholderTexture | `:661` | — | **not extent-sized** (1x1, `:318-319` shape) |

Ten extent-sized resources, ten recreations. The eleventh creation site is a 1x1
placeholder and is correctly excluded — it is the one site whose absence from the resize
block is right.

**And the extents agree at both ends**: init reads `Framebuffer->getFramebufferInfo()`
(`:392-393`), resize reads `CurrentFBInfo` (`:898-899`), dispatch reads
`args.width = CurrentFBInfo.width` (`:1145-1146`). One extent source throughout, and
every resource it sizes is recreated on the event that moves it. `BackBufferResizing`
(`:1236-1249`) additionally nulls all ten before the recreation runs.

**This is the substantive finding of the cycle.** Card L asserted this target "has the
same resize-block shape" as the defective one. It has the same *syntactic* shape and the
**opposite semantic content**: `TestCornellBoxGI` recreates nine of twenty-three, this
recreates ten of ten. So the lineage now has a **worked positive control for the
set-difference procedure** — v198 demonstrated the query shape finding a defect, and this
demonstrates it returning clean on a file that superficially matches the defect pattern.
A procedure that has only ever been run where it fires is not yet known to discriminate.

### Target 2 — `TestRenderSponza.cpp`: clean, and card L mis-targeted it

Per the plan gate's requirement to distinguish the two ways a set difference can be empty:
**this target's set difference is empty because there is nothing extent-sized to contain**,
which is a different finding from target 1's.

- `createTexture` → **1 hit**, `:327`, the 1x1 `PlaceholderTexture` (`:318-319`
  `Desc.width = 1; Desc.height = 1;`). Not extent-sized.
- `createStagingTexture` → **0 hits**. `createFramebuffer` → **0 hits**.
- `dispatch` → **0 hits**. There is no compute or RT dispatch anywhere in the file, so
  the "swapchain-sized grid over startup-sized UAV" failure mode has no site to occur at.
- The block card L cited (`:413-416`) recreates a **graphics pipeline**, not textures, and
  `BackBufferResizing` (`:549-553`) nulls the pipeline and clears the binding cache.

So the card's citation was structurally wrong: it pattern-matched a resize block and
assumed extent-sized resources behind it. **A forward raster target with no UAVs and no
dispatch cannot host this defect class at all.** Clean, vacuously, and the vacuity is the
point — recorded so a future cycle does not re-sweep it.

## Plan Deviations

**One, and it widens scope by one file.** The plan scoped the cycle to card L's two named
targets. While closing target 1's enumeration I checked whether the file's near-twin
sibling shares its shape, because two files in the tree carry the same six-MRT GBuffer +
HDR + staging layout and the card named only one:

- `TestRTShadowsGBuffer.cpp` — init `:295-371`/`:671`/`:703`, resize block `:801-872`,
  `BackBufferResizing` `:1123-1136`. Same ten-for-ten containment: GBuffer MRTs `:801-820`,
  HDR `:854`, staging `:869`. **Also clean.**

Justification: this is a read, it modified nothing, and excluding it would have left the
class's most defect-prone shape (RT dispatch over UAV, the exact shape that failed in
`TestCornellBoxGI`) with one unexamined member while the cycle claimed to have swept the
class. The reviewer should judge whether this is scope creep; my position is that a
determination cycle's value is the completeness of its enumeration, and stopping at the
card's two names would have made the cycle's own claim weaker than the work supported.

## What was NOT done

Not built, not run, not compiled, not viewed. Terminal denied categorically this tick
(a bare `true` refused, `tirith:unknown`). No runtime result claimed. No commit, no push,
no governance file touched.
