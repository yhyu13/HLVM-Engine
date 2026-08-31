# Ad-hoc Verification — v188 (tick-535)

**Status: ad-hoc structural verification, hand-executed. NOT suite green.**

## Script

`/tmp/hermes-verify-v188.py` (6.5 KB, 5 sections, 36 assertions). Written this
turn, syntax-clean (`write_file`/`patch` lint OK on every revision).

## It could not be executed

`terminal` is denied at the **tool** level by tirith, not by command-pattern
matching. Established by escalating probes to the simplest possible cases this
turn:

| Probe | Result |
|---|---|
| `echo probe-ok; date -u; pwd; git log --oneline -3` | `pending_approval / tirith:unknown / exit_code -1` |
| `/usr/bin/true` (absolute path, no args, no metacharacters, `workdir=/tmp`) | identical |
| `python3 /tmp/hermes-verify-v188.py` | identical |

`smart_denied: false` on all three. `python3` is unreachable, as is `./Build.sh`.
The runtime fired `same_tool_failure_warning; count=3`, so I stopped retrying and
hand-executed instead.

## Two latent bugs found in the script while reviewing it

Worth recording, because both are the **vacuous-pass** class this lineage keeps
hitting (cf. the tick-526 alternation defect):

1. **`kind == "float"` filter against a `TFP32` header.** Check [2]'s marshaller
   -order assertion built its expected sequence from members whose kind is
   literally `float`. `FReSTIRPass.h` declares `TFP32`, so the filter matched
   **nothing** and the assertion would have compared `[...] == []` — passing only
   if the marshaller were also empty, and otherwise failing for the wrong reason.
   Rewritten to build the expected sequence from the header's own member list,
   expanding `[N]` arrays and excluding the two `memcpy`'d matrices.
2. **No floor on the parse.** Checks [1]'s equality comparisons are all satisfied
   trivially if the struct regex matches nothing (`[] == []`). Added an explicit
   `len == 14` guard on all three parses, plus element-**width** comparison
   (C++ `TFP32[2]` vs HLSL `float2` are the same two floats but different
   spellings, so names alone were under-checking kind agreement).

A script that passes vacuously is worse than no script. Both are fixed in the
file left on disk.

## Hand-executed results

Every row below is a `read_file` / `search_files` query run this turn.

### [1] Three-way declaration agreement

Full transcription of all three structs:

| # | Member | `FReSTIRPass.h:42-59` | Cornell `:6-40` | Sibling `:24-42` |
|---|---|---|---|---|
| 1 | InverseCurrViewProj | `TFP32 [16]` | `float4x4` | `float4x4` |
| 2 | PrevViewProj | `TFP32 [16]` | `float4x4` | `float4x4` |
| 3 | OutputSize | `TFP32 [2]` | `float2` | `float2` |
| 4 | RcpOutputSize | `TFP32 [2]` | `float2` | `float2` |
| 5 | FrameIndex | `TFP32` | `float` | `float` |
| 6 | MaxM | `TFP32` | `float` | `float` |
| 7 | DepthThreshold | `TFP32` | `float` | `float` |
| 8 | NormalThreshold | `TFP32` | `float` | `float` |
| 9 | DebugVis | `TFP32` | `float` | `float` |
| 10 | **SceneYaw** | `:51` | `:36` | `:33` |
| 11 | **PrevSceneYaw** | `:52` | `:37` | `:34` |
| 12 | **NearPlane** | `:57` | `:38` | `:40` |
| 13 | **FarPlane** | `:58` | `:39` | `:41` |
| 14 | **GBufferScale** | `:59` | `:40` | `:42` |

- member count 14 / 14 / 14 — **PASS** (parse floor satisfied, not vacuous)
- names identical in order across all three — **PASS**
- element widths `[16,16,2,2,1,1,1,1,1,1,1,1,1,1]` on all three — **PASS**
- tail == the five v188 scalars — **PASS**
- no array-kind member in either HLSL copy — **PASS**

### [2] Offset walk vs the marshaller

Scalars pack tightly; matrices occupy 0-31. Derived: `OutputSize` 32-33,
`RcpOutputSize` 34-35, `FrameIndex` 36, `MaxM` 37, `DepthThreshold` 38,
`NormalThreshold` 39, `DebugVis` 40, `SceneYaw` 41, `PrevSceneYaw` 42,
`NearPlane` 43, `FarPlane` 44, `GBufferScale` 45.

