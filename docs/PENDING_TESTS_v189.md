# Pending Tests v189

- commit: docs/PENDING_COMMIT_v189.md
- tester: agent_5_tester (tick-536)
- timestamp: 2026-08-30
- test files produced: **none** (`produces_test_files: no`)

## Why no test file

The change is two `uint32_t` assignments in a GPU test harness's per-frame
render function. Exercising it requires a Vulkan device, an RTX GPU, a built
Debug target and a shell — none available in this runspace (`terminal` denied,
`tirith:unknown`). Writing a C++ unit test that cannot be compiled or run would
be the "test-bug-in-itself" pattern the verifier audits for. The executable test
is the operator recipe at the bottom.

What follows is a **static verification matrix**: 18 rows, each a query or read I
executed this turn against the patched tree, with its real result. Per tick-526,
every query is single-term — no `|` alternation anywhere.

## Matrix

### A. The edit itself

| # | Assertion | Method | Result |
|---|---|---|---|
| 1 | `Bd.OutputWidth` reads `HalfResWidth` | `search_files pattern="Bd\.Output"` | `:872 Bd.OutputWidth = HalfResWidth` | **PASS** |
| 2 | `Bd.OutputHeight` reads `HalfResHeight` | same query | `:873 Bd.OutputHeight = HalfResHeight` | **PASS** |
| 3 | Exactly 3 `Bd.Output*` sites, no stragglers | same query | 3 hits (`:851`, `:872`, `:873`) | **PASS** |
| 4 | No `FB.width` remains on any `Bd.` field | `search_files pattern="FB\.width"` → 15 hits, each inspected | only surviving hit in the bilateral block is `:853`, a **comment**. All other 14 are unrelated (GBuffer `:773`, ReBLUR `:1169`, accumulate `:1204`, blit `:1235`, …) | **PASS** |
| 5 | Edit is inside the bypass guard | read `:845-873` contiguously | `:845 if (!bBypass)` opens; `:872-873` inside | **PASS** |

### B. The new value is valid at the point of use

| # | Assertion | Method | Result |
|---|---|---|---|
| 6 | `HalfResWidth`/`HalfResHeight` are members | `search_files pattern="HalfResWidth"` | declared `:2808` `uint32_t HalfResWidth = 0` | **PASS** |
| 7 | Assigned in resource setup | read `:1560-1563` | `HalfW = W / 2`; `HalfResWidth = HalfW` | **PASS** |
| 8 | **Non-zero at `:872` — the strong form** | `search_files pattern="HalfResWidth"` → `:793` | `:793 Desc.OutputWidth = HalfResWidth` is in the **same function**, 79 lines earlier, driving the GI trace grid. Zero there would empty the GI dispatch | **PASS** |
| 9 | Zero would degrade safely anyway | read `FBilateralDenoisePass.cpp:149-153` | logs a warning and early-returns; no UB | **PASS** |
| 10 | New extent equals the input texture's extent | read `:1564-1566` | `OutputTexture` created `HalfW x HalfH` — exact match | **PASS** |

### C. The defect is really the one described

| # | Assertion | Method | Result |
|---|---|---|---|
| 11 | Grid derives from `OutputWidth` | read `FBilateralDenoisePass.cpp:179-180` | `dispatchX = (outputW + 7) / 8` | **PASS** |
| 12 | `TexelSize` derives from the same | read `:158-159` | `ConstantsData[0] = 1.0f / outputW` | **PASS** |
| 13 | Shader inverts `TexelSize` for bounds | read `BilateralDenoise_cs.hlsl:60,62,87` | `outputSize = uint2(uint(1.0/TexelSize.x), …)`; early-out `:62`; 5x5 bounds `:87` | **PASS** |

### D. Nothing validated can move

| # | Assertion | Method | Result |
|---|---|---|---|
| 14 | Only two `AccumInput` assignments | `search_files pattern="AccumInput"` → 8 hits | `:1111` and `:1168` only | **PASS** |
| 15 | ReBLUR fully overwrites the texture first | read `:1148`, `:1169-1170`, `FReBLURPass.cpp:248-249` | output = `DenoisedTexture` at `FB.width/height`; grid from those | **PASS** |
| 16 | Validator never reads `denoised` | `search_files pattern="denoised"` on `validate_restir_gi.py` | **0 hits**; `:190` requires display/spatial/gi_raw/gbuffer_material | **PASS** |
| 17 | Recipe never reads `denoised` | `search_files pattern="denoised"` on `v176-recipe.sh` | **0 hits**; gate 7 uses `gi_raw` (`:294`, `:299`) | **PASS** |

### E. Traps this lineage has been bitten by

| # | Assertion | Method | Result |
|---|---|---|---|
| 18a | v182 dead-copy trap (patch a shader copy nothing compiles) | this cycle edits no shader; `ShaderMake.cfg:3` does compile `BilateralDenoise_cs.hlsl`, but it is untouched | **N/A — cannot fire** |
| 18b | Sibling caller not collaterally changed | read `TestCornellBoxGI.cpp:1478-1488`, `:867-868`, `:885-886` | passes `CurrentFBInfo.width/height`; both its textures are `GBufferWidth x GBufferHeight`; file not in `files:` | **PASS** |
| 18c | v183/v184/v185 chain not perturbed | `search_files pattern="GBufferScale"` on the test | `:1025`, `:1071` untouched by this hunk (different block, different struct) | **PASS** |

**18/18 PASS.** Rows 8, 14, 16, 17 are the discriminators — each could
independently have failed the cycle. Rows 1-5 are individually weak but stated
separately on purpose: collapsing them into one alternation query returns 0 hits
on this runspace and would read as a false failure.

## What this matrix does NOT establish

It confirms textual and arithmetic properties of the tree. It does **not**
establish that the file compiles, that the target links, that the dispatch is
correct at runtime, or that any pixel is unchanged. **No build, no run, no
image.** Rows are static evidence only.

## Executable test — for the operator

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Expected, and the failure conditions that matter:

1. **Compiles.** `HalfResWidth`/`HalfResHeight` are `uint32_t`, matching
   `FDesc::OutputWidth`. No conversion warning expected.
2. **`display`, `spatial`, `gi_raw`, `gbuffer_material` dumps unchanged** vs. a
   pre-patch run at the same seed. **If `display` moves, this patch is wrong** —
   the entire justification is that the bilateral output is dead. Do not explain
   that away.
3. **`denoised` dump WILL differ** (previously a blur of out-of-bounds zeros over
   3/4 of the frame; now that region is left unwritten). Expected. No gate reads
   it. Not a regression, not an improvement.
4. **No `VUID-VkDescriptorImageInfo-imageLayout-00344`.** This is the real risk
   row: the dispatch is retained for its barrier-flushing side effect. Static
   analysis says the flush is grid-independent (binding set `:167-176` and
   `setComputeState` `:185` both precede `dispatch` `:186`, and neither reads
   `dispatchX/Y`). **If that VUID appears, the static argument was wrong and this
   patch must be reverted**, not patched around.
5. `validate_restir_gi.py <dumps>` on the newest group only.

Note this run also carries v183/v184/v185, which are staged and still awaiting
their single run. **Judge them separately from this patch** — v189 touches a
different pass and cannot affect `ReSTIR summary: M mean=...`.
