# Pending Impl Review v192

- plan: docs/PENDING_PLAN_v192.md
- commit: docs/PENDING_COMMIT_v192.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-538)
- timestamp: 2026-08-30

## plan_fidelity_check

The plan specified three substitutions, forbade the shader edit, forbade
line-number references in the comment, and required a note recording *why* the
shader hardcode stays. All four honored; no deviation declared and I found none.

Re-read the patched region myself (`:1110-1132`). The comment at `:1121-1125`
states the shader rationale in the required terms — `fp = hp * 2 + 1` encodes the
fixed half-to-full footprint relationship, "not a swapchain ratio," and "a
constant for it could only ever hold 2." That is the plan-criticer's instruction
discharged, and it closes card E's open question in source rather than only in a
marker.

The comment contains **no `:NNNN` references** — only symbol names and one "See
the note above." This is the first cycle since the defect was identified to get
that right on the first attempt; v191 needed a mid-cycle correction.

## Independent re-derivation

I re-ran the load-bearing rows rather than accepting the table:

- `RcpFullW` → 2 hits: the struct field `:1101` and the assignment `:1126`
  `1.0f / static_cast<float>(WIDTH)`. Confirmed.
- `dispatch((WIDTH` → 1 hit `:1156`. Confirmed.
- `DispatchResolve` → 3 hits: lambda `:1132`, calls `:1159` and `:1160`, passing
  `FullResGIRaw` and `FullResSpatial` respectively. **Both call sites pass
  fixed-size outputs**, so the single substitution is correct for both — this is
  the check that would have caught a bug had one output been swapchain-sized.
- `AccumTexture  = CreateTexture2D` → 1 hit `:1658`, `(NvrhiDevice, W, H, ...)`.

**Correction to the impler's table, caught here:** rows 1, 5 and 6 originally
carried line numbers that its own 18-line insertion had invalidated (`:1123` for
an assignment now at `:1126`; `:1115` for a lambda now at `:1132`). The impler
detected and corrected these before I reviewed, and row 2's query had to be
reshaped after an over-escaped form returned a false zero. The corrected table
matches what I independently observe. **Noted rather than penalised** — the
self-correction happened inside the cycle, which is the behaviour the v191 audit
asked for.

## Verification of the severity claim

The comment asserts the widened case is an out-of-bounds **UAV store**, which is
a materially stronger hazard than the out-of-bounds `Load`s of v189/v191. I
verified against the shader rather than the comment:
`search_files pattern="FullResOutput"` on `Resolve_cs.hlsl` → 3 hits: the
declaration `:10` and two writes, `:34` and `:73`, **both indexed by raw
`tid.xy`**. Reading the kernel, the only early-out is `:32` `centerDepth <= 0.0`
— a data condition. **There is no extent guard anywhere.** The claim is accurate
and is not overstated.

## Shader inertness

`int2 fp = hp` → 1 hit at `Resolve_cs.hlsl:60`, unchanged. `Resolve_cs.hlsl` does
not appear in `files:`. Note for the record: an over-escaped pattern
(`hp \* 2 \+ 1`) returned 0 here; the plain form returns the real hit. That is
the same false-zero mechanism the tester hit in v191 and it recurred in this
cycle — **the escaping artifact is reproducible, not a one-off.**

## NET-NEW: the seventh instance is on the acceptance path

The impler's card F finding is correct and I am raising its priority. Verified:
`AccumTexture` (u0) and `DisplayTexture` (u1) are bound at `:1246-1247`, created
at `:1658-1661` from `CreateTexture2D(NvrhiDevice, W, H, ...)`; the pass is sized
from `FB.width` at `:1238-1239` and `:1255`.

**`DisplayTexture` is the source of the `display` dump** — the artifact that
`validate_restir_gi.py` runs all four of its checks on, and the one gate 6 would
inspect. Every prior instance of this class (v189 bilateral, v191 ReSTIR reuse,
v192 resolve) was off the acceptance path or provably dead. **This one is not.**

Still correctly not bundled: bundling would have made row 3 of the impler's own
table unverifiable. Card F stands as its own cycle.

## Security scan

- [x] No hardcoded secrets — extent constants only
- [x] No shell injection — nothing executed
- [x] No eval/exec — N/A (C++)
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: no input handling changed; the shader's own `:49` input clamp
      and `:32` depth early-out are untouched
- [x] Error handling: no control flow changed
- [x] Tests: `produces_test_files: no`; static verification only, correctly

## Feedback for impler (FIX only)

None — KEEP.
