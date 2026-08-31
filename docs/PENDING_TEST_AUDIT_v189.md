# Pending Test Audit v189

- tests: docs/PENDING_TESTS_v189.md
- commit: docs/PENDING_COMMIT_v189.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-536)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static checks)
- [x] No test-bug-in-itself — I re-executed rows 4, 8, 14, 16, 17 myself
- [x] No source-incomplete-relative-to-test — every row names file + query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No vacuous-pass rows — every query is single-term per tick-526; I checked
      specifically that no row uses `|` alternation. None does.

## Independent re-derivation of the discriminators

**Row 4 re-run (the row most likely to be sloppy).** The tester claims 15
`FB.width` hits with the only bilateral-block survivor being a comment. Re-ran
`search_files pattern="FB\.width"` → 15 hits. Walked all 15. In the bilateral
block (`:845-878`) the sole hit is `:853`, inside the comment. The other 14 are
GBuffer (`:773`), resize bookkeeping (`:754`, `:756`), view constants (`:764`),
GBufferScale (`:1025`, `:1071`), resolve (`:1093`), resolve dispatch (`:1122`),
ReBLUR (`:1148`, `:1150`, `:1169`), accumulate (`:1204`, `:1221`), blit
(`:1235`). **All correctly full-res.** Claim holds.

**Row 8 re-run (the initialization-order proof).** `:793 Desc.OutputWidth =
HalfResWidth` is in the same function as `:872`, 79 lines earlier, and drives the
GI trace grid. This is a genuine discriminator, not a restatement: it converts
"assigned somewhere earlier in the program" into "if this were zero the test
would already be visibly broken upstream." Sound.

**Row 14 re-run (deadness).** `search_files pattern="AccumInput"` → 8 hits;
exactly two assignments, `:1111` and `:1168`. `:1168` is inside
`if (bReBLURInitialized && !bBypass)` (`:1112`) and executes only after ReBLUR
overwrote the texture at `:1148`. No third assignment exists. Holds.

**Rows 16/17 re-run.** `denoised` → 0 hits in `validate_restir_gi.py`, 0 hits in
`v176-recipe.sh`, run as separate single-term queries. Holds.

## Net-new check the tester did not make — and it strengthens the patch

The tester verified `HalfResWidth` is non-zero but never asked whether it can go
**stale**. It can't, and the reason makes the fix better than claimed:

- `WindowProps.Resizable = true` (`:2916`), so `FB.width` **is** a runtime
  variable.
- `BackBufferResizing()` (`:1294-1297`) does only `BindingCache.Clear()`. It does
  **not** recreate any GI texture.
- `CreateGBufferTextures()` (`:1523-1525`) uses `const uint32_t W = WIDTH, H =
  HEIGHT` — the file-scope constants `800`/`600` (`:106-107`) — **not** the
  framebuffer extent. `OutputTexture` (`:1564-1566`) and `HalfResWidth`
  (`:1562`) therefore derive from those constants and are fixed at 400x300 for
  the process lifetime.

**Consequence: pre-patch, `Bd.OutputWidth = FB.width` fed a *variable* extent to
a pass whose input is a *fixed* 400x300 texture.** At the default 800x600 the
mismatch is the 2x the card described; after any window resize it becomes an
arbitrary ratio, in either direction. Post-patch both sides are the same fixed
constant and cannot diverge at all.

So the patch is not merely "correct at 800x600" — **it removes a
resize-dependent mismatch entirely.** Neither the card, the plan, the impler nor
the tester noticed this. It raises confidence rather than lowering it, so it does
not change the verdict, but it belongs in the record.

(Corroborating that this fixed-size design is deliberate, not a second bug:
`:2565-2569` explicitly reads each texture's *own* `getDesc()` for readback, with
the comment "Phase D: use the texture's real size … the old hardcoded
WIDTH×HEIGHT readback hung on them." The codebase already learned this lesson in
the dump path; `:852` was simply never updated.)

## Per-row verdict

