# Pending Commit v226

- plan: docs/PENDING_PLAN_v226.md
- plan_review: docs/PENDING_PLAN_REVIEW_v226.md (KEEP)
- files: **NONE — zero source files modified** (audit/determination cycle)
- source: no bundle — direct source read
- target: (no branch — this pipeline does not commit)
- task: Extend v200's pre-build compile-risk audit to the v200–v225 window
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: the load-bearing claim of this marker is a **NEGATIVE** (the window is compile-coherent). Per v212/v196, a determination cycle's absence of a diff is itself the artifact and must be verified as one.

## DETERMINATION: the v200–v225 source window is compile-coherent. No patch.

Three cycles modified engine source after v200's audit boundary. All three audited; all three clean.

| Cycle | Files | Shape | Verdict |
|---|---|---|---|
| v206 | `FReBLURPass.h` | comment-only, members byte-unchanged | CLEAN — struct layout/size/member set identical, no consumer affected at the type level |
| v207 | `FGIPass.cpp` + `.h` | **live UAV binding change** (u2 dummy → ternary onto `Desc.OutputTexture`) | CLEAN — see below |
| v209 | `FGIPass.h` + `.cpp` | **class member deletion** (`DummyDirectionTexture`) | CLEAN — 0 residual references in source |

### v209 — the deletion is complete, and the zero is controlled twice over

`DummyDirection` (substring, so it catches both the member and any `debugName`):

- `Runtime/Private` → **0**
- `Runtime/Public` → **0**
- Live sibling `DummyDebugStatsTexture` → **5** in the same scope (`FGIPass.cpp:217/639/650/652`, `FGIPass.h:139`) — the positive control that proves the scope completed.

**The only surviving occurrences are stale build artifacts**: `Binary/Debug/TestReSTIR_GI_Temporal`, `Binary/Debug/TestPathTraceGI`, and two `.o` files under `Build/Debug/CMakeFiles/`. Those are the *previous* build's output and are expected to contain the old symbol; they are not source and will be regenerated. Recorded explicitly because a future sweep that does not partition source from artifacts would read those 4 hits as "the deletion did not land."

### v207 — sound in BOTH consumers, including the one it did not check

v207 changed u2 from a lazily-created 1x1 dummy to a ternary that falls back to `Desc.OutputTexture`. Aliasing two UAV slots onto one resource is only safe if the shader's stray write is subsequently overwritten on **every** path, in **every** consumer, in the shader copy **each consumer actually compiles**.

**FGIPass has exactly two consumers** — `FGIPassDesc` → 2 construction sites tree-wide (`TestPathTraceGI.cpp:427`, `TestReSTIR_GI_Temporal.cpp:803`). `TestCornellBoxGI` is **not** one of them (it uses `FReSTIRPass`/`FReBLURPass` directly), so v207 cannot affect the known-good control at all.

1. **Primary target takes the real texture, not the fallback.** `TestReSTIR_GI_Temporal.cpp:811` sets `Desc.OutputDirection = DirectionTexture`, and `DirectionTexture` (`:1678-1680`) and `OutputTexture` (`:1675-1677`) are **both** `HalfW x HalfH` — matching the half-res Phase-D dispatch. Contract satisfied.
2. **Control takes the fallback, and it is exactly in bounds.** `TestPathTraceGI` never sets `OutputDirection` (0 hits, controlled by `Desc.OutputTexture` present at `:434`). It dispatches `CurrentFBInfo.width/height` (`:438-439`) over an `OutputTexture` created at `:265-267` from `WIDTH, HEIGHT`. Per card J, that target is `Resizable = false`, so the two are equal by construction.
3. **The dual-copy resolution — the check that had never been done.** `TestPathTraceGI_Data/ShaderMake.cfg:1` names `GIPathTracing.hlsl`, **but no such file exists in that directory**; `ShaderMakeBuild.py:571` resolves it to the shared `Private/Renderer/Shader/GI/` copy. v207 argued from line numbers; this cycle verified the two copies agree in the load-bearing region: `OutputDirection[pixel]` at **645 in both**, `Output[pixel]` at **537/819/822/826 in both**, sole RayGen-scope `return;` at **538 in both**. The argument transfers.
4. **Every compiled path overwrites the alias.** `:819` and `:826` are the two arms of an `#if HLVM_RGI_DEBUG_VIS / #else` — exactly one compiles, both are unconditional, both follow `:645`. The sky path returns at `:538`, *before* `:645`, so it never performs the stray write. No configuration leaves the stray direction value in `Output`.

## NET-NEW FINDING — v225's enumeration cap reproduces on `output_mode=count`, and it produces FALSE ZEROS ON SOURCE

v225 established that a directory-scoped query enumerates only ~50 files. This cycle hit it **first-hand, on a load-bearing query**:

| Query | scope | mode | result |
|---|---|---|---|
| `MaterialPlaceholderTexture` | `Runtime` | `count` | **0** ← FALSE |
| `MaterialPlaceholderTexture` | `Runtime` | `files_only` | **6** ← true |
| `DummyDebugStats` | `Runtime` | `count` | **0** ← FALSE |
| `DummyDebugStatsTexture` | `Runtime/Private` + `/Public` | `content` | **5** ← true |

The returned `counts` map shows why: it lists ~50 files, all `ThirdParty/stb`, `ThirdParty/Imgui`, build logs — and **never reaches `Private/Renderer/`**. The zero is an artifact of enumeration order, not of content.

**This is worse than v225 characterised it.** v225 found the cap while reading the *marker queue*. Here it fired on an **engine-source** query, in the exact shape an audit uses to prove a deletion is complete — the query whose zero this cycle's verdict depends on. Had the impler accepted the first `count` result, this marker would have reported `MaterialPlaceholderTexture` as deleted when it is live at 6 sites, and the audit's own control would have been the fabricated row.

**Standing rule reinforced:** `output_mode=count` at directory scope is unusable for any load-bearing negative. Use `files_only` (which enumerates correctly), or narrow `path` to a subtree small enough to complete — and always pair the zero with a positive control **in the same scope and the same mode**.

## Plan Deviations

**None.** The plan called for an audit and a patch only if a defect was found; none was found, so no source was touched. One deviation was considered and rejected: adding a comment to `FGIPass.cpp` recording that u0/u2 aliasing is audited-safe. Rejected on v212's precedent — the finding is "these are in agreement," which is the default expectation, and a stale "verified safe" comment decays silently the moment a consumer changes. The finding belongs in the marker.

## Verification that zero files were modified

Every file this cycle opened was opened with `read_file` or `search_files`. The only `write_file` calls targeted `docs/` markers (`PENDING_PLAN_v226.md`, `PENDING_PLAN_REVIEW_v226.md`, this file). No `.hlsl`, `.cpp`, `.h`, `.py`, `.sh`, `.cfg` or CMake file was written. **Controlled positive** (v205's rule): the same write path *did* successfully create three marker files this tick, each returning a byte count, so the write tool is demonstrably functional and the absence of source writes is a real negative rather than a silently-failing tool.

## What this cycle did NOT establish — load-bearing

That anything compiles, links, runs, renders or validates. **0 of 7 acceptance gates are verified against the current tree.** `terminal` was probed first-hand this tick in three shapes — a compound command, a bare `date`, and a `grep` — all refused with `pending_approval / tirith:unknown / exit_code -1 / smart_denied: false`. The refusal is categorical, not command-dependent. Nothing was built, run, or viewed.

**Severity: latent.** This cycle moves no pixel and clears no gate. Its value is that the three post-v200 source changes — including a member deletion and a live binding change — are no longer unaudited going into the operator's first build.
