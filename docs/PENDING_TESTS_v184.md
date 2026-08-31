# Pending Tests v184

- commit: docs/PENDING_COMMIT_v184.md
- tester: agent_5_tester (tick-531)
- timestamp: 2026-08-30
- test type: **file-only static verification** (no build, no run — see §Why)

## Why static

Every `terminal` invocation in this runspace is refused by tirith. Three
probes this tick, spanning the trivial and the substantive:

| probe | result |
|---|---|
| `date` (bare) | `pending_approval / tirith:unknown / exit_code -1` |
| `ls docs/ && git log` | same |
| `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` | same |

The runtime fired `same_tool_failure_warning; count=3` after the third. This
is not command-specific — a bare `date` is refused identically to a build.
Shell does not exist here. The enumerated toolset (`patch`, `process`,
`read_file`, `search_files`, `terminal`, `write_file`) also contains no
vision capability, so image inspection is structurally unavailable
independent of the shell block.

**Tool-soundness discipline applied** (per tick-526): `search_files` returns
a vacuous 0 for any pattern containing `|` alternation. One probe this tick
accidentally used `pattern="NearPlane|FarPlane"` → **0 hits**, then the same
query split into two single-term searches → **1 hit each**. Every row below
uses single-term patterns with `path` at a file or directory. No row's
evidence rests on an alternation query.

## Test rows

| # | Check | Method | Result |
|---|---|---|---|
| 1 | No dangling `.Pad[` consumer anywhere in Runtime | `search_files path=Engine/Source/Runtime pattern="\.Pad\["` | **PASS** — 0 hits (was 6 across 3 files pre-patch) |
| 2 | Test writes migrated | `pattern="NearPlane"` / `"FarPlane"` on `TestReSTIR_GI_Temporal.cpp` | **PASS** — `:976 TC.NearPlane = 0.001f`, `:977 TC.FarPlane = 50.0f` |
| 3 | Marshaller migrated | `pattern="NearPlane"` on `FReSTIRPass.cpp` | **PASS** — `:454 ConstantsData[offset++] = Constants.NearPlane` |
| 4 | Shader reads migrated | `read_file ReSTIR_Temporal_cs.hlsl` | **PASS** — `:146-147` read `gConstants.NearPlane` / `.FarPlane` |
| 5 | No array remains in the temporal cbuffer | `read_file ReSTIR_Temporal_cs.hlsl:22-43` | **PASS** — only `float4x4`/`float2`/scalars; `float4x4` is register-aligned and unaffected by the array rule |
| 6 | Field order parity, element-by-element | 3-way read of `.h:33-50` / `.hlsl:24-42` / `.cpp:428-455` | **PASS** — 14/14 positions identical, incl. `NearPlane` → `FarPlane` → `GBufferScale` |
| 7 | Edited shader is the compiled one | `read_file ShaderMake.cfg` | **PASS** — `:6 ReSTIR_Temporal_cs.hlsl -T cs` |
| 8 | **Negative control** — Cornell sibling untouched | `search_files path=TestCornellBoxGI_Data pattern="NearPlane"` | **PASS** — 0 hits; sibling still `float Pad[3]`, unread |
| 9 | **Negative control** — spatial pass NOT over-converted | `read_file ReSTIR_Spatial_cs.hlsl:16-27` | **PASS** — no array, `GBufferScale` at float 9 both sides; correctly left alone |
| 10 | Buffer capacity still sufficient | count vs `ConstantsData[64]` | **PASS** — 46 floats high-water, 18 spare |

10/10 PASS.

## Rows 8 and 9 are the real discriminators

Rows 1-7 confirm the change was applied. Rows 8 and 9 are the ones that could
have failed and would have caught a genuine defect:

- **Row 8** catches over-application. The Cornell test shares
  `FReSTIRPass.cpp`; had the impler "helpfully" renamed its shader struct to
  match, that test's cbuffer would have desynced from a marshaller it shares.
  0 hits confirms it did not.
- **Row 9** catches the symmetric-fix error. The obvious-looking move is to
  apply the same scalar treatment to the spatial struct. That would be wrong
  — the spatial struct has no array and is already correct, so "fixing" it
  risks shifting a field that currently lines up. Confirmed untouched.

## What these tests do NOT establish

They establish that the **source is internally consistent**. They establish
nothing about the rendered output.

Specifically unverified: that it compiles under slangc; that the SPIR-V
reflection puts `NearPlane` at float 43; that `M mean` rises; that the
display image or `validate_restir_gi.py` improve rather than regress. The
packing rule is applied here from the HLSL specification and from
by-hand offset arithmetic — **not** from a reflection dump. A `spirv-reflect`
or `slangc -reflection` output would be the decisive artifact and is not
obtainable in this runspace.

**Falsifiable prediction, to be reported honestly either way:** with
`GBufferScale` now arriving as 2.0 instead of 0 (clamped to 1), and near/far
as 0.001/50.0 instead of 50.0/0.0, `ReSTIR summary: M mean` should rise
substantially from `2.93` toward `MaxM=30`. **If it does not move, the
half-res-mismatch hypothesis underlying v183+v184 is wrong** and must be
recorded as a refutation, not rationalised.

## Operator command that decides it (~5 min)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Then read one line: `ReSTIR summary: M mean=...`. Compare against `2.93`.
Also re-check the display dump and `validate_restir_gi.py` — both v183 and
v184 are production-path and can regress the image.
