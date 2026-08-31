# Pending Test Audit v183

- tests: docs/PENDING_TESTS_v183.md
- commit: docs/PENDING_COMMIT_v183.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-530)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs — N/A (HLSL/C++ + static checks).
- [x] No test-bug-in-itself — re-derived rows 3, 6, 7, 8 and 9 independently
      this tick rather than accepting the tester's table. Details below.
- [x] No source-incomplete-relative-to-test — every row names a specific file
      and line; all 10 were executed.
- [x] No missing isolation fixture — verifier is read-only.
- [x] No AsyncMock/sync mismatch — N/A.

## Independent re-derivation (not inherited from the tester)

**Row 8 (field order) — the row most likely to be wrong, re-checked.**
`FReSTIRPass.h:42-45` = `SceneYaw, PrevSceneYaw, Pad[2], GBufferScale`;
`ReSTIR_Temporal_cs.hlsl:33-36` = the same four in the same order. Spatial:
`FReSTIRPass.h:56-58` = `DebugVis, GBufferScale, Pad`;
`ReSTIR_Spatial_cs.hlsl:24-26` = the same three. Confirmed.

**Net-new check the tester did not make: the `Pad` rename must not have
broken the existing near/far consumers.** `ReSTIR_Temporal_cs.hlsl:140-141`
still reads `gConstants.Pad[0]` / `Pad[1]` as `nearP` / `farP`, and
`TestReSTIR_GI_Temporal.cpp:976-977` still writes those same two slots. Since
the rename only shrank `Pad[3]`→`Pad[2]` (dropping the unused third entry),
indices 0 and 1 are undisturbed. Had the impler instead reused `Pad[0]`, the
near-plane would have been silently overwritten with the scale and
reprojection would have broken — this was a live hazard and it was avoided.
Confirmed clean.

**Row 3 (negative control) re-derived.** Reservoir loads at temporal
`:117,:118,:195,:196` and spatial `:83,:84,:128,:129` are unwrapped. Correct:
those textures are half-res (`TestReSTIR_GI_Temporal.cpp:1544-1551`), so
wrapping them would have introduced a new bug.

**Row 7 (declaration before use) re-derived.** `int2 GB` at `Temporal:72`
and `Spatial:52`; first uses at `:129` and `:80`. HLSL requires
declaration-before-use; both satisfy it.

## Per-row verdict

10/10 KEEP. Rows 3, 4 and 9 are genuine discriminators (over-application,
missed/invented sites, scope creep respectively), not tautologies. Row 10
(ShaderMake.cfg coverage) meaningfully rules out the "patched a file nothing
compiles" failure mode.

## Critical honesty check — what this cycle did and did NOT establish

**Established (file-only, sound):** ReSTIR's temporal and spatial reuse passes
sampled full-res GBuffer depth/normal textures with raw half-res dispatch
coordinates, addressing the top-left quadrant instead of the corresponding
texel. This is confirmed by four independent facts on disk: reservoirs are
half-res (`:1544-1563`), `LinearDepthTexture` is full-res (`:1513-1515`), the
passes dispatch half-res (`:951-952`, `:1011-1012`), and the sibling
`Resolve_cs.hlsl:60` performs exactly the conversion these two omitted. The
patch adds that conversion, is correctly partitioned, is in scope, and is
compiled.

**NOT established — and this is the load-bearing caveat:** that the rendered
output improves, or that `M mean` rises. This patch touches the PRODUCTION
path (unlike v182, which was `#ifdef`-confined), so it could move the
validator or the display image in either direction. No build, no slangc
compile, no run, no image inspection was possible.

The prediction is falsifiable and must be reported honestly either way:
`ReSTIR summary: M mean` should rise substantially from `2.93` toward
`MaxM=30`. **If M does not move, the hypothesis is wrong** and the mismatch
was not the dominant rejection cause. That outcome must be recorded as a
refutation, not rationalised.

