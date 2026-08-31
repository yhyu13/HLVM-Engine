# Pending Commit v213

- plan: docs/PENDING_PLAN_v213.md
- files: Engine/Source/Runtime/Public/Renderer/PostProcess/FBilateralDenoisePass.h,
  Engine/Source/Runtime/Private/Renderer/PostProcess/FBilateralDenoisePass.cpp
- source: no bundle — re-derived from source
- target: (no branch; cron does not commit)
- task: `NormalTexture` was documented optional and is required at every level.
  Guard added; contract corrected at all four sites.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: **+5 functional / -0, ~+34 comment / -9**, 2 files. No shader byte
  changed, no cbuffer field changed, no signature changed.

## What changed

| Site | Before | After |
|---|---|---|
| `.h:47` | `NormalTexture; // ... (optional)` | `(required)` + 11-line rationale |
| `.h:32` | "scale derived from DepthTexture **because NormalTexture is optional**" | rewritten — premise removed |
| `.cpp:90` | `// t2 -> 2 (Normal guide - optional)` | `REQUIRED, see FDesc` |
| `.cpp:199` | v205's rationale resting on "declared optional in the header" | rewritten, v205's *finding* preserved |
| `.cpp:155-176` | — | **the guard** (new) |

## The guard, and why it is loud

    if (!Desc.NormalTexture)
    {
        HLVM_LOG(LogPostProcess, err, TXT("... NormalTexture is required ...; output left UNWRITTEN"));
        return;
    }

Per the plan gate's FIX. A quiet `warn`+`return` would have left
`DenoisedTexture` **stale** — it feeds `AccumInput` → `DisplayTexture` → the
`display` dump → `validate_restir_gi.py` — so the downstream pass would consume
the previous frame and the validator would see a plausible image. Today's
behaviour (null reaches `createBindingSet` against an unconditional t2 and fails
loudly) is *more* diagnosable than that. The guard's job is to keep the failure
attributable and name the field, not to make a null survivable.

## Verification of what was NOT disturbed

- **v205's fix is byte-intact**: `if (Desc.DepthTexture)` / `GuideW / outputW` /
  `ConstantsData[5]` all present and unmoved. Its *finding* is preserved in the
  rewritten comment; only its false premise was removed.
- `optional` → **3 hits total** across both files, all three the new
  explanatory text. Zero stale claims remain.
- Both live consumers unaffected: `TestReSTIR_GI_Temporal.cpp:883` and
  `TestCornellBoxGI.cpp:1481` each assign a real texture, so the guard cannot
  fire for either. **The behaviour of both tests is unchanged.**

## LSP diagnostics — mapped, not dismissed (v207's rule)

The header edits returned clang errors. **All pre-existing; none edit-caused.**
Two independent proofs:

1. **Every error is an unresolved `nvrhi`, `FString` or `uint32_t`** — the
   file's own `#include "Core/String.h"` and `<nvrhi/nvrhi.h>` at `:17-18` are
   unresolved in a standalone parse with no project include paths. My edits
   introduced **no declaration of any kind** — comment lines and one word.
2. **The count fell 14 → 11 across an edit that only ADDED comment lines.** A
   real regression cannot reduce the error count; the line numbers also lagged
   the file (`[53:53] FString` pointed at `float SpatialSigma`). The
   diagnostics are unstable standalone-parse artifacts.

The `.cpp` edits — including the one adding a real `if` — returned **no
diagnostics at all**, which is the controlled positive for the above.

## Plan Deviations

**None.** The plan as revised at the FIX gate is what was implemented. Two
deviations were considered and rejected:

1. **Adding a dummy normal** (`FReBLURPass`'s idiom). Rejected in the plan and
   re-confirmed here: `FReBLURPass.cpp:289` documents its dummy as `(0,0,1)`,
   which is safe *there* because its shader tolerates a constant guide. Here
   `normalWeight` is `dot(n1,n2)`-driven and a constant guide makes every
   kernel weight identical — depth-only filtering with no diagnostic. The
   header now records this explicitly so the idiom is not carried across.
2. **Also patching the three shader copies to gate `t_Normal`.** Rejected: it
   engages the v182 dual-copy hazard for no gain, since the guard makes the null
   case unreachable before any dispatch.

## Severity — stated without inflation

**LATENT. This cycle moves no pixel and clears no acceptance gate.** Both
consumers already pass a real guide. The value is that the contract stops
advertising an affordance that nothing behind it can honour.
