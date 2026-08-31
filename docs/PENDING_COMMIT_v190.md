# Pending Commit v190

- plan: docs/PENDING_PLAN_v190.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (no branch — working tree only; job instruction forbids commit/push)
- task: card D — correct the bilateral-denoise rationale comment; do NOT apply the card's remedy
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- notes: **comment-only change.** Zero functional lines touched.

## Change applied

Two hunks, both comment-only, in the bilateral denoise block.

**Hunk 1** — replaced the 8-line block comment above `if (!bBypass)` with a
19-line one. The old text asserted the false mechanism ("its *execution* forces
nvrhi to emit the pending layout transitions"). The new text states the real one
(`setComputeState` calls `commitBarriers`; `dispatch` does not), notes that every
consuming pass issues its own `setComputeState`, distinguishes the genuinely-
required manual flush before `ReBLURPass::Dispatch`, and records that deletion is
gated on absence-evidence.

**Hunk 2** — cut 7 lines from the v189 comment inside the block. Once hunk 1
states grid-independence and the scope limits, the v189 text restated both
verbatim. Replaced with two lines pointing at the block comment. Net effect is
that each fact is stated once.

**Hunk 3 (post-review tightening)** — condensed hunk 1 from 31 lines to 19,
dropping blank `//` separators and redundant phrasing while keeping every fact.
Matches the density of the surrounding comment blocks; same content, less prose.

Functional diff: **+0 / -0**. File 2986 → 2992 lines (+6, all comment).

## Verified after applying (queries actually run, not asserted)

Per the v188 lesson — an impler asserting a query result must have run that exact
query — every row below is a query I executed this turn with its real result.

| # | Check | Query / read | Result |
|---|---|---|---|
| 1 | No functional line moved | `search_files pattern="Bd\."` | **9 hits**, `:871-874` and `:882-886`. Same nine fields, same nine values as pre-edit (`InputTexture`/`DepthTexture`/`NormalTexture`/`OutputTexture`/`OutputWidth=HalfResWidth`/`OutputHeight=HalfResHeight`/`0.05f`/`0.5f`/`4.0f`) |
| 2 | v189's fix survived intact | read `:882-883` | `Bd.OutputWidth = HalfResWidth; Bd.OutputHeight = HalfResHeight;` — unchanged |
| 3 | Guard unchanged | read `:868` | `if (!bBypass)` still immediately precedes the block |
| 4 | Dispatch call unchanged | read `:887` | `BilateralDenoisePass.Dispatch(CommandList, Bd);` |
| 5 | **No stale line-number cross-refs introduced** | `search_files pattern="// .*:[0-9][0-9]+"` over the whole file | **0 hits.** The constraint from the plan-review is satisfied file-wide, not just in my hunks |
| 6 | Planner's `:1111` NOT propagated | (consequence of row 5) | the wrong citation the plan-criticer caught does not appear in source |
| 7 | nvrhi claim is quotable as written | read `vulkan-compute.cpp:112-152` and `:166-173` | `setComputeState` ends `commitBarriers();` at `:145`; `dispatch` body is `assert` + `updateComputeVolatileBuffers()` + `cmdBuf.dispatch()`, complete, no barrier call |
| 8 | Generation flushes before temporal binds | read `FReSTIRPass.cpp:400` vs `:481`/`:489` | `setComputeState` in `DispatchGeneration` precedes temporal's two `createBindingSet` calls |
| 9 | Contrast case is real | read the `commitBarriers()` site before `ReBLURPass.Dispatch` | its own comment describes the intra-`setComputeState` hazard — distinct from this block, as the new comment says |
| 10 | No shader touched | only edited file is the `.cpp` | the v182 dead-shader-copy trap cannot fire |
| 11 | Cornell untouched | not edited this cycle | — |

Rows 5, 7 and 8 are the discriminators; each could independently have failed.

## Plan Deviations

**One, additive and within the plan-criticer's constraint.**

The plan specified hunk 1 only. I also cut 7 lines from the v189 comment (hunk
2) because hunk 1 made them a verbatim duplicate — the same "grid-independence"
and "not a full fix" statements appearing twice, twelve lines apart. Leaving
both would mean a future correction has two places to miss.

This is still comment-only, so the binding constraint ("any functional edit is
an automatic FIX") is not engaged. Flagging it explicitly because it was not in
the plan text.

## What the reviewer must check

1. That the change really is inert. Row 1 is the load-bearing claim: nine `Bd.`
   assignments, unchanged values. If any differs, this is a FIX.
2. That the new comment does not overstate. It says deletion is *gated on
   absence-evidence*, not that deletion is safe. If it reads as licence to
   delete the pass without a run, that is a FIX.
3. That row 5's query is the right shape — it must catch `:NNN` inside comments
   anywhere in the file, not only in my hunks.

## Verification attempt (ad-hoc, post-cycle)

Wrote `/tmp/hermes-verify-v190.py` — 9 assertion groups. **Could not execute it.**
Four `terminal` invocations across two distinct modes (foreground and
`background=true`), all `status: pending_approval / pattern_key: tirith:unknown /
exit_code: -1 / smart_denied: false`: a compound `echo probe-ok; date -u; pwd;
nvidia-smi ...`; a bare `/bin/true` (absolute path, no arguments, no shell
metacharacters, `workdir=/tmp`); `python3 /tmp/hermes-verify-v190.py`; and the
same under background dispatch. The minimal probe rules out command-pattern
matching and the background attempt rules out a foreground-only policy — **the
block is on the tool.** Runtime fired `same_tool_failure_warning; count=4`, so I
stopped retrying.

**Fallback: hand-executed all 9 groups via `read_file`/`search_files`. 9/9 hold**
against the final (post-tightening) file:

| # | Assertion | Result |
|---|---|---|
| 1 | 9 `Bd.` fields, exact v189 values | `:859-862`, `:870-874` |
| 2 | No `Bd.` field carries a full-res extent | only `FB.width` in-block is `:863`, a comment |
| 3 | Dispatch once, inside `if (!bBypass)` | `:875`, guard `:856` |
| 4 | `setComputeState` flushes, `dispatch` does not | `vulkan-compute.cpp:145` vs `:166-173` |
| 5 | No `:NNN` cross-refs in comments, file-wide | 0 hits |
| 6 | `AccumInput` 2 assignments; ReBLUR overwrite first | overwrite precedes the `DenoisedTexture` assignment |
| 7 | Cornell caller self-consistent | `TestCornellBoxGI.cpp:1488`, `CurrentFBInfo.width` |
| 8 | `FDesc` scalars all defaulted (v187 trap absent) | `FBilateralDenoisePass.h:29-33` |
| 9 | No shader touched | `ShaderMake.cfg` intact |

**Comment-syntax safety** (the only way a comment-only edit can break a build)
checked separately: `search_files pattern="/\*"` → 4 hits, all pre-existing
(file header `:1`, three `/*param*/` annotations), none in the edited block;
`search_files pattern="\\$"` → 0 hits, so no line-continuation backslash can
swallow the following line. The block is 19 consecutive `//` lines and `:856`
resumes code cleanly.

**Three defects found in the script while reviewing it** (a script that passes
vacuously is worse than none): a broken `check()` helper whose nested ternary
evaluated the wrong branch on failure — it could have printed failures as passes;
a dead `blk = [...][:0]` placeholder asserting nothing; and a raw `FB.width`
grep that **would have failed on the patch's own documentation**, fixed by
parsing only comment-stripped lines.

**This is ad-hoc structural verification, NOT suite green.** It does not confirm
that the file compiles, that the target links, that any pixel is unchanged, or
that VUID-00344 stays absent. Gates 1-7 remain 0/7 against the patched tree.

Script left at `/tmp/hermes-verify-v190.py` — cleanup requires shell, which is
blocked. Ready to run verbatim once an operator has a terminal; its exit code is
the check.

## Not claimed

Not compiled. Not run. No image inspected. `terminal` is denied in this runspace
(two probes this tick: `echo probe-ok; date -u; pwd; nvidia-smi ...` and a bare
`/bin/true` with `workdir=/tmp` — both `status: pending_approval, pattern_key:
tirith:unknown, exit_code: -1, smart_denied: false`). No commit, no push, no
governance file touched.
