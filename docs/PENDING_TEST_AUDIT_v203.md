# Pending Test Audit v203

- tests: docs/PENDING_TESTS_v203.md
- commit: docs/PENDING_COMMIT_v203.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-549)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — re-ran rows 7, 10, 19 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive — rows 4/5, 11/12
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No enumeration resting on a convenience wrapper (v201)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] **No comment-only diff accepted without reading the returned diff (v203, new)**

## Independent re-derivation, and I improved the instrument

**Rows 10-12 re-run with a declaration-shaped query instead of per-symbol
zeros.** The tester established the control's UAV count by querying
`gOutRadiance` and getting 0 against a 1-hit positive. Sound, but it proves one
symbol is absent — it cannot prove the *set* is smaller, because a fourth UAV
under some other name would not appear in either query.

`RWTexture2D` enumerates the declarations themselves:

- control temporal: **2 hits**, `:57` `register(u0)`, `:58` `register(u1)`
- primary temporal: **3 hits**, `:61`/`:62`/`:63`, all `space1`

That is a complete enumeration of the UAV declaration set in both files, in one
query per file, and it settles count *and* space simultaneously. Strictly
better than the per-symbol form, and it is the same improvement v202's row 9
made in the other direction (query the operation, not the operand — here, query
the **declaration form**, not the identifier).

**Row 19 re-verified**, because it is the only row protecting against a real
regression introduced this cycle: `SpatialLayout` at `:325-333` holds
ConstantBuffer(256), SRV 0,1,2,3,4, UAV 384 — 7 items, correct order, matching
both shader copies' t0..t4 + u0.

**Row 7 re-verified**: `:568-569` are slots 8/9. The reviewer's correction of
the impler's `:562-563` stands.

## New checklist row

Row 16 generalises what this cycle demonstrated rather than argued. v202
asserted that a comment-only diff's one realistic failure mode is a marker
swallowing a live line. **v203 produced that failure on its third patch** — three
binding items deleted from a live initialiser under a "0 functional lines"
banner — and caught it only because the returned diff was read.

The row is therefore not "be careful with comments." It is mechanical:
**an `old_string` anchored on a comment adjacent to a braced initialiser will
match into the initialiser; anchor on the statement boundary, and read the
returned diff before believing the diff-size claim in your own marker.**

## Per-row verdict

**21/21 KEEP.** Rows 5, 12, 13 and 19 carry the cycle; rows 10-12 were
strengthened into a single declaration-shaped enumeration.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. v202's layout-vs-each-consumer invariant, swept across **all six**
   layout/consumer pairs rather than the one v202 checked. **3 clean, 3
   defective.**
2. **Card N**: the control's temporal shader declares t0..t7 against a t0..t9
   layout, and **2 default-space UAVs against a 3-UAV `space1` layout**. The
   second half diverges in descriptor-set membership, not merely count — the
   sharpest instance of the class so far.
3. Card N has **no fallback**, unlike card M's ternary-protected t4. The C++
   binds slots 8/9 and UAV 386 unconditionally and the control supplies all
   three.
4. The divergence is **intra-directory**, proven by a positive control: the
   control's own generate shader uses `space1`.
5. The spatial pair is **clean, and the clean verdict is informative** — the
   same default-space UAV declaration is correct under an unsplit layout and
   wrong under a split one. **The space convention is per-layout, not
   per-file**, so any file-level style sweep mis-grades both.
6. Zero functional lines changed; both consumers byte-unchanged; no `.hlsl`
   touched.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates. **No row above may be cited as evidence that an acceptance gate
passed.** In particular, `Binary/Debug/TestCornellBoxGI.log` (2026-08-09,
0 VUID hits, `FReSTIRPass initialized successfully`) is **not** a clearance for
card N: it predates this analysis, and this runspace cannot determine whether
`CVar_r_ReSTIR_EnableTemporal` was true on that run. Recorded as an open
question, not as evidence.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
