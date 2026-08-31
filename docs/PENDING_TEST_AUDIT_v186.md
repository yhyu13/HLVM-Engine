# Pending Test Audit v186

- tests: docs/PENDING_TESTS_v186.md
- commit: docs/PENDING_COMMIT_v186.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-533)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static checks)
- [x] No test-bug-in-itself — I re-executed rows 5 and 8 myself
- [x] No source-incomplete-relative-to-test — every row names file + query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A

## Independent re-derivation

**Row 5 re-run.** `pattern="float2 Pad;"` over Runtime → 3 hits: Cornell
*spatial* `:19`, `SSAOBlur_cs.hlsl:9`, `ExposureAdaptation_cs.hlsl:12`. No
generation shader among them. Migration is complete and the row is a genuine
enumeration, not a hit-count.

**Row 8 re-run — the row that would have hidden a dead patch.** Read both
`ShaderMake.cfg` directly: Temporal cfg `:5`, Cornell cfg `:5` both list
`ReSTIR_Generate_cs.hlsl`. This is the check v182 failed to make and it is
correctly load-bearing here because v186, unlike v185, does touch shaders.

**Net-new check the tester did not make: are the OTHER two `float2 Pad;`
sites the same defect?** Checked both, because a verifier that only confirms
the tester's framing adds nothing. `FExposureAdaptationPass.h:12` declares
`TFP32 Pad[2]` against `ExposureAdaptation_cs.hlsl:12 float2 Pad` — the same
kind mismatch. But the struct is closed by
`static_assert(sizeof(...) == 16)` at `:14` and the pad is the *final* field
of a 16-byte block, so nothing can be appended after it without breaking the
assert loudly at compile time. **Genuinely inert and self-guarding — correctly
out of scope.** Recording it so a future tick does not re-litigate it.

## Per-row verdict

10/10 KEEP. Rows 5, 8, 9 and 10 are real discriminators (partial-migration,
uncompiled-copy, blast-radius, spatial-struct-undisturbed). No row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):** the `FReSTIRConstants` trailing padding
now agrees in kind across the C++ header and both compiled HLSL copies; the
field is provably write-never and read-never, so the change is inert; and both
edited files are on compiled paths.

**The findings that make this cycle worth more than its 2-line patch:**

1. **The card's stated reason was wrong.** `float2` is not an array; both
   sides sat at floats 9/10 and agreed. The fix is still right, but as
   "remove a kind inconsistency before it becomes load-bearing", not as
   "repair a live desync". Recorded so the patch is not later cited as
   evidence of a bug that never existed.
2. **The header is shared by two tests.** Patching one shader copy would have
   *relocated* the mismatch into the known-good control while the marker
   claimed it fixed.
3. **A live desync in the control test (found at impl review).**
   `TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl` has **no `GBufferScale`
   field**, while `FReSTIRPass::DispatchSpatial:547` writes it unconditionally
   at float 9. That constant currently lands in the shader's unread `Pad`, so
   it is swallowed rather than corrupting — but it is one appended field away
   from producing wrong values in the very test the lineage uses as its
   driver/framework control. Confirmed live: Cornell `ShaderMake.cfg:7`
   compiles that shader.
4. **A live half/full-res dispatch mismatch (found at impl).** Bilateral
   denoise dispatches 800x600 over a 400x300 input.

Items 3 and 4 are queued to PICK, not patched. Both need a build to verify and
item 4's correct value is ambiguous from source.

**NOT established — load-bearing:** that either test compiles, that slangc
accepts the edited structs, that reflection places the fields where expected,
or that any pixel is unchanged. No build, no compile, no run, no image.

**Ordering caveat carried forward, unchanged from v185:** v183 (add `GB()`),
v184 (make `GBufferScale` arrive), v185 (fix its coordinate space) are one
dependency chain and must be judged on a single run. v186 is independent of
that chain and cannot affect its outcome — which is the point of keeping it
inert.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183-v186 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | pre-patch only | 0 VUID in the current log, but it predates all four patches |
| 4 | No command-list errors | pre-patch only | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell/python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 are deliberately not
carried forward as PASS: a 2026-08-14 log says nothing about a tree patched on
2026-08-30.

## Blockers (concrete, with evidence)

1. **`terminal` denied categorically by tirith.** Three probes this tick: a
   combined `ls docs/`, a bare `date`, and `/usr/bin/echo verifier-probe` with
   an absolute interpreter path. All returned
   `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1,
   smart_denied: false`. The third was chosen specifically to rule out
   command- or path-pattern matching. The runtime fired
   `same_tool_failure_warning; count=3`. It is a block on the tool itself.
2. **No vision capability.** Toolset is `patch`, `process`, `read_file`,
   `search_files`, `terminal`, `write_file`. Gate 6 is unreachable even if the
   shell block were lifted.

## Operator action (~5 min — unchanged priority from v185)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Read `ReSTIR summary: M mean=...` against the long-standing
`2.93 max=9.0 (MaxM=30)`. **If M does not move with v183+v184+v185 present,
the half-res-reuse hypothesis is wrong and must be recorded as a refutation,
not explained away.** Also re-check the display dump and
`validate_restir_gi.py` — those three are production-path and could regress
the image.

Then `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild` to
confirm v186 did not disturb the control.

## Caveat

Single-profile host: all six roles are the same model. Each stage re-derived
its inputs from source rather than inheriting the previous marker's claims,
which is the most this shape offers, but these are self-checks rather than
fresh eyes (`six-role-pipeline §Anti-patterns §7`).

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
