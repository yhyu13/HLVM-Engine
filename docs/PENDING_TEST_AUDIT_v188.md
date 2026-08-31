# Pending Test Audit v188

- tests: docs/PENDING_TESTS_v188.md
- commit: docs/PENDING_COMMIT_v188.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-535)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static checks)
- [x] No test-bug-in-itself — I re-executed rows 6, 10, 12 and 19 myself
- [x] No source-incomplete-relative-to-test — every row names file + query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A

## Independent re-derivation

**Row 6/7 re-run — the row the whole cycle rests on.** Read the edited struct
`:4-40` and the header `FReSTIRPass.h:44-59` side by side. Twelve post-matrix
members, same names, same order, same kinds. Critically, the tail is
`SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale` — **not** the
three-scalar tail card C prescribed. Had the impler executed the card verbatim,
this row fails. It is a real discriminator, not a restatement.

**Row 10 re-run — the row the impl-review gate had to correct.** `float Pad\[`
over Runtime → 6 hits, and I re-read all three real declarations rather than
accepting the triage: `FGBufferFillPass.h:21`, `FToneMappingPass.h:30`,
`FContactShadowsPass.h:18` are each the trailing member of a
`static_assert(sizeof(...) == 256)` struct, and `FContactShadowsPass.cpp:152`
`memcpy`s the struct whole. The v184 bug needs a hand-written flat-offset writer
*and* a field behind the array; these have neither. Triage sound.

**Row 19 re-run — the v182 dead-copy trap.** `TestCornellBoxGI_Data/ShaderMake.cfg:6
ReSTIR_Temporal_cs.hlsl -T cs`. The edited shader is compiled. Correctly
load-bearing here, as in v186 and v187, because this cycle touches a shader.

**Net-new check the tester did not make: is the caller set closed?**
`DispatchTemporal` over the whole Runtime tree → **exactly two call sites**:
`TestCornellBoxGI.cpp:1612` and `TestReSTIR_GI_Temporal.cpp:1007`. The sibling
assigns all five (`:993-994` yaws, `:998-999` planes, `:1003` scale per v183);
Cornell now assigns all five (`:1579/:1580/:1585/:1586/:1592`). **There is no
third caller still feeding the marshaller unassigned members of this struct.**
That closure — the same check that made v187 ALL_KEEP rather than "applied,
coverage unknown" — is what makes ALL_KEEP appropriate here.

Combined with v187 (which closed `FReSTIRSpatialConstants` across both its
callers) and v186 (`FReSTIRConstants`), **all three ReSTIR constant structs are
now name-, kind- and coverage-aligned across every caller and every shader copy.
The v184 defect class is closed at the source level** — subject entirely to the
build caveat below.

## Per-row verdict

22/22 KEEP. Rows 6, 7, 12, 19, 21, 22 are genuine discriminators. Rows 1-5 are
individually weak but correctly stated separately: the alternation form that
would collapse them returns 0 hits on this runspace (tick-526), so a collapsed
row would have read as a false failure. No row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. Cornell's temporal cbuffer agrees field-for-field and kind-for-kind with the
   shared C++ header and the sibling copy across all twelve post-matrix members.
2. All five previously-unwritten fields are explicitly assigned, with values
   derived from Cornell's own call site (`:1276` projection, `:1585-1586`
   dispatch extents) rather than copied from the sibling.
3. The patch is **read-inert**: the shader consumes none of the five, so no
   Cornell pixel can move. Deliberately not claimed byte-inert — unlike v187,
   floats 41-45 do change on the wire.
4. The edited shader is on a compiled path, and the struct fits the 256-byte
   buffer with 64 bytes to spare.
5. Both callers of the struct are covered; the fix is complete for this struct.

**The findings that make this cycle worth more than its 10 lines:**