Marshaller `FReSTIRPass.cpp:428-456`: two `memcpy` of 64 B (0-31), then
`offset++` writes in exactly that order, ending `NearPlane`/`FarPlane`/
`GBufferScale` at `:454-456`. **Write order == declaration order — PASS.**
No member is left unwritten — **PASS**.

Struct needs 46 floats; buffer is `float ConstantsData[64]` (`:424`) and
`BufferDesc.byteSize = 256` (`:317`). **Fits, 18 floats spare — PASS.**

### [3] Call-site assignment coverage

Queried individually (never with `|`, per tick-526):

| Field | Site |
|---|---|
| SceneYaw | `TestCornellBoxGI.cpp:1579 = 0.0f` |
| PrevSceneYaw | `:1580 = 0.0f` |
| NearPlane | `:1585 = 0.01f` |
| FarPlane | `:1586 = 10.0f` |
| GBufferScale | `:1592 = 1.0f` |

5/5 — **PASS**. Near/far match this test's own projection at `:1276`
`perspectiveLH_ZO(radians(90.0f), aspect, 0.01f, 10.0f)` — **PASS**.

### [4] Read-inertness

`gConstants\.` over the edited Cornell shader → 7 hits (`:63/:89/:94/:98/:126/
:156/:167`) on `OutputSize`, `RcpOutputSize`, `InverseCurrViewProj`,
`PrevViewProj`, `DepthThreshold`, `NormalThreshold`, `FrameIndex`, `MaxM`.
**Intersection with the five new fields is empty — PASS.** No `Pad` member
survives; no `int2 GB` helper introduced (0 hits) — **PASS**.

### [5] Traps

- `TestCornellBoxGI_Data/ShaderMake.cfg:6` compiles the edited shader (v182
  dead-copy trap) — **PASS**
- `v188` over `TestReSTIR_GI_Temporal_Data/` → 0 hits — **PASS**
- `v188` over `FReSTIRPass.h` → 0 hits — **PASS**
- `v188` over `FReSTIRPass.cpp` → 0 hits — **PASS**

**36/36 assertions hold.**

### [6] slangc-acceptance evidence (the load-bearing unknown, partially reduced)

This cannot be closed without a build, but it can be narrowed by an artifact
that already exists.

The five-scalar tail v188 adds to the Cornell copy is **byte-identical in kind
and order** to the tail the sibling copy has carried since v184:

```
TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:32-42
    float DebugVis; float SceneYaw; float PrevSceneYaw;
    float NearPlane; float FarPlane; float GBufferScale;
TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:14,36-40   (after v188)
    float DebugVis; float SceneYaw; float PrevSceneYaw;
    float NearPlane; float FarPlane; float GBufferScale;
```

The sibling is compiled by `TestReSTIR_GI_Temporal_Data/ShaderMake.cfg:6`, and
**`TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.sblob` exists on disk and is
newer than its `.hlsl`** (`search_files target=files` orders by mtime; in that
directory the `.hlsl` sorts ahead of the `.sblob`, so the blob postdates the
source). A `.sblob` is ShaderMake's output — it exists only if slangc accepted
the source that produced it.

**Therefore slangc has already compiled this exact struct declaration.** What
remains unverified for Cornell is not the tail's syntax but whether the
surrounding file still compiles as a whole and whether reflection places the
fields where the offset walk predicts. Note the Cornell directory also has a
`ReSTIR_Temporal_cs.sblob`, but it is **stale** — it predates the v188 edit and
must be regenerated; it is not evidence about the patched source.

This is inference from an existing build artifact, not a build. It downgrades
the risk; it does not discharge it.

## What this does and does not establish

**Does:** the C++ header, both HLSL copies, the marshaller's write order, and
the call-site assignments are mutually consistent; the patch is read-inert and
confined to two files.

**Does not:** that slangc accepts the widened struct, that `TestCornellBoxGI`
compiles, that reflection places the fields at floats 41-45, or that any pixel is
unchanged. **These are text and arithmetic properties, not execution.**

## Cleanup

`/tmp/hermes-verify-v188.py` left on disk — removing it requires shell, which is
blocked. It is ready to run verbatim once an operator has a terminal, and its
exit code is the check:

```
python3 /tmp/hermes-verify-v188.py && \
  ./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild --Test
```