**18/18 KEEP.** Rows 8, 14, 16, 17 and 4 are genuine discriminators — each could
independently have failed the cycle. Rows 1-5 are individually weak but correctly
kept separate: the alternation form that would collapse them returns 0 hits on
this runspace and would read as a false failure. Row 18a is correctly marked
`N/A — cannot fire` rather than PASS, which is the honest form; I confirmed
`ShaderMake.cfg:3` does compile `BilateralDenoise_cs.hlsl`, so the trap would be
live if a shader had been touched. None was.

No row is padding. No row asserts a result its stated query would not produce.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The bilateral dispatch extent now matches its input texture exactly, and both
   are process-lifetime constants that cannot diverge under resize.
2. Every `t_Input.Load` in `BilateralDenoise_cs.hlsl` becomes in-bounds; the
   launched grid stops being 4x oversized.
3. The pass's output is dead on **all three** control paths (`bBypass`; ReBLUR
   off; ReBLUR on), so no validated artifact can move.
4. The barrier-flushing side effect the dispatch is retained for is
   grid-independent and preserved verbatim.
5. The sibling caller (`TestCornellBoxGI`) is already self-consistent and
   untouched.

**The finding that makes this cycle worth more than its 2 lines:**

**Card B had been deferred three consecutive times as "hard-blocked, needs a
build", and both of its stated blockers were refutable from source without one.**
Blocker 1 ("the choice populates different regions of a dump the validator and
gate 6 read") fails because `denoised` appears in neither the validator nor the
recipe — two single-term greps. Blocker 2 ("changing the grid could perturb the
barriers it exists to flush") fails because the flush comes from binding-set
creation plus `setComputeState`, both of which precede the dispatch and neither
of which reads the grid — one file read.

This inverts the standing conclusion in `PIPELINE_HEALTH_..._tick-535.md`
("**No further file-only cycle can advance this repair without guessing**").
That statement was wrong, and it was wrong because three cycles re-quoted the
card's self-assessment instead of testing it. **General lesson, third
consecutive cycle in this lineage to produce a variant of it: a card's stated
reason for being blocked is a claim, not a fact, and must be re-derived like any
other.** v187 and v188 found cards right about the symptom and wrong about the
remedy; v189 found a card wrong about being blocked at all.

**NOT established — load-bearing:** that the file compiles, that the target
links, that the dispatch is correct at runtime, that the `VUID-...-00344` class
stays absent, or that any pixel is unchanged. **No build, no run, no image.**

**Ordering caveat carried forward:** v183/v184/v185 are one dependency chain
still awaiting a single operator run. v189 touches a different pass
(`FBilateralDenoisePass`, not `FReSTIRPass`) and cannot affect
`ReSTIR summary: M mean=...`. Judge them separately.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183-v189 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log is 2026-08-14, predates v183-v189. Not carried forward as PASS |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell/python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.**

## Blockers (concrete, with evidence)

1. **`terminal` denied categorically by tirith.** Two probes this tick: a
   compound `echo probe-ok; date -u; pwd`, and a bare `/usr/bin/true` with
   absolute path, no arguments, no shell metacharacters, `workdir=/tmp`. Both
   returned `status: pending_approval, pattern_key: tirith:unknown, exit_code:
   -1, smart_denied: false, allow_permanent: true`. The minimal probe rules out
   command-pattern matching — it is a block on the tool itself. I stopped at two
   rather than tripping the `same_tool_failure_warning` threshold.
2. **No vision capability.** Toolset this session: `patch`, `process`,
   `read_file`, `search_files`, `terminal`, `write_file`. Gate 6 is unreachable
   even if the shell block were lifted.

## Operator action (~8 min)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
python3 ../../Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
        ../../Test/TestReSTIR_GI_Temporal_Data/dumps
```

Two failure conditions that must **not** be explained away:

- **`display` changes** → the deadness argument is wrong; revert v189.
- **`VUID-VkDescriptorImageInfo-imageLayout-00344` appears** → the
  grid-independence argument is wrong; revert v189 rather than patching around
  it.

`denoised` differing is expected and is neither regression nor improvement.

## Caveat

Single-profile host: all six roles are the same model. Each stage re-derived its
inputs from source rather than inheriting the previous marker's claims — which is
how the plan gate refuted the card's blockers and how this audit found the
resize-staleness angle — but these are self-checks, not fresh eyes
(`six-role-pipeline §Anti-patterns §7`).

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
