# Pending Test Audit v190

- tests: docs/PENDING_TESTS_v190.md
- commit: docs/PENDING_COMMIT_v190.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-537)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static checks)
- [x] No test-bug-in-itself — I re-executed rows 1, 4, 8, 9, 11, 14 myself
- [x] No source-incomplete-relative-to-test — every row names file + query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No vacuous-pass rows — checked every row for `|` alternation. None uses it.

## Independent re-derivation

**Row 9 (the row most likely to be sloppy).** The tester claims the refuted
sentence survives only as a quotation. Re-ran
`search_files pattern="forces nvrhi to emit"` → **1 hit at `:841`**, and it is
inside the quoted clause, with `:844` `That mechanism is wrong.` following. The
old assertion cannot be read as current guidance. Holds.

**Row 8.** `search_files pattern="Do not cite this pass as correct"` → **1 hit**
at `:867`. The duplicate the impler removed in hunk 2 is genuinely gone; the
statement now appears once. Holds.

**Row 11 (the cycle's premise).** Re-read `vulkan-compute.cpp:166-173` in full.
`CommandList::dispatch` is three statements — `assert`,
`updateComputeVolatileBuffers()`, `cmdBuf.dispatch()` — and `:173` closes the
function. There is no barrier call, and this is the complete body rather than a
grep window. The comment's claim is sound.

**Row 14.** `search_files pattern="AccumInput"` → 4 hits: `:1123` (declaration
with the `bBypass ? FullResGIRaw : FullResSpatial` ternary), `:1180`
(reassignment inside `if (bReBLURInitialized && !bBypass)`), `:1190`, `:1203`
(uses). Exactly two assignments. v189's deadness argument is undisturbed.

**Row 1.** `search_files pattern="Bd\."` → 9 hits, values identical to those
recorded in `PENDING_COMMIT_v189.md`. Inertness confirmed against an
independent pre-edit record, not against the impler's own assertion.

## Net-new check the tester did not make — and it matters

**Nobody in this cycle asked whether `FBilateralDenoisePass` has a second
caller.** It does. `search_files path=Engine/Source/Runtime
pattern="BilateralDenoisePass"` → 42 hits, including
`TestCornellBoxGI.cpp:1478-1488` — the known-good control test — which builds its
own `FDesc` and calls `DenoisePass.Dispatch`.

This matters for two reasons:

1. **The v190 comment is scoped correctly by luck, not by design.** It describes
   the pass as vestigial and its output as dead. That is true *of the ReSTIR
   test's call site*. In Cornell the call is guarded by
   `else if (CVar_r_GI_Denoise.GetValue())` and its output `DenoisedHDRTexture`
   is the live denoise path when ReBLUR is off. The comment lives in
   `TestReSTIR_GI_Temporal.cpp` so it cannot mislead about Cornell — but **if
   card D is ever executed, "delete the dispatch" must mean the ReSTIR call
   site only.** Deleting the pass class would break the control test. Recorded
   here because card D's wording ("delete a whole dispatch, a binding-set
   creation and a constant-buffer write per frame") is ambiguous on this point.

2. **The v187 lifetime defect class does NOT apply here — checked, not assumed.**
   Cornell declares `FBilateralDenoisePass::FDesc DenoiseDesc;` **without** `{}`,
   which is the exact shape that made three ReSTIR constant structs
   indeterminate in v187. I read `FBilateralDenoisePass.h:23-34`: every scalar
   member has a default member initializer (`OutputWidth = 0`,
   `OutputHeight = 0`, `DepthSigma = 0.01f`, `NormalSigma = 0.1f`,
   `SpatialSigma = 2.0f`) and the four handles are `nvrhi::TextureHandle`
   (RAII, self-initializing). **Default-initialization is therefore fully
   defined here.** Cornell also assigns all nine fields explicitly
   (`:1479-1487`). No defect. This is a negative result and it is worth
   recording precisely because the surface shape matches v187's bug.

Also confirmed Cornell is self-consistent on the v189 axis: `:1483-1484` pass
`CurrentFBInfo.width/height` while `:1479` inputs `HDRTexture` and `:1482`
outputs `DenoisedHDRTexture`, both full-res. Input extent matches dispatch
extent. Untouched and correct.

## Per-row verdict

**17/17 KEEP.** Rows 6, 9, 11, 14 and 1 are genuine discriminators. Rows 2-5 and
15-17 are individually weak collateral-damage checks but correctly kept separate
— the collapsed alternation form returns 0 hits on this runspace and would read
as a false failure. No row is padding; no row asserts a result its stated query
would not produce.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. Card D's prescribed remedy is a no-op in the position it specifies.
   `setComputeState` calls `commitBarriers()`; `dispatch` does not. Replacing
   the dispatch with a manual `commitBarriers()` would delete a real pass and
   insert a line that adds nothing beyond what the next `setComputeState`
   already does.
2. The barrier flush the pass was retained for is supplied by every consuming
   pass in turn — `DispatchGeneration`'s `setComputeState` precedes
   `DispatchTemporal`'s binding-set creation.
3. The source comment no longer teaches the false mechanism.
4. Zero functional lines changed; v189's fix and all half-res plumbing intact.
5. The second caller (`TestCornellBoxGI`) is correct, self-consistent, and free
   of the v187 lifetime defect despite matching its surface shape.

**NOT established — load-bearing:** that the file compiles, that the target
links, that deleting the dispatch would be safe, that `VUID-...-00344` stays
absent, or that any pixel is unchanged. **No build, no run, no image.**

**Ordering caveat carried forward:** v183/v184/v185 remain one dependency chain
awaiting a single operator run. v189 and v190 touch a different pass and cannot
move `ReSTIR summary: M mean=...`. Judge them separately.

## The lesson this cycle adds

The lineage's running lesson — *a card's stated reason for being blocked is a
claim, not a fact* — gets a fourth variant, and it inverts:

- v187: card right about the symptom, wrong about the remedy.
- v188: same.
- v189: card wrong about being blocked at all.
- **v190: card right about being blocked, wrong about the remedy — and the
  remedy would have been actively harmful, because it encodes a false model of
  when nvrhi flushes barriers.**

Card D was the first card in this lineage whose "needs a build" was accurate
(absence-evidence genuinely cannot be derived from source). It was still wrong
about what to build. **Correct blocked-ness does not confer correct
prescription**, and a gate that accepts a card because its blocker checks out
will still ship the wrong change.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183-v190 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v190. NOT carried forward as PASS |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell/python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.**

## Blockers (concrete, with evidence)

1. **`terminal` denied categorically by tirith.** Two probes this tick: a
   compound `echo probe-ok; date -u; pwd; nvidia-smi ...`, and a bare `/bin/true`
   (absolute path, no arguments, no shell metacharacters, `workdir=/tmp`) chosen
   to rule out command-pattern matching. Both returned
   `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1,
   smart_denied: false, allow_permanent: true`. Stopped at two rather than
   tripping `same_tool_failure_warning`.
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

v190 is comment-only, so **no v190-specific failure condition exists** — any
change in output is attributable to v183-v189, not to this cycle.

**The decisive experiment for card D**, once a terminal is available: delete the
`if (!bBypass)` bilateral block in `TestReSTIR_GI_Temporal.cpp` **only** (not the
pass class — Cornell uses it), rebuild, run, and grep the log for
`VUID-VkDescriptorImageInfo-imageLayout-00344`. Clean → the pass goes
permanently. Present → the retention comment becomes true again and the pass
stays, justified for the first time.

## Caveat

Single-profile host: all six roles are the same model. Each stage re-derived its
inputs from source rather than inheriting the previous marker's claims — which is
how the plan gate refuted card D's remedy, how the review gate caught the
duplicated comment, and how this audit found the second caller — but these are
self-checks, not fresh eyes (`six-role-pipeline §Anti-patterns §7`).

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
