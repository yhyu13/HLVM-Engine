# Pending Tests v186

- commit: docs/PENDING_COMMIT_v186.md
- author: agent_5_tester (tick-533)
- timestamp: 2026-08-30
- test type: **file-only static verification.** No build, no shader compile,
  no run, no image. `terminal` is denied categorically on this host.

## Tooling discipline applied

Per the accumulated unsound-pattern findings in this lineage, every query
below uses a single literal term, `path` pointed at a directory or a file, no
`|` alternation, no `[N]` subscripts (parsed as a regex character class), no
`file_glob`. Where a count could be misleading the file was read directly.

| # | Check | Query / method | Expected | Result |
|---|---|---|---|---|
| 1 | C++ side migrated | `pattern="TFP32 Pad"` on `FReSTIRPass.h` | `Pad0`,`Pad1` present; no `Pad[2]` | **PASS** — `:36 Pad0`, `:37 Pad1`, `:72 Pad` (spatial, untouched) |
| 2 | No `Pad[2]` left in the generation struct | `pattern="Pad\[2\]"` on Runtime | 0 hits in `FReSTIRPass.h` generation block | **PASS** — remaining hits are comments + unrelated passes (FTAAPass, FReBLURPass, etc.) |
| 3 | Temporal_Data shader migrated | direct read `:22-28` | two scalars | **PASS** |
| 4 | Cornell shader migrated | `pattern="Pad0"` → `:16` | two scalars | **PASS** |
| 5 | No `float2 Pad` left in ANY generation copy | `pattern="float2 Pad;"` on Runtime | 3 hits, none a generation shader | **PASS** — hits are Cornell *spatial*, SSAOBlur, ExposureAdaptation |
| 6 | Field is read-never (inertness) | `pattern="gConstants.Pad"` on Runtime | 0 | **PASS** |
| 7 | Field is write-never (inertness) | direct read `FReSTIRPass.cpp:354-365` | marshaller stops at `DebugVis` | **PASS** — 9 increments, ends offset 8 |
| 8 | **Edited shader is actually compiled** | direct read of both `ShaderMake.cfg` | `ReSTIR_Generate_cs.hlsl` listed | **PASS** — Temporal cfg `:5`, Cornell cfg `:5` |
| 9 | Negative control: no unrelated file touched | `pattern="v186"` on `TestReSTIR_GI_Temporal_Data/` | exactly 1 hit | **PASS** — only the generation shader |
| 10 | Spatial struct NOT disturbed | direct read `FReSTIRPass.h:70-72` + `ReSTIR_Spatial_cs.hlsl:24-26` | unchanged, still 3-way for the temporal test | **PASS** — `GBufferScale` intact at float 9 |

10/10 PASS.

## Row 8 is the row that matters this cycle

v185 could dismiss the "patched a copy nothing compiles" trap structurally,
because it touched no shader at all. **v186 touches two shaders, so that trap
is back in scope** and had to be tested rather than argued away. Both
`ShaderMake.cfg` files list `ReSTIR_Generate_cs.hlsl` explicitly, so both
edits are on compiled paths.

This row has a second consequence the reviewer's net-new finding depends on:
because `TestCornellBoxGI_Data/ShaderMake.cfg:7` also lists
`ReSTIR_Spatial_cs.hlsl`, the Cornell spatial shader — the one missing
`GBufferScale` — is genuinely built and dispatched, so that desync is live
code, not a dead file.

## Row 5 is a real discriminator, not a tautology

A blanket "did the string disappear" check would have passed even if the
impler had patched only one of the two generation copies. Row 5 enumerates the
*remaining* `float2 Pad;` sites and inspects what they are — which is how the
Cornell **spatial** shader surfaced as a separate live defect rather than
being silently counted as "still fine."

## What these tests do NOT establish

They do not establish that either test builds, that slangc accepts the edited
structs, that the constant buffer reflects at the expected offsets, or that
any rendered pixel is unchanged. Every row above is a source-text property.

For a change to a GPU constant-buffer layout, the verification that counts is
a build plus a reflection dump plus a run. All three are on the far side of
the `terminal` block. The inertness argument (rows 6+7) is strong enough that
a behaviour change would be surprising, but "surprising" is not "verified".

## Blocker evidence

`terminal` probed this tick with a bare `date` — returned
`status: pending_approval, pattern_key: tirith:unknown, exit_code: -1,
smart_denied: false`. Not a command-pattern rejection; the tool itself is
blocked. No vision tool exists in this runspace (`patch`, `process`,
`read_file`, `search_files`, `terminal`, `write_file`), so acceptance gate 6
is unreachable regardless.
