# Pending Tests v223

- commit: docs/PENDING_COMMIT_v223.md
- tester: agent_5_tester (tick-now, autonomous invocation #574, this turn)
- timestamp: 2026-08-21
- nature: file-only re-derivation. **Nothing built, run, or executed.** Every row re-queried independently rather than read from the commit or plan markers.

|| # | Row | Method | Result |
||---|---|---|---|---|
|| 1 | Misleading phrase gone | `search_files pattern="FCommonRenderPasses uses it unless" path=Engine` | **PASS** — 0 hits; the comment block no longer claims `FCommonRenderPasses` selects the shader |
|| 2 | Real mechanism named | `read_file` of patched comment block, lines 26-40 | **PASS** — names `FBilateralDenoisePass::Initialize(..., InShaderDataDir)` as the selector and explicitly states `FCommonRenderPasses::SetShaderDataDir()` "governs BLIT resources only" |
|| 3 | `SetShaderDataDir` zero-caller claim re-derived | `search_files pattern=SetShaderDataDir path=Engine` | **PASS** — 2 hits: the definition at `FCommonRenderPasses.cpp:290` and the patched comment at `BilateralDenoise_cs.hlsl:34`; **zero call sites** in Engine source |
|| 4 | Row 3 positive control | `search_files pattern="BlitTexture" path=Engine` | **PASS** — 17 hits across 11 files, every callsite is `FCommonRenderPasses::BlitTexture`; proves the query shape works on Engine |
|| 5 | Override's sole consumer is Blit init | `read_file` of `FCommonRenderPasses.cpp:319-322` and the body of `SetShaderDataDir` at `:290-298` | **PASS** — `:319` calls `GetShaderDataDir()`, `:322` calls `InitBlitResources(Device, ShaderDataDir)`; the function's own comment at `:293` states "Reset initialization so next BlitTexture will reinitialize" |
|| 6 | `g_ShaderDataDirOverride` reader scope | `search_files pattern="g_ShaderDataDirOverride" path=Engine` | **PASS** — 4 hits, all in `FCommonRenderPasses.cpp` (write site `:292`, read sites `:67`/`:71` inside `GetShaderDataDir`, and one setter call). Reader scope is Blit only. |
|| 7 | `FBilateralDenoisePass::Initialize` signature carries `InShaderDataDir` | `read_file` of `FBilateralDenoisePass.cpp:39-62` | **PASS** — `:39` `bool FBilateralDenoisePass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)`; `:44` stores it; `:48` uses it to compose `BilateralDenoise_cs.sblob` path |
|| 8 | `FBilateralDenoisePass.cpp` does NOT mention `FCommonRenderPasses` | `search_files pattern="FCommonRenderPasses" path=FBilateralDenoisePass.cpp` (scoped) | **PASS** — 0 hits; the two classes are independent |
|| 9 | cbuffer untouched | `read_file` of `BilateralDenoise_cs.hlsl:15-24` | **PASS** — `cbuffer Constants : register(b0) { float2 TexelSize; float DepthSigma; float NormalSigma; float SpatialSigma; float GuideScale; float Pad1; float Pad2; };` byte-identical to pre-patch |
|| 10 | `GB()` function untouched | `read_file` of `BilateralDenoise_cs.hlsl:51-55` | **PASS** — `int2 GB(int2 p) { int s = max(int(GuideScale), 1); return p * s + (s / 2); }` byte-identical |
|| 11 | Patch anchor did NOT match into cbuffer | v203 near-miss geometry check | **PASS** — `old_string` was the literal phrase "FCommonRenderPasses uses it unless" inside the comment block (lines 28-29 pre-patch), not on the line preceding the cbuffer. Returned diff shows only comment lines changed. |
|| 12 | Diff re-read for accidental collateral | `read_file` of file lines 1-25 and 41-57 | **PASS** — pre-cbuffer text and post-cbuffer text byte-identical to pre-patch; `int2 GB(int2 p)` declared on line 51 (was 43 pre-patch), the +8 lines shifted every line below by exactly 8 |
|| 13 | Other copies unchanged | `search_files pattern="FCommonRenderPasses uses it unless" path=Engine/Source/Runtime/Test/TestCornellBoxGI_Data path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data` (split scope) | **PASS** — 0 hits in either (the misleading phrase only existed in the shared copy; the other two don't claim anything about FCommonRenderPasses) |
|| 14 | No build required | engine comment cannot perturb SPIR-V, layout, or bindings | **PASS** by construction — `comments` in HLSL are stripped before SPIR-V; the patch cannot move a pixel |
|| 15 | Card S original premise — `FCommonRenderPasses` has 4 hits zero callers in original | `search_files pattern="FCommonRenderPasses::SetShaderDataDir"` Engine-wide excluding the patch | **PASS** — 1 hit at `FCommonRenderPasses.cpp:290` (definition), 1 hit in the original wrong comment now replaced. Zero callers. |

## Row 8 is the one I want on the record

Card S's claim that the two classes don't interact is what makes the comment wrong by structure, not just by wording. `FBilateralDenoisePass.cpp` is a stand-alone class with its own `Initialize(Device, DataDir)` signature; it does not import `FCommonRenderPasses`, does not call `FCommonRenderPasses::SetShaderDataDir()`, and does not consult `g_ShaderDataDirOverride`. The override literally cannot reach the bilateral pass. Re-derived as: `search_files pattern="FCommonRenderPasses" path=FBilateralDenoisePass.cpp` (the file, not the directory) returned 0 hits.

## Row 12 is the one that matters most

The cbuffer and GB() are byte-identical to pre-patch and the comment-block replacement landed on the literal anchor. v203's near-miss geometry (third patch of v203 cycle deleted three live binding items by anchoring on a comment adjacent to a cbuffer initialiser) was avoided by anchoring on the literal phrase **inside** the comment block rather than on the line preceding the cbuffer.