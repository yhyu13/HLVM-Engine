# Pending Plan Review v137
- plan: docs/PENDING_PLAN_v137.md
- verdict: KEEP
- reviewer: plan-criticer (file-only single-profile mode)
- timestamp: 2026-07-31

## Design soundness

The plan correctly identifies the descriptor-slot double-add bug at `FGIPass.cpp:301-310`. The bug trace is mechanically complete:

1. `nvrhi::BindingLayoutDesc::bindingOffsets` defaults to `{shaderResource=0, sampler=128, constantBuffer=256, unorderedAccess=384}` (nvrhi.h:1971-1974).
2. FGIPass's manually-built UAV layout at FGIPass.cpp:301-310 does NOT call `setBindingOffsets()`.
3. UAV items at `slot = URegShift + 0 = 384` and `URegShift + 1 = 385` (URegShift = 384 per FBindingLayoutBuilder.h:37).
4. nvrhi's `BindingLayout` ctor (vulkan-resource-bindings.cpp:100) computes `bindingLocation = registerOffset + binding.slot = 384 + 384 = 768` (and `384 + 385 = 769`).
5. The shader's `register(u0, space1)` and `register(u1, space1)` (GIPathTracing.hlsl:88, 91) compile to SPIR-V `Binding=384` and `Binding=385` (default `--uRegShift=384`).
6. **Mismatch: descriptor write at slot 768, shader reads at slot 384.** Writes are silently dropped.

The diagnostic chain is corroborated by `docs/DIAGNOSTIC_2026-07-30-v24.md` mode-6 finding: mode 6 writes a per-pixel gradient with NO SRV reads. If the dispatch body runs, the gradient should be visible. It is not. This proves the OutputTexture UAV write is broken at the descriptor level, exactly the bug this plan fixes.

The fix matches the FReSTIRPass precedent (FReSTIRPass.cpp:161-162, 186-188, 207-208) which explicitly calls `setBindingOffsets(0,0,0,0)` on all three of its layouts. Same pattern, same fix.

## Plan completeness

The plan correctly notes this fix may NOT close the bisect alone (mode 20 may have a separate root cause). The honest scoping is appreciated — Risk #2 says "after v137 lands, parent runspace must rebuild + run + check mode 20 result."

The plan also correctly notes the SRV layout is structurally correct (FBindingLayoutBuilder's constructor sets `bindingOffsets = {0,0,0,0}`, so the SRV layout's `bindingOffsets.unorderedAccess = 0`, not the default 384). Risk #5 makes this explicit.

One minor completeness issue: the plan should specify the EXACT form of the `setBindingOffsets` call. Two options exist:
- `setBindingOffsets(0,0,0,0)` (4-arg overload at FBindingLayoutBuilder.cpp:36-49)
- `setBindingOffsets(offsets)` with a `VulkanBindingOffsets` struct (at FBindingLayoutBuilder.cpp:30-34 / vulkan-resource-bindings callsite pattern)

The 4-arg overload goes through `FBindingLayoutBuilder`'s internal state — that's NOT the right API here because we're using a raw `nvrhi::BindingLayoutDesc` not the builder. We should use `UAVLayoutDesc.setBindingOffsets(VulkanBindingOffsets(...))` directly. The plan doesn't explicitly say which form to use. **This is a minor clarity issue, not a fix-blocker — the impler will use the correct form because the precedent (FReSTIRPass.cpp:161-163, 186-188) uses the struct form.**

## Feedback for planner (FIX only)

None — plan is acceptable as-is. Proceed to impler.

---

**Per `six-role-pipeline §Role #2 (plan-criticer)`, this is a file-only verdict based on the plan content + read_file verification of the cited file:line references (FGIPass.cpp:301-310, FReSTIRPass.cpp:158-198, FBindingLayoutBuilder.cpp:14-22, vulkan-resource-bindings.cpp:48-110, nvrhi.h:1969-2015, GIPathTracing.hlsl:88-91).**