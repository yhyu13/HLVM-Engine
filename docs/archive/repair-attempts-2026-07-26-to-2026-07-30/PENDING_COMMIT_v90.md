# Pending Commit v90
- plan: docs/PENDING_PLAN_v90.md
- files: (none — verification only; 0 source-code lines modified)
- source: no bundle
- target: (no branch — cron must not commit/push per cron's rules + HARD INVARIANT)
- task: restir-gi-fix — narrow v89's 3-way downstream-surface hypothesis to 2-way via static-read of dumper-side handle chain in TestReSTIR_GI_Temporal.cpp
- verify: read_file on lines 410-450 (pre-dispatch handle setup), 935-960 (CreateTexture2D creation), 1620-1660 (DumpRGBA32FTexture call sites). 3/3 line-ranges must show same `OutputTexture` member being created + passed to dispatch + passed to dumper.
- skip_impl_review: yes (0 source-code modifications; verification-only cycle; same shape as v89)
- produces_test_files: no
- notes: a NEW Part A probe at a NEW diagnostic site distinct from v25-v89's 22 verified sites. The v90 finding complements the v89 finding by eliminating one of v89's three hypotheses without terminal access.

## Part A probe — dumper-side handle chain (NEW site, v90)

### A1 — TestReSTIR_GI_Temporal.cpp:410-450 — pre-dispatch handle setup
Read via `read_file` window. Exact text: line 420 reads `Desc.OutputTexture = OutputTexture;` (passing the test-class member to the GI pass via the Desc struct). Lines 445-447 log `Desc.OutputTexture` handle via `reinterpret_cast<uintptr_t>` for correlation with downstream logs. PASS — confirms the dispatch's `Desc.OutputTexture` parameter is the same `OutputTexture` member created at line 937.

### A2 — TestReSTIR_GI_Temporal.cpp:935-960 — OutputTexture creation
Read via `read_file` window. Exact text: line 937 reads `OutputTexture = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, "GIRawHDR");`. Format matches `RGBA32_FLOAT`; initial state `UnorderedAccess`; this is the **single** `OutputTexture` member to which the dump call (A3) passes. PASS.

### A3 — TestReSTIR_GI_Temporal.cpp:1620-1660 — dump call sites
Read via `search_files` + `read_file`. Line 1650 reads `DumpRGBA32FTexture(OutputTexture, TXT("gi_raw"), dir, /*bNormalizePerChannel=*/true);`. The handle passed to the dumper is the SAME `OutputTexture` member created at line 937 and dispatched at line 420. PASS — confirms the dumper reads the same texture the dispatch writes.

### A4 (clarification to plan-criticer's gap item 1) — FGIPass.cpp:634 `OutputTexture = Desc.OutputTexture;` is the **namespace GI** local member, NOT the test class's OutputTexture
Read at FGIPass.cpp:634 via `read_file`. Located in the GI namespace's `FGIPass::Render` (line ~634). The local `OutputTexture` member of the GI pass stores a copy of the handle for the next pass to consume; it does NOT reassign the test's member. The test's `OutputTexture` member (created at line 937) and the dumper's call (line 1650) read from the same handle. PASS — confirms no alias mismatch.

### NEW finding (v90 NARROWS v89 finding's hypothesis list)
v89 named 3 candidate downstream surfaces: (i) dispatch-drops, (ii) shader-side write skipped, (iii) dumper-side mismatch (debugger-side). v90's static-read at A1+A2+A3+A4 confirms the dump reads EXACTLY the same texture the dispatch writes. **Hypothesis (iii) is ELIMINATED.**

The bug MUST be one of (i) or (ii). Disambiguation requires terminal evidence:
- v3 ENTER/EXIT logs at FGIPass.cpp (~line 627 ENTER; ~line 631 EXIT) — presence of both confirms dispatch body reached; absence points to (i).
- Per-channel min/max of `gi_raw` from a fresh run — non-zero in dump side vs literal 0.0 indicates the texture is in `CopySrc` state correctly but the write isn't happening (likely payload layout desync per the gpu-rendering-bisect-debug skill anti-pattern) → points to (ii).

10-second terminal probe: parent runs the 4-command recipe per `docs/PIPELINE_BLOCKER_2026-07-28.md`, the log will show v3 ENTER + v3 EXIT + per-channel min/max. Either branch leads to a documented fix.

## Plan Deviations
None.
