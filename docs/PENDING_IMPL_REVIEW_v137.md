# Pending Impl Review v137
- plan: docs/PENDING_PLAN_v137.md
- commit: docs/PENDING_COMMIT_v137.md
- verdict: KEEP
- reviewer: reviewer (file-only single-profile mode)
- timestamp: 2026-07-31

## plan_fidelity_check

Impler followed the plan. The change at `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:301-318` correctly added the `nvrhi::VulkanBindingOffsets UAVOffsets` struct (with all four offsets set to 0) and called `UAVLayoutDesc.setBindingOffsets(UAVOffsets)` BEFORE the items are added. The deviation from the plan (struct form vs 4-arg overload) is correctly justified in PENDING_COMMIT_v137.md and matches the FReSTIRPass precedent (FReSTIRPass.cpp:161-163, 186-188, 207-208).

The fix:
- `bindingOffsets.constantBuffer = 0` (default was 256 — no change in effect because no CBV items)
- `bindingOffsets.shaderResource = 0` (default was 0 — no change in effect because no SRV items)
- `bindingOffsets.sampler = 0` (default was 128 — no change in effect because no sampler items)
- **`bindingOffsets.unorderedAccess = 0`** (default was 384 — this is the fix; eliminates the double-add with `URegShift + N = 384 + N`)

After the fix, nvrhi's `BindingLayout` ctor (vulkan-resource-bindings.cpp:100) computes:
- Item 0 (slot=384): `bindingLocation = 0 + 384 = 384` ✓ matches shader's `Binding=384`
- Item 1 (slot=385): `bindingLocation = 0 + 385 = 385` ✓ matches shader's `Binding=385`

## TDD evidence

- [ ] Test file present: n/a (this is a binding-bug patch, not a behavioral change)
- [ ] Test commit precedes impl: n/a
- [ ] Red-phase commit message: n/a

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (os.system, shell=True)
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] Validation: the change is a 5-line addition + 1 call; validation is "the next rebuild succeeds + mode 20 returns non-zero per-pixel" — terminal+vision-required, deferred to parent
- [x] Error handling: no change to error handling; existing per-frame `[v23-diag]` log lines will surface any descriptor set creation failure (which won't happen because the binding offsets fix is structural)
- [x] Tests: no test changes; v131+v135+v136 patches have file-only test markers (HLVM_PT_DEBUG_MODE=20/21/22)

## File-only state verification

All 5 patches still intact after v137:

| Patch | File:Line | Status |
|---|---|---|
| v131 (commitBarriers defense-in-depth) | `FGIPass.cpp:675` | INTACT |
| v131 (cases 20/21/22/30/31u discriminator) | `GIPathTracing.hlsl:685-687, 712-714` | INTACT |
| v132 (createValidationLayer hookup) | `DeviceManagerVk4_LifeCycle.cpp:88` | **REVERTED by v136** |
| v133 (cmake FORCE NVRHI_WITH_VALIDATION=ON) | `Engine/Source/Runtime/CMakeLists.txt:182` | INTACT |
| v134 (validation TUs in add_library) | `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:209-214, 233-236` | INTACT |
| v135 (commitBarriers BEFORE createBindingSet) | `FGIPass.cpp:557-562` | INTACT |
| v136 (v132 revert) | `DeviceManagerVk4_LifeCycle.cpp:88, 163` | APPLIED |
| **v137 (UAV binding-offset fix)** | `FGIPass.cpp:301-318` | **APPLIED** |

No other references to `UAVLayoutDesc` exist in the codebase outside FGIPass.cpp (verified via search_files).

## Feedback for impler (FIX only)

None.

---

**Per `six-role-pipeline §Role #4 (reviewer)`, this is a file-only verdict based on the diff content + read_file verification of the cited file:line references. Behavioral verification requires terminal+vision — deferred to parent runspace.**