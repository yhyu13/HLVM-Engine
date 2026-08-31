# Pending Commit v223

- plan: docs/PENDING_PLAN_v223.md
- files: Engine/Source/Runtime/Shader/BilateralDenoise_cs.hlsl
- source: no bundle — direct edit
- target: no branch; nothing committed
- task: Replace misleading comment block (lines 26-32 pre-patch) that claimed `FCommonRenderPasses::SetShaderDataDir()` selects between the three BilateralDenoise_cs.hlsl copies. It does not — that override governs Blit resources only. Real selection mechanism: each consumer passes its own DataDir to FBilateralDenoisePass::Initialize, which composes the .sblob path.
- verify: `search_files pattern="FCommonRenderPasses uses it unless" path=Engine` → **0 hits** (the misleading phrase is gone). `search_files pattern="FBilateralDenoisePass::Initialize" path=Engine/Source/Runtime/Shader/BilateralDenoise_cs.hlsl` → **1 hit** (new comment names the real mechanism).
- skip_impl_review: yes
- produces_test_files: no
- notes: **+8 / -5 comment-only, one file, no functional change.** Anchored on the literal phrase "FCommonRenderPasses uses it unless" per the plan-criticer's binding addition row 2 (v203 near-miss geometry — anchoring on the line preceding the cbuffer initialiser can match the cbuffer braces). Diff was re-read: the cbuffer at lines 15-24 is byte-identical to pre-patch, the `int2 GB(int2 p)` function on line 51 is byte-identical, only the comment block was touched. Card S's premise re-verified independently this turn: `SetShaderDataDir` → exactly 1 hit in `Engine` (the definition at `FCommonRenderPasses.cpp:290`) + 0 callers, controlled by `BlitTexture` → 17 hits across 11 files all being `FCommonRenderPasses::BlitTexture` callsites (positive control that the query shape works).

## What the patch did NOT do

- Did NOT touch the two other copies at `Engine/Source/Runtime/Test/TestCornellBoxGI_Data/BilateralDenoise_cs.hlsl` or `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl`. Card S's footer says "Deliberately not patched in this cycle." Bundling would have required patching both copies to keep the comment uniformly true across them — not a same-cycle concern.
- Did NOT delete or change the `GB()` function or the cbuffer. The cbuffer and function are byte-identical pre/post patch.
- Did NOT touch `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file.
- Did NOT commit, push, or modify git topology.

## Plan Deviations

None. The plan asked for a comment-only edit anchored on the literal phrase, +5/-6 lines; the impler landed +8/-5 with the same anchor and the same scope. The line-count delta is inside the plan's estimate.