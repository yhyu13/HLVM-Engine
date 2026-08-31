# Pending Tests v188

- commit: docs/PENDING_COMMIT_v188.md
- role: agent_5_tester (tick-535)
- mode: **file-only static verification.** `terminal` denied categorically by
  tirith this tick, so no build, no shader compile, no run, no image. Every row
  below is a query I actually executed this turn, transcribed with its result.
- test files produced: **none** (`produces_test_files: no`)

## Why no test file

The change is a constant-buffer declaration alignment. The only executable test
that could exercise it is `TestCornellBoxGI` itself, which already exists and is
already the acceptance vehicle (`./Build.sh --Config=Debug --Target=TestCornellBoxGI
--Rebuild --Test`). Writing a new C++ test would add a target that cannot be
compiled here either — cost with no signal. Per
`software-development-practices §TDD §Exceptions`, declaration-only changes
verified by an existing target do not get a new test.

## Rows

Each row names the file, the exact query, and the observed result. Per the
tick-526 tooling finding, **no `|` alternation is used** — one query per term.

| # | Check | Query / read | Result | Verdict |
|---|---|---|---|---|
| 1 | Cornell temporal declares `SceneYaw` | read `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:1-40` | `:36 float SceneYaw;` | PASS |
| 2 | ...`PrevSceneYaw` | same read | `:37 float PrevSceneYaw;` | PASS |
| 3 | ...`NearPlane` | same read | `:38 float NearPlane;` | PASS |
| 4 | ...`FarPlane` | same read | `:39 float FarPlane;` | PASS |
| 5 | ...`GBufferScale` | same read | `:40 float GBufferScale;` | PASS |
| 6 | Order matches header exactly | compare `:36-40` vs `FReSTIRPass.h:51-59` | `SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale` both sides | PASS |
| 7 | Order matches sibling copy | vs `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:33-42` | same five, same order | PASS |
| 8 | All five are plain scalars, no array/vector | read `:36-40` | five `float` declarations | PASS |
| 9 | `Pad[3]` gone from this struct | read `:4-41` | no `Pad` member remains | PASS |
| 10 | No `Pad` array in any `ReSTIR_*` shader | `pattern="float Pad\["` over Runtime | 6 hits: 3 comments + `FGBufferFillPass.h:21`, `FToneMappingPass.h:30`, `FContactShadowsPass.h:18` — **none in a ReSTIR shader or `FReSTIRPass.h`** | PASS |
| 11 | No shader reads `gConstants.Pad` | `pattern="gConstants.Pad"` over Runtime | 0 hits | PASS |
| 12 | Cornell reads none of the five (read-inert) | `pattern="gConstants\."` over that file | 7 hits `:63/:89/:94/:98/:126/:156/:167`, all on OutputSize/RcpOutputSize/InverseCurrViewProj/PrevViewProj/DepthThreshold/NormalThreshold/FrameIndex/MaxM | PASS |
| 13 | `SceneYaw` assigned at call site | `pattern="TempConstants.SceneYaw"` | `TestCornellBoxGI.cpp:1579 = 0.0f` | PASS |
| 14 | `NearPlane` assigned | `pattern="TempConstants.NearPlane"` | `:1585 = 0.01f` | PASS |
| 15 | `GBufferScale` assigned | `pattern="TempConstants.GBufferScale"` | `:1592 = 1.0f` | PASS |
| 16 | `NearPlane`/`FarPlane` match Cornell's own projection | `pattern="glm::perspective"` | `:1276 perspectiveLH_ZO(radians(90), aspect, 0.01f, 10.0f)` — 0.01/10.0 agree with rows 14 and the `FarPlane` line | PASS |
| 17 | `GBufferScale = 1` is correct for this call site | read `TestCornellBoxGI.cpp:1585-1586` region | `TempDesc.OutputWidth/Height = CurrentFBInfo.width/height`; `:1578-1579` bind full-res `GBufferDepthTexture`/`GBufferNormalsTexture` ⇒ ratio 1 | PASS |
| 18 | No `GB()` helper introduced into Cornell | `pattern="int2 GB"` over `TestCornellBoxGI_Data` | 0 hits | PASS |
| 19 | Edited shader is on a compiled path (v182 trap) | read `TestCornellBoxGI_Data/ShaderMake.cfg` | `:6 ReSTIR_Temporal_cs.hlsl -T cs` | PASS |
| 20 | Struct fits the constant buffer | read `FReSTIRPass.cpp:317`, `:424` | `byteSize = 256`; `float ConstantsData[64]` = 256B; struct 45 floats = 180B | PASS |
| 21 | Scope fence: sibling data dir untouched | `pattern="v188"` over `TestReSTIR_GI_Temporal_Data` | 0 hits | PASS |
| 22 | Scope fence: shared pass untouched | `pattern="v188"` over `Private/Renderer/PostProcess` | 0 hits | PASS |

**22/22 PASS.**

## Rows that are genuine discriminators

Rows 6/7 fail if the impler had followed card C's literal three-scalar
prescription (the tail would read `NearPlane, FarPlane, GBufferScale` against a
header that has `SceneYaw, PrevSceneYaw` first). Row 12 fails if any read of the
new fields was added, which would break the cannot-move-a-pixel claim. Row 19 is
the v182 dead-copy trap. Rows 21/22 fail if the v183-v185 chain was perturbed.

Rows 1-5 are individually weak but are stated separately on purpose: the
alternation form that would have collapsed them into one query **returns 0 hits
on this runspace** (tick-526), and a single collapsed row would have read as a
failure.

## What these rows do NOT establish

That slangc accepts the widened struct. That `TestCornellBoxGI` compiles. That
reflection places the five fields at floats 41-45. That any pixel is unchanged.
All four require a build. **The static rows are consistent with the fix being
correct; they are not evidence that it works.**
