# Pending Tests v211

- commit: docs/PENDING_COMMIT_v211.md
- tester: agent_5_tester (tick-557)
- timestamp: 2026-08-30
- mode: **file-only**. `terminal` probed first-hand this tick and refused at
  the tool boundary (`pending_approval / tirith:unknown / exit_code -1`), so
  nothing was compiled, run, validated or viewed. Every row below is a
  file-derived proposition and is falsifiable as written.

| # | Row | Method | Result |
|---|-----|--------|--------|
| 1 | Slot 5 renamed, cbuffer still 8 floats in original order | read `:17-23` in place | **PASS** — `float2 TexelSize, DepthSigma, NormalSigma, SpatialSigma, GuideScale, Pad1, Pad2` |
| 2 | `GB()` declared exactly once | `GB(` set enumerated, decl at `:43` | **PASS** |
| 3 | Exactly four guide call sites | set: `:90`, `:91`, `:121`, `:126` | **PASS** — matches primary one-for-one |
| 4 | No dispatch-res resource routed through `GB()` | enumerated `t_Input` `:92`,`:133` and `u_Output` `:139` | **PASS** — all raw |
| 5 | Binding surface unchanged | `register` → 6: b0,t0,t1,t2,s0,u0 | **PASS** |
| 6 | `GuideScale` declared before `GB()` uses it (HLSL requirement) | cbuffer closes `:24`, helper `:43` | **PASS** |
| 7 | Unfilled-constant guard present | `:45` `int s = max(int(GuideScale), 1);` read in place | **PASS** |
| 8 | Primary copy byte-unchanged | 131 L / 4,872 B, equal to pre-edit read this tick | **PASS** |
| 9 | Control copy byte-unchanged | 120 L / 4,246 B, equal to pre-edit read this tick | **PASS** |
| 10 | `Pad0` gone from the patched source | `Pad0` → 0 in the `.hlsl` | **PASS** (see row 11 — this zero is controlled) |
| 11 | **Row 10's zero is controlled, and the control proves the cycle's premise** | `Pad0` scoped at the DIRECTORY → 5 hits | **PASS** — see below |
| 12 | Marshaller and shader agree on slot 5 | `ConstantsData[5] = GuideScale` `:189` ↔ shader slot 5 | **PASS** |

## Row 11 — the control turned into the cycle's strongest single piece of evidence

`Pad0` → 0 in the patched `.hlsl` is exactly the shape of zero this lineage has
been burned by six times, so it needed a same-shape positive (v205). Scoping
the identical query at the **directory** gives 5 hits across 11 files —
including `HBAO_cs.hlsl` and `JointBilateralUpsample_cs.hlsl`, so the token is
findable and the query is sound.

But one of those five hits is the finding:

> `Engine/Source/Runtime/Shader/BilateralDenoise_cs.sblob` → **1 hit for `Pad0`**

The **compiled blob still contains `Pad0`** while the source no longer does.
That is not merely corroboration that the blob is a build product — it is a
**direct, byte-level demonstration that the on-disk SPIR-V was built from the
pre-v204 source**, which until now was an inference from `ShaderMake.cfg:5`
and `build.ninja:2372`.

The plan argued *intent to build*. The plan review upgraded it to *a completed
build*, from the blob's existence. This row upgrades it again to **the stale
shape is inside the shipped artifact**, from the blob's contents. Three
independent strengthenings of the same claim across three roles, each by a
different method — which is what the gate structure is for.

It also means the fix is **incomplete until someone rebuilds**: the corrected
source will not reach any consumer that loads this blob until
`Common_ShaderMake` re-runs. That is stated plainly rather than left implicit,
and it is one more thing waiting on the operator's first build.

## Row 12 — what was actually broken

`FBilateralDenoisePass::Dispatch` writes `ConstantsData[5] = GuideScale`
(`:189`) into a 64-float buffer. The patched shader now reads slot 5 as
`GuideScale`. **Before this cycle the C++ wrote a live scale into a slot this
shader called `Pad0` and discarded** — the two ends of the same constant
buffer disagreed about what slot 5 meant, silently, with no VUID and no error.

## Acceptance gates (job instruction, 7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT
carried forward as PASS from the 2026-08-14 log — it describes a pre-v183 tree
with 29 cycles of source change since.

**Operator command that clears the blocker:**

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
