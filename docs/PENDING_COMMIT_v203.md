# Pending Commit v203

- plan: docs/PENDING_PLAN_v203.md
- files: Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp
- source: no bundle — direct source analysis
- target: (no branch — no commit made, per job instruction)
- task: Sweep v202's layout-vs-each-consumer invariant across all six
  layout/consumer pairs, not the one pair v202 happened to check.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- notes: **Comment-only. +45 lines, 0 functional lines, one shared file.** No
  `.hlsl` touched (v182 dual-copy hazard not engaged). The known-good control is
  byte-unchanged (v196 row holds). Both consumers' shaders byte-unchanged.

## The six pairs

| Layout | Primary consumer | Control consumer | Verdict |
|---|---|---|---|
| `GenerationLayoutSRV` (cb + t0..t4) | t0..t4 — match | t0..t3 — **missing t4** | defect, card M (v202) |
| `GenerationLayoutUAV` (u384/385) | u0/u1 `space1` | u0/u1 `space1` | clean |
| `TemporalLayoutSRV` (cb + t0..t9) | t0..t9 — match | **t0..t7 only** | **defect, card N (new)** |
| `TemporalLayoutUAV` (u384/385/386) | u0/u1/u2 `space1` | **u0/u1, DEFAULT space** | **defect, card N (new)** |
| `SpatialLayout` (cb + t0..t4 + u384) | t0..t4 + u0 | t0..t4 + u0 | clean |
| — | — | — | 3 clean / 3 defective |

## Findings

**1. Card N — the control's temporal shader diverges on BOTH sets.**
`TemporalLayoutSRV` declares t0..t9 (`:236-248`). The control's
`ReSTIR_Temporal_cs.hlsl` declares t0..t7 (`:48-55`); `gCurrRadiance` and
`gHistRadiance` return **0 hits** there against **2** in the primary — a
same-shape positive control, so the zero is real.

**This has no fallback, which is what makes it worse than card M.**
`DispatchGeneration` protects card M's t4 with a ternary. `DispatchTemporal`
binds slots 8 and 9 unconditionally (`:562-563`), and `TestCornellBoxGI.cpp`
supplies both (`:1599` `CurrentRadiance`, `:1600` `HistoryRadiance`). Nothing
substitutes; the shader simply has no receiver.

**2. The UAV half is the sharpest instance of the class so far, because it
diverges in DESCRIPTOR SET MEMBERSHIP rather than count.** `TemporalLayoutUAV`
declares three UAVs and exists as a separate layout *because* the primary
declares `register(uN, space1)` → SPIR-V set 1; the file states that reflection
at `:243-247`. The control declares two UAVs at `register(u0)`/`register(u1)` in
the **default** space (`:57-58`), which reflect into set 0.

**Controlled positive that this is not house style:** the control's own
`ReSTIR_Generate_cs.hlsl` **does** use `register(u0, space1)` (`:36-37`). The
divergence is intra-directory, so it is a real inconsistency, not a convention.

**3. Spatial is clean, and the clean verdict carries information.** Both copies
declare t0..t4 + one `gOutput : register(u0)`. Note the control's spatial UAV is
in the default space and that is **correct here**, because `SpatialLayout` is
unsplit and expects set 0 — the identical declaration that is wrong in its
temporal copy. **The space convention is per-layout, not per-file**, which is
why a file-level style sweep would have mis-graded both.

**4. Runtime evidence, reported for what it is.** `Binary/Debug/TestCornellBoxGI.log`
exists (2026-08-09) and shows `FReSTIRPass initialized successfully` (`:91`)
with **0 VUID hits**. I am NOT citing that as evidence the divergence is benign:
the log predates this analysis by weeks, and I cannot tell from it whether
`CVar_r_ReSTIR_EnableTemporal` was true on that run. It is recorded as an open
question for whoever runs the control next, not as a clearance.

## Plan Deviations

**One, and it is the plan gate's correction rather than my own choice.** The
plan enumerated three layouts; the gate corrected the domain to six
layout/consumer *pairs* and I swept six. The two clean pairs are documented,
not merely omitted — an unswept pair and a clean pair look identical in a diff.

**I did not fix any defect found.** All three live in the known-good control.
Card L's precondition and the plan gate's ruling govern: a control with a
documented defect is still a usable control; a control we have edited is not.

## NEAR-MISS TO RECORD — my third patch deleted two live lines

The comment insertion before `SpatialLayout`'s binding list matched a hunk that
included the first three binding items and **dropped `ConstantBuffer(256)`,
`Texture_SRV(0)` and `Texture_SRV(1)`**, leaving an empty line in their place.
The returned diff showed it; I restored them byte-exact and re-verified
(`:325-333`, 7 items) plus the invariants `createBindingLayout` → 5 and
`LayoutDesc.bindings` → 5.

This is exactly the failure mode v202's assertion 5 was written for — "a
comment-only edit's only realistic failure is a marker swallowing a live line" —
and it fired on the very next cycle. **The lesson is not 'be careful': it is
that an old-string anchored on a comment adjacent to a list will match into the
list.** Anchor on the statement boundary instead. Had I trusted the
comment-only framing and not read the returned diff, this cycle would have
shipped a silently broken spatial layout under a "0 functional lines" claim.

## LSP note

`Use of undeclared identifier 'FReSTIRPass'` reported at 365 → 380 → 399 → 407,
tracking insertions, always column 10 of an unmodified line. Same stale-index
artifact v202 diagnosed. A comment-only diff cannot undeclare an identifier.
