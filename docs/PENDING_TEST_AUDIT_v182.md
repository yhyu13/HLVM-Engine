# Pending Test Audit v182

- tests: docs/PENDING_TESTS_v182.md
- commit: docs/PENDING_COMMIT_v182.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-529)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs — N/A (HLSL + grep verifier).
- [x] No test-bug-in-itself — rows assert against the two on-disk shader copies,
      which ARE the artifacts the commit modified. Row 4 is the negative control
      (no stragglers), row 6 is the scope control. Both are real discriminators,
      not tautologies.
- [x] No source-incomplete-relative-to-test — every row maps to a specific line
      in a specific file; all 8 executed this tick.
- [x] No missing test isolation fixture — verifier is read-only.
- [x] No AsyncMock/sync mismatch — N/A.

## Per-row verdict

8/8 KEEP. Rows 1-3 pin the copy-parity invariant (the trap that would silently
rebuild a stale probe); row 4 is a true negative control; rows 5-6 pin HLSL
scope validity; rows 7-8 pin that the probe remains compiled and reachable.

## Critical honesty check — what this cycle did and did NOT establish

**Established (by construction, file-only, sound):** the mode-20/21/22/31 probes
were reading dispatch-space coordinates against a full-res GBuffer at gbScale=2.0,
i.e. a different texel set than the production reads they were built to bisect.
The 2026-07-30 inference "mode 20 returns black ⇒ t3 SRV unbound" therefore does
not follow from its evidence. Combined with ticks 526-528's direct verification
of the layout↔set↔handle chain, the card's premise is now refuted **with the
original observation explained**, not merely contradicted.

**NOT established:** that mode 20 now returns non-zero. That requires a rebuild
and a run. No tick of this job can do that (see blockers). Any future tick that
reports "gate 7 PASS" without a post-patch log dated later than 2026-08-30 is
fabricating. The correct post-patch expectation, to be checked by the operator:
mode 20 should show per-material variation comparable to the direct
`gbuffer_material` dump (std ≈ [0.16, 0.16, 0.13]), not solid black.

## Acceptance-gate status against the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | INDIRECT PASS | binary + 273-line clean log on disk; NOT re-verified post-patch |
| 2 | HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 runs | INDIRECT PASS | 8 frames, clean 21.83s completion (log:265) |
| 3 | No Vulkan VUID/ERROR | PASS (sound) | `path=Binary/Debug pattern="VUID"` → 23 hits, ALL in rotated `_1`/`_2` logs + unrelated TestPathTraceGI WSI; **0 in the current log**. Validation layer confirmed hooked up at DeviceManagerVk4_LifeCycle.cpp:118, so this is a real negative |
| 4 | No command-list errors | PASS | `pattern="command list"` → 0 hits in current log |
| 5 | validate_restir_gi.py on newest dump group | BLOCKED | needs python3 via terminal |
| 6 | Vision check of fresh display PNG | BLOCKED (structurally) | this job has no vision tool at all — enumerated toolset is patch/process/read_file/search_files/terminal/write_file |
| 7 | HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial | **NOW ACTIONABLE** | was previously believed refuted-or-broken; this cycle identifies and fixes the instrumentation bug that made it read black. Needs one operator run to confirm |

## Blockers (concrete, with evidence)

1. **terminal denied by tirith** — fresh probe this tick returned
   `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`.
   Gates 1, 2, 5, 7 all require shell.
2. **No vision capability in this runspace** — gate 6 is unreachable regardless
   of terminal, since `read_file` refuses image input and no image tool exists.

## Operator action (~5 min, now genuinely worth running)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```
Then compare the fresh `gi_raw_frame8.png` against `gbuffer_material_frame8.png`.
If mode 20 now shows Sponza material variation, gate 7 passes and the card closes
on positive evidence for the first time. If it is still solid black, the
instrumentation is now trustworthy and the remaining hypotheses in
`DIAGNOSTIC_2026-07-30.md` §1/§2/§3 become live again — with a probe that is
finally measuring the right address.

## Ad-hoc verification attempt (tick-529, post-report)

Wrote a focused verification script at `/tmp/hermes-verify-v182-gbpixel.py`
(6 checks: probe alignment, straggler scan, declaration-before-use, guard
containment, production-read integrity, cross-copy parity). **It could not be
executed** — `python3 /tmp/...` was denied by tirith with
`status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`, the same
block that denies every shell call in this runspace. `rm` of the temp file was
likewise denied, so **the script remains at `/tmp/hermes-verify-v182-gbpixel.py`
and could not be cleaned up.** Reported rather than silently retried.

Its checks were therefore re-derived by hand with file tools. Two of them were
claims the earlier rows had ASSUMED rather than confirmed, and both are now
actually checked:

- **Guard containment** (previously assumed): `#ifdef HLVM_RGI_DEBUG_VIS` at
  L653 pairs with `#endif` at L827 (`^#endif` hits: 105, 516, 572, 637, 827,
  837). All four probes (764/765/766/793) fall strictly inside 653..827, so the
  patch genuinely cannot affect a default build. Confirmed, not inferred.
- **Cross-copy byte parity** (previously asserted from matching line numbers
  only): both copies report 949 lines / 41340 bytes, with identical content at
  every probed line. Consistent with byte-identical; note this is a size+spot
  check, not a true hash comparison, since no shell is available to run `cmp`.
- **Production reads intact**: `GBuffer{WorldPos,Normal,Material}[gbPixel]` at
  501/502/503 plus the roughness read at 584 — 4 hits in each copy, unchanged.

**Verification status: ad-hoc file-only, NOT suite-green and NOT runtime-verified.**
No build, no run, no validator, no image inspection. This does not upgrade any
gate; gate 7 still requires the operator run described above.

### The blocker is categorical, not pattern-specific (proved this tick)

To rule out the possibility that only *certain* commands were being rejected
(e.g. `python3`, or `/tmp` paths), the minimal possible command was probed:

```
terminal command="pwd"
→ status: pending_approval, pattern_key: tirith:unknown, exit_code: -1
```

`pwd` is denied identically to `python3 /tmp/...`, `rm`, and `echo`. **Shell
access does not exist in this runspace at all** — it is not a matter of finding
a safer command, a different working directory, or an absolute path. For an
HLSL change the only verification that would genuinely count is a slangc compile
plus a GPU run; both sit on the far side of this block. Static file inspection is
the maximum available evidence, and it is reported as exactly that.


## What this auditor did NOT do

Did not build, run, validate, or view any image. Did not commit, push, or touch
governance files. Did not fabricate any runtime result.