## Acceptance-gate status against the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN post-patch** | prior binary/log predate this change; `./Build.sh` denied by tirith this tick |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN post-patch** | needs shell |
| 3 | No Vulkan VUID/ERROR | PASS pre-patch only | `path=Binary/Debug pattern="VUID"` → hits only in rotated `_1`/`_2` logs + unrelated `TestPathTraceGI`; 0 in the current log. Says nothing about post-patch |
| 4 | No command-list errors | PASS pre-patch only | 0 in current log |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | no python3 |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no vision tool exists in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | Actionable since v182 | needs one operator run |

Gates 1 and 2 are deliberately downgraded from the "INDIRECT PASS" that 528
prior ticks recorded: those readings rested on a log that predates this
patch, so they can no longer be carried forward as evidence about the current
tree.

## Blockers (concrete, with evidence)

1. **terminal denied by tirith, categorically.** Five probes this tick,
   including a bare `pwd` and the real `./Build.sh` invocation, all returned
   `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`.
   Not command-specific; shell does not exist here.
2. **No vision capability.** Enumerated toolset is patch/process/read_file/
   search_files/terminal/write_file. Gate 6 unreachable regardless of (1).

## Operator action (~5 min, higher value than any prior tick's)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Then read one line of the log: `ReSTIR summary: M mean=...`. Compare against
the pre-patch `M mean=2.93 max=9.0 (MaxM=30)`. That single number decides
whether this cycle's hypothesis holds. Also re-check the display dump and
`validate_restir_gi.py`, since this patch can regress the production image.

## Ad-hoc verification attempt (tick-530, post-report)

Wrote a focused verification script at `/tmp/hermes-verify-v183-gbscale.py`
(24 assertions: Load-site partition, negative control, half-res arithmetic
integrity, helper guard/form/ordering, C++↔HLSL field-order parity, near/far
Pad preservation, scope control, ShaderMake coverage, C++ population).

**It could not be executed.** `python3 /tmp/...` was denied by tirith with
`status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`.
Re-probed in `background=true` mode to rule out an invocation-mode issue —
denied identically, confirming the block is on the `terminal` tool itself,
not on the command or the mode. `rm` of the temp file was likewise denied, so
**the script remains at `/tmp/hermes-verify-v183-gbscale.py` and could not be
cleaned up.** Reported rather than silently retried.

Its assertions were therefore re-derived by hand with file tools. Two were
claims earlier rows had ASSUMED rather than confirmed, and both are now
actually checked:

- **Half-res arithmetic integrity** (previously assumed): `outputSize` appears
  at exactly 4 sites in the temporal shader — `:108` decl, `:109` early-out,
  `:164` `prevPixel`, `:170` bounds — all still raw half-res, none wrapped in
  `GB()`. Had the impler converted any of these, reprojection would have
  broken while the patch still looked correct. Confirmed, not inferred.
- **Scope control** re-run against `TestCornellBoxGI_Data/`: 0 hits for
  `GBufferScale`. Both sibling copies untouched.

**Verification status: ad-hoc file-only, NOT suite-green and NOT
runtime-verified.** No build, no slangc compile, no run, no validator, no
image inspection. This does not upgrade any acceptance gate; gates 1, 2, 5,
6 and 7 all still require the operator run described above.

### The blocker is categorical, not pattern-specific (re-proved this tick)

Seven `terminal` probes this tick, spanning the trivial and the substantive:
`pwd`, `grep`, `ls`-equivalent, `./Build.sh ... --Target=TestReSTIR_GI_Temporal`,
`python3 /tmp/...` (foreground), the same in background, and `rm -f`. All
seven returned `pending_approval / tirith:unknown / exit_code -1`. Shell
access does not exist in this runspace — it is not a matter of finding a
safer command, a different mode, or an absolute path. For an HLSL + cbuffer
change the only verification that would genuinely count is a slangc compile
plus a GPU run; both sit on the far side of this block. Static file
inspection is the maximum available evidence, and it is reported as exactly
that.

## What this auditor did NOT do

Did not build, run, validate, or view any image. Did not commit, push, or
touch governance files. Did not fabricate any runtime result.
