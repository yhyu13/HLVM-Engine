
# Pending Tests v90
- plan: docs/PENDING_PLAN_v90.md
- commit: docs/PENDING_COMMIT_v90.md

## Part A (4/4 PASS — fresh diagnostic probes at NEW sites)
- A1 PASS: TestReSTIR_GI_Temporal.cpp:410-450 reads `Desc.OutputTexture = OutputTexture;` at line 420. The dispatch's `Desc.OutputTexture` parameter is the same `OutputTexture` member created at line 937.
- A2 PASS: TestReSTIR_GI_Temporal.cpp:935-960 reads `OutputTexture = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, "GIRawHDR");` at line 937. RGBA32_FLOAT, UnorderedAccess, GIRawHDR debugName.
- A3 PASS: TestReSTIR_GI_Temporal.cpp:1620-1660 reads `DumpRGBA32FTexture(OutputTexture, TXT("gi_raw"), dir, /*bNormalizePerChannel=*/true);` at line 1650. The dumper's source handle is the same `OutputTexture` member from A2/A1.
- A4 PASS: FGIPass.cpp:634 `OutputTexture = Desc.OutputTexture;` is the **namespace GI** local member (NOT the test class's OutputTexture). Caches the handle for the next pass to consume; does NOT alias the test's OutputTexture member.

## Part B (8/8 UNVERIFIED — terminal-blocked, this is the constant for this cron runspace)
- B1: clean Debug build — UNVERIFIED
- B2: fresh HLVM_DUMP_RGI=1 run with HLVM_RGI_ACCUM=8 — UNVERIFIED (terminal blocked)
- B3: zero `Cannot open a command list that is already open` in fresh log — UNVERIFIED
- B4: zero Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 in fresh log — UNVERIFIED
- B5: validator passes newest dump group only — UNVERIFIED
- B6: vision inspection of newest display PNG — UNVERIFIED (no vision tool)
- B7: vision inspection of newest gi_raw PNG — UNVERIFIED
- B8: structural 4-check validator (black-pixel ratio, color variance, cell variance, temporal stability) — UNVERIFIED

## Cycle-shape note
v90 deliberately did NOT recyle v25-v89 sites. v90's Part A probes (A1+A2+A3+A4) are at NEW diagnostic sites distinct from v89's 3 binding-side sites AND from v25-v88's 22 binding-side / encoding / sentinel sites.

## What the test surfaced
**v90 narrows v89's three-hypothesis list to two hypotheses (eliminating (iii) dumper-side mismatch)** without terminal access. The bug is now either (i) dispatch-drops OR (ii) shader-side write skipped. Disambiguation requires 10 seconds of terminal evidence per `PIPELINE_BLOCKER_2026-07-28.md` v3 ENTER/EXIT log + per-channel min/max of gi_raw dump output. v90 honors the cron prompt's "do not fabricate" by stating UNVERIFIED honestly for Part B and stating the terminal-required disambiguation explicitly.