1. **The card's prescribed fix was wrong, and executing it would have
   re-introduced the exact bug it names.** Card C said replace `Pad[3]` with
   three scalars; the Cornell copy was short by *five* — it never had the
   Phase-C `SceneYaw`/`PrevSceneYaw` either. The card's patch would have named
   floats 41/42/43 `NearPlane`/`FarPlane`/`GBufferScale` while C++ writes
   `SceneYaw`/`PrevSceneYaw`/`NearPlane` there: three actively-misleading
   declarations, produced by a patch whose stated purpose is preventing exactly
   that. **This is the second consecutive cycle where the card was right about
   the symptom and wrong about the remedy** (v187: right mismatch, missed the
   coupled lifetime defect). General lesson: cards opened at a review gate
   describe what was visible from that gate; the next planner must re-derive the
   fix from source rather than execute the prescription.
2. **An impler assertion was false and was caught at the impl-review gate.**
   The commit marker claimed `float Pad\[` → 0 hits; the real count is 6. The
   conclusion survived triage but the stated evidence did not. Carried forward
   as a rule: **an impler asserting a query result must have run that exact
   query** — a near-miss pattern returning a different count is worse than no
   evidence, because it reads as verified.
3. **The tick-526 alternation defect earned its keep.** The combined
   five-field assignment query returned 0 hits and would have read as "none
   assigned"; the five separate queries returned 5/5. Any tick using `|` in a
   `search_files` pattern is producing vacuous negatives.

**NOT established — load-bearing:** that slangc accepts the widened struct, that
`TestCornellBoxGI` compiles, that reflection places the fields at floats 41-45,
or that any pixel is unchanged. No build, no compile, no run, no image.

**Ordering caveat carried forward:** v183 (add `GB()`), v184 (make
`GBufferScale` arrive), v185 (fix its coordinate space) are one dependency chain
and must be judged on a single run. v188 does not touch
`TestReSTIR_GI_Temporal_Data/` (row 21: 0 hits) or `FReSTIRPass.{h,cpp}`
(row 22: 0 hits) and cannot affect that outcome.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183-v188 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | pre-patch only | 0 VUID in the current log, but it is dated 2026-08-14 and predates all six patches |
| 4 | No command-list errors | pre-patch only | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell/python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 are deliberately not
carried forward as PASS: a 2026-08-14 log says nothing about a tree patched on
2026-08-30.

## Blockers (concrete, with evidence)

1. **`terminal` denied categorically by tirith.** Two probes this tick: a
   compound `echo probe-ok; date -u; pwd; git log --oneline -3`, and a bare
   `/usr/bin/true` with absolute path, no arguments, no shell metacharacters,
   `workdir=/tmp`. Both returned `status: pending_approval, pattern_key:
   tirith:unknown, exit_code: -1, smart_denied: false, allow_permanent: true`.
   The minimal probe rules out command-pattern matching — it is a block on the
   tool itself. `python3` and `./Build.sh` are therefore unreachable.
2. **No vision capability.** Toolset this session: `patch`, `process`,
   `read_file`, `search_files`, `terminal`, `write_file`. Gate 6 is unreachable
   even if the shell block were lifted.

## Operator action (~8 min)

```
./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild --Test
```

Expected: compiles (the widened struct is the load-bearing unknown) and the
control renders **identically** to before — the patch is read-inert, so any
image change in Cornell is a bug in this patch and should be treated as such,
not explained away.

Then the v183+v184+v185 chain, which is still awaiting its single run:

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Read `ReSTIR summary: M mean=...` against the long-standing
`2.93 max=9.0 (MaxM=30)`. **If M does not move with v183+v184+v185 present, the
half-res-reuse hypothesis is wrong and must be recorded as a refutation, not
explained away.**

## Caveat

Single-profile host: all six roles are the same model. Each stage re-derived its
inputs from source rather than inheriting the previous marker's claims — which
is how the plan gate caught the card's wrong prescription and how the
impl-review gate caught the false query result — but these are self-checks, not
fresh eyes (`six-role-pipeline §Anti-patterns §7`).

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
