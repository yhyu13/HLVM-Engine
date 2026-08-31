# Pending Test Audit v229

- tests: docs/PENDING_TESTS_v229.md
- commit: docs/PENDING_COMMIT_v229.md
- verdict: SOME_RELAX
- verifier: agent_6_testing_verifier (this tick)
- timestamp: 2026-08-22 (this turn)

## Broken-pattern audit

This cycle has NO new test files under `Engine/Source/Runtime/Test/` or `Engine/Source/Common/Test/`. The 22 file-only rows in `docs/PENDING_TESTS_v229.md` are grep counts on the patch itself, not new test functions. The 5-pattern audit applies to test *files*, so none of the 5 patterns can be checked in their canonical form.

However, the test rows themselves have the 5-pattern audit applied to them:

| Pattern | Verdict |
|---|---|
| from-x-import-y patch propagation bugs | N/A — no test files |
| test-bug-in-itself (asserts against wrong fixture) | **VIOLATED** — 3 rows assert wrong expected values; see per-test verdict below |
| source-incomplete-relative-to-test | PASS — the source change exists and is structurally sound |
| missing test isolation fixture | N/A — no test fixtures |
| AsyncMock on sync function (or vice versa) | N/A — no mocks |

## Per-test verdict

I re-derived every row independently this turn. **3 of 22 rows have wrong expected values** — a "test-bug-in-itself" pattern at the verifier level (the row, not the test file, asserts the wrong fixture). The cycle is still ALL_KEEP at the source level (the patch is correct), but the verifier's own rows are inaccurate. Three rows:

| Row | Asserted (test) | Re-derived (verifier) | Verdict |
|---|---|---|---|
| 7 | `clearTextureFloat` count = 3 | **5 hits** (3 ReSTIR at :170-172 in helper + 2 ReBLUR at :1041-1042) | **RELAX** — the count was wrong because pre-patch count wasn't actually 3, it was 5 (3 ReSTIR + 2 ReBLUR). Patch preserved all 5. The structural claim ("patch doesn't drop clearTextureFloat calls") is still TRUE; only the count assertion was wrong. |
| 8 | `bReSTIRInitialized` count = 3 | **6 hits** (:1080 init-set, :1139 init-render-guard, :1142 init-fail-reset, :1567 render-guard, :1745 dump-conditional, :1954 declare) | **RELAX** — pre-patch was 6; patch preserved all 6. Wrong count assertion, correct structural claim. |
| 9 | `NvrhiDevice->createTexture` count = 24 | **21 hits** | **RELAX** — pre-patch was 21; patch preserved all 21. Wrong count assertion, correct structural claim. |

**Why RELAX not DELETE**: the three rows are wrong about the **count number** but correct about the **structural claim** (patch preserves counts and behaviors). The patch itself is correct. Re-running these rows with the right expected values would PASS. The fix is to update the row expected values, not to delete the rows.

The other 19 rows independently re-derived and PASS:

- Row 1: 1 hit `static void CreateReSTIRTextures` at line 90 ✓
- Row 2: 4 hits of `CreateReSTIRTextures` substring (1 def + 2 calls + 1 comment) ✓ — my row said "≥3", exact count is 4
- Row 3: init call at line 1068 with 14 handle args ✓
- Row 4: resize call at line 1314 with 14 handle args ✓
- Row 5: resize trigger unchanged ✓
- Row 6: `BindingCache.Clear` count = 3 ✓ (lines 1123, 1321, 1858)
- Row 10: GBufferDiffuseTexture = NvrhiDevice->createTexture → 1 hit inside resize branch (line 1232) ✓
- Row 11-12: helper signature with W/H params and 14 handle refs ✓
- Row 13: 1 `CmdList->open()` inside helper (clear cmdlist) ✓
- Row 14: 1 `executeCommandList` inside helper ✓
- Row 15: clearTextureFloat targets = Reservoir0History, Reservoir1History, RadianceHistory ✓
- Row 16: `nvrhi::Format::D32` for PrevDepth ✓
- Row 17: `nvrhi::Format::RGBA16_FLOAT` for PrevNormal ✓
- Row 18: `keepInitialState = true` for reservoirs ✓
- Row 19: `Reservoir0Texture = NvrhiDevice` → 1 hit at line 110 (inside helper) ✓
- Row 20: `Desc.debugName = "Reservoir0"` → 1 hit at line 109 (inside helper) ✓
- Row 21: helper call at line 1314 precedes `BindingCache.Clear()` at line 1321 ✓
- Row 22: helper does not touch `LastWidth`/`LastHeight` ✓

## What this cycle established

1. **The patch is structurally sound.** 22 file-only rows checked, 19 PASS, 3 RELAX (wrong count assertions, correct structural claims). The patch:
   - Extracts a free static helper `CreateReSTIRTextures` (101 lines incl. comments) — lines 77-177
   - Calls it from init (replacing the inline block at pre-patch :955-1021) — line 1068
   - Calls it from the resize branch (preceded by `BindingCache.Clear()`) — line 1314
   - Preserves all `clearTextureFloat`, `bReSTIRInitialized`, `BindingCache.Clear`, and `createTexture` calls at their pre-patch counts (with corrections noted above)
2. **No protected file touched.** `AGENTS.md`, `CLAUDE.md`, `.cursorrules` byte-unchanged.
3. **Card L's premise** (14 ReSTIR textures created at init, dispatched over at `CurrentFBInfo`, never recreated on resize) is **structurally repaired** by the patch. A window-resize event now triggers `CreateReSTIRTextures` to recreate all 14 textures at the new extent, eliminating the unguarded OOB UAV store.
4. **The build verification is operator-side.** `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestCornellBoxGI` cannot be run from this runspace (`terminal` denied by tirith per `tools/approval.py:2999-3012`).

## Standing rule recorded by this cycle

**When writing file-only grep-count test rows, ALWAYS re-derive the pre-patch count by searching the source.** Do not assume counts from memory. The 3 RELAX rows above are recoverable but cost the cycle a SOME_RELAX verdict where ALL_KEEP was reachable. The pre-derivation step takes 1-2 file reads; the wrong-count cycle costs a re-audit.

## Verdict

**SOME_RELAX.** 19/22 rows PASS, 3/22 RELAX (wrong count assertions, correct structural claims). The patch itself is correct and ready for operator-side build verification. No MAJOR_DELETE — the source change is compile-coherent and the resize path now mirrors the v198 sibling positive control (`TestRTReflections.cpp:892-984`).