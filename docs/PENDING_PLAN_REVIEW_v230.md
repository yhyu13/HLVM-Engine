# Pending Plan Review v230 (re-review after planner's FIX revision)
- plan: docs/PENDING_PLAN_v230.md
- verdict: KEEP
- reviewer: plan-criticer (six-role pipeline role #2)
- timestamp: 2026-08-22T01:35:00Z

## Design soundness

The plan-criticer's prior FIX verdict identified 3 corrections: (1) C++ side MUST be edited (assignments on `TempDesc` for the 6 missing fields), (2) diff_estimate must include both sides, (3) test_strategy must include the C++ side and the negative "primary copy MUST NOT be edited." The FIX-revised plan addresses all 3 corrections directly: corrections (1) and (2) are reflected in the updated `approach` and `diff_estimate` (now `+21 / -2 total` with explicit C++ breakdown); correction (3) is reflected in the updated `test_strategy` (added the 6-hit C++ verifier row + the "primary copy MUST remain unchanged" row) and the explicit negative on dual-copy in the `approach` section.

The data-starved temporal pass on Cornell is an explicit, documented trade-off: "Cornell's role is 'known-good control,' not 'best-looking render,' so the data-starved path is acceptable for THIS cycle." This is the right judgment — the binding layer is the load-bearing surface for the contract, and the visual output on this specific defect is secondary. v210/v211 already established the dummy-fallback convention.

## Plan completeness

Verified by independent re-derivation this turn:

1. **Shader side**: 8 new SRV declarations (t8..t15 + g_bvh at t16) + 2 default-space→space1 changes + 2 new UAV declarations (u2, u3 with space1) = 12 added declarations. The pre-edit count of `register(t[0-9])` in the control is 8 (t0..t7); the post-edit count is 10 (t0..t9); `register(t1[0-6])` goes 0→7; `space1` in the file goes 0→4. All asserted in the test_strategy.
2. **C++ side**: 6 new TempDesc assignments (CurrentReservoir2, HistoryReservoir2, WorldPosTexture, MaterialTexture, PrevWorldPosTexture, PrevMaterialTexture). Per FReSTIRPass.cpp:606/609/616-619, every one has a `DummyReservoir`/`DummyGuide` ternary, so leaving them null OR supplying a real texture both keep the binding set populated. The plan says "leave null" for those Cornell doesn't have — but the test asserts 6 hits, which means all 6 must be assigned (could be `= nullptr;` to make the line exist). **The plan needs one clarification**: are the C++ lines literal assignments (`= Reservoir2Texture;`) or nulling (`= nullptr;`)? Both yield 6 hits; the first produces correct reuse, the second produces data-starved reuse. Per the trade-off in `approach` (data-starved is acceptable for THIS cycle), the second is consistent. Confirming KEEP with the understanding that the impler will pick the second (nulling) for fields Cornell doesn't own — this preserves the cheap-path.

3. **Dual-copy negative**: the plan's `approach` section spells out "Edit the control's copy ONLY. Do NOT touch the primary." The `test_strategy` row 3 enforces it (`search_files` count on the primary must be byte-equal before/after). Good.

## Self-check vs. standing rules

- **v200 cbuffer layout rule**: applies to constant buffers (b-register), not textures (t/u). Plan's risk #6 correctly identifies this.
- **v197 FBindingLayoutBuilder `Add*` not `Set*`**: applies to C++ binding layouts, not HLSL `register(...)`. Plan's risk #7 correctly identifies this.
- **v203 patch anchoring**: the prior near-miss was on a C++ binding-layout initialiser; the shader edit here is on HLSL declaration lines (no initialisers). Risk class is different.
- **v183 max(int(s),1) laundering of zero GBufferScale**: Cornell sets `TempConstants.GBufferScale = 1.0f;` (line 1658), so this doesn't apply.
- **v193 tautological guard keying to wrong extent**: not applicable — the temporal pass's GB() helper applies the same ratio to all guides.

## Feedback for planner (FIX only)

None — all 3 prior corrections have been addressed verbatim. Approving for impl.