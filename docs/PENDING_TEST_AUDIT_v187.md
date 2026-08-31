# Pending Test Audit v187

- tests: docs/PENDING_TESTS_v187.md
- commit: docs/PENDING_COMMIT_v187.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-534)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static checks)
- [x] No test-bug-in-itself — I re-executed rows 5, 6 and 9 myself
- [x] No source-incomplete-relative-to-test — every row names file + query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A

## Independent re-derivation

**Row 5 re-run — the row the whole cycle rests on.** `FReSTIRSpatialConstants`
over `TestCornellBoxGI.cpp` → exactly 1 hit, `:1607`, carrying `{}`. The
negative form the tester used (search for a declaration *without* braces → 0
hits) and this positive form agree. Sound.

**Row 9 re-run — the v182 dead-copy trap.** Read `TestCornellBoxGI_Data/ShaderMake.cfg`
directly: `:7 ReSTIR_Spatial_cs.hlsl -T cs`. The edited shader is compiled. This
is the check v182 failed to make, and it is correctly load-bearing here because
v187, like v186, touches a shader.

**Net-new check the tester did not make: is the caller set actually closed?**
A verifier that only re-runs the tester's framing adds nothing. `DispatchSpatial`
over the whole Runtime tree → **exactly two call sites**:
`TestCornellBoxGI.cpp:1633` and `TestReSTIR_GI_Temporal.cpp:1056`. Both now pass
a value-initialized struct with `GBufferScale` explicitly assigned (Cornell
`1.0f` at `:1622`; Temporal the computed ratio at `:1051`). **There is no third
caller that could still be feeding the marshaller indeterminate memory** — so
the fix is complete for this struct, not merely applied at the site the card
named. That closure is what makes ALL_KEEP appropriate rather than "applied,
coverage unknown."

## Per-row verdict

12/12 KEEP. Rows 5 and 10 are genuine discriminators (row 5 fails on the
shader-only regression the plan warned about; row 10 fails if Cornell was made
half-res-aware). Rows 9 and 11 are trap-checks. No row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The Cornell spatial cbuffer now agrees field-for-field and kind-for-kind with
   the shared C++ header and the Temporal-test copy.
2. All three Cornell ReSTIR structs are value-initialized, and both callers of
   the shared struct are covered — **four indeterminate floats per frame were
   reaching a GPU-visible buffer in the control test, and no longer are.**
3. The wire image is byte-identical before and after (offsets 0..9 either way),
   so the patch cannot move a Cornell pixel — verified by the packing argument,
   not assumed.
4. The edited shader is on a compiled path.

**The findings that make this cycle worth more than its 6 lines:**

1. **The card was right but incomplete.** It described a declaration mismatch.
   Underneath sat a C++ lifetime defect — reads of indeterminate members — that
   the card did not mention and that is not confined to the spatial struct.
2. **The obvious fix alone would have been a regression.** Adding `GBufferScale`
   to the Cornell shader without the value-init converts a discarded write into
   a *named, readable* field backed by indeterminate memory, in the exact file a
   future v183-style patch would want a `GB()` helper in — where `max(int(s),1)`
   would launder garbage into a plausible wrong answer. The two edits are
   coupled; the ordering matters.
3. **A stale line-number cross-reference was caught at the impl-review gate.**
   The impler's shader comment cited `TestCornellBoxGI.cpp:1620-1621`, but its
   own inserted lines had shifted the target to `:1631-1632`. Fixed to a symbolic
   reference. General lesson worth carrying: **line-number citations between two
   files edited in the same patch are wrong by construction.**

**NOT established — load-bearing:** that either test compiles, that slangc
accepts the edited struct, that reflection places the fields where expected, or
that any pixel is unchanged. No build, no compile, no run, no image.

**Ordering caveat carried forward:** v183 (add `GB()`), v184 (make `GBufferScale`
arrive), v185 (fix its coordinate space) are one dependency chain and must be
judged on a single run. v187 does not touch `TestReSTIR_GI_Temporal_Data/` and
cannot affect that outcome.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183-v187 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | pre-patch only | 0 VUID in the current log, but it predates all five patches |
| 4 | No command-list errors | pre-patch only | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell/python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 are deliberately not
carried forward as PASS: a 2026-08-14 log says nothing about a tree patched on
2026-08-30.

## Blockers (concrete, with evidence)

1. **`terminal` denied categorically by tirith.** Probes this tick: a combined
   `date; git log; ls`, and a bare `echo probe-tick534`. Both returned
   `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1,
   smart_denied: false, allow_permanent: true`. The bare `echo` was chosen to
   rule out command-pattern matching. It is a block on the tool itself.
2. **No vision capability.** Toolset is `patch`, `process`, `read_file`,
   `search_files`, `terminal`, `write_file`. Gate 6 is unreachable even if the
   shell block were lifted.

## Operator action (~6 min)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Read `ReSTIR summary: M mean=...` against the long-standing
`2.93 max=9.0 (MaxM=30)`. **If M does not move with v183+v184+v185 present, the
half-res-reuse hypothesis is wrong and must be recorded as a refutation, not
explained away.** Also re-check the display dump and `validate_restir_gi.py`.

Then, for v187 specifically:

```
./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild --Test
```

Expected: compiles (the struct edit is the load-bearing unknown) and the control
renders **identically** to before — the patch is designed to be byte-inert on the
wire, so any image change in Cornell is a bug in this patch and should be treated
as such.

## Caveat

Single-profile host: all six roles are the same model. Each stage re-derived its
inputs from source rather than inheriting the previous marker's claims — which is
how the impl-review gate caught the stale line reference — but these are
self-checks, not fresh eyes (`six-role-pipeline §Anti-patterns §7`).

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
