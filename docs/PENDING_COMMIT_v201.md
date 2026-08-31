# Pending Commit v201

- plan: docs/PENDING_PLAN_v201.md
- files: none — zero source files modified
- source: no bundle
- target: no branch (no commit made, per job instruction)
- task: run v198's set-difference procedure against the primary target
- verify: see the query table below; every zero has a same-shape positive
- skip_impl_review: no
- produces_test_files: no
- notes: the answer is the plan-criticer's second branch — **structurally
  immune, not merely clean**. Those are different findings and the distinction
  is load-bearing. See below.

## Finding 1 — the set difference is VACUOUS, and that is the result

v198 found the tenth instance by partitioning creation sites against a resize
branch: `TestCornellBoxGI.cpp` recreates nine textures on resize and leaves
fourteen behind. The defect existed as the *gap* between those two sets.

The primary target has no such gap because **it has no recreation set at all**:

    virtual void BackBufferResizing() override
    {
        BindingCache.Clear();
    }
    // TestReSTIR_GI_Temporal.cpp:1385-1388 — the entire override, read in full

Four lines. No `createTexture`, no `CreateGBufferTextures()` call, nothing
recreated. So the partition v198 performed cannot be performed here: one side of
the difference is empty.

**Why this is immunity rather than luck.** All 16 `CreateTexture2D` call sites
in the file resolve to fixed constants — `:1655`-`:1717`, every one inside
`CreateGBufferTextures()`, whose first line is `const uint32_t W = WIDTH, H =
HEIGHT;` (`:1616`) — and the half-res set derives from the same constant
(`const uint32_t HalfW = W / 2;`, `:1671`). Nothing in the file is sized from
the swapchain, so there is nothing a resize *could* invalidate. The control's
defect required extent-sized resources to exist; here none do.

This also explains why nine substitution cycles were the right method for this
file and the wrong method for the control: the primary target's defects were all
*wrong-operand* defects, which grep can express. The control's was a *lifetime*
defect, which grep cannot. The two files needed different procedures, and each
received the one it needed — but only by accident, since nobody had checked.

## Finding 2 — the cumulative `FB.width` union, never run before

v191-v195 each deliberately declined to bundle so their own enumerations stayed
verifiable. The consequence nobody stated: each cycle verified only its own
site, and the **union was never taken**. Taken now.

`FB.width` → 13 hits; `FB.height` → 7. Classified exhaustively:

| Lines | Kind | Correct? |
|---|---|---|
| `:764`, `:885`, `:1046`, `:1051`, `:1107`, `:1135`, `:1204`, `:1276`, `:2171`, `:2360` | comments recording prior fixes | n/a — not code |
| `:754`, `:756`, `:757` | resize **detection** state (`LastWidth`/`LastHeight`) | **yes** — must follow the swapchain; this is what detects the resize |
| `:1326` | blit **destination** extent | **yes** — the one quantity that genuinely should follow the window |

**Three live sites, all three correct.** Ten of thirteen hits are v183-v197's own
comments, which is why a naive count reads alarmingly high. Per v200's rule
(a count answers existence, never kind) I read every hit rather than counting.

## Finding 3 — dispatch grids agree with their UAVs

`+ 7) / 8` → 10 hits; the two live dispatches are `:1178` and `:1312`, both
`(WIDTH + 7) / 8, (HEIGHT + 7) / 8`, both against fixed-extent UAVs (v192, v193).
No swapchain-derived grid remains.

## Plan Deviations

One, and it is the plan-criticer's instruction rather than a departure from the
plan: the plan anticipated finding-or-not-finding an instance; the gate required
distinguishing "clean" from "structurally immune." The marker reports the latter
explicitly, because "clean" would wrongly imply the check could regress. It
cannot regress without someone first adding an extent-sized resource.

## New tooling failure — a search that ERRORS rather than lying

`search_files pattern="dispatch\("` returned
`Search failed: grep: Unmatched ( or \(` — the backslash escape is
double-processed, so the standard escaping of a literal paren is rejected.

This is the ninth false-instrument mechanism and the first that **fails loudly**.
The lineage's eight prior mechanisms all returned a plausible wrong answer
(tick-526's false zeros, v192's false failures, v200's correct-count-wrong-kind).
An error is strictly better: it cannot be mistaken for evidence. Recorded so the
next cycle uses the plain substring form (`CmdList->dispatch`) instead.
