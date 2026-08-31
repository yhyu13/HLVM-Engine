# Pending Impl Review v139 — nvrhi validation layer hookup re-applied

- plan: docs/PENDING_PLAN_v139.md
- commit: docs/PENDING_COMMIT_v139.md
- verdict: KEEP
- reviewer: reviewer (file-only single-profile mode, tick 263)
- timestamp: 2026-07-31

## plan_fidelity_check

The v139 impl matches the v139 plan exactly. NO deviations declared. The impler:

1. **Include addition (lines 7-15)**: Added `#include <nvrhi/validation.h>` at the top of `DeviceManagerVk4_LifeCycle.cpp` after `#include "DeviceManagerVk.h"`, with a 9-line comment block explaining the rationale. **Verified at line 15** — the include is present, the comment is present, the position is correct (between `DeviceManagerVk.h` and `#if HLVM_VULKAN_RENDERER`).

2. **Stub → call edit (line 118)**: Replaced `m_ValidationLayer = nullptr;` with `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`. **Verified at line 118** — the call is present, the gate `if (DeviceParams.bEnableNVRHIValidationLayer)` is preserved at lines 82-119, the closing brace at line 119 is preserved.

3. **Comment block update (lines 84-117)**: Existing v132+v136 comments preserved; 12 new v139 comment lines added (lines 106-117) explaining the v134 prerequisite satisfaction and the rationale for re-applying the v132 patch.

4. **Destructor left untouched (line 198)**: `m_ValidationLayer = nullptr;` in destructor is preserved. **Verified at line 198** — the destructor still nulls the handle on shutdown, which is the correct behavior per v132 review §81-86.

All four edits match the plan's specified approach exactly. No unjustified deviations.

## TDD evidence

- [ ] Test file present: N/A — v139 is a diagnostic/behavioral change, not a test-producing cycle. The "test" is the parent-runspace recipe (rebuild + run + VUID inspection).
- [ ] Test commit precedes impl: N/A
- [ ] Red-phase commit message: N/A
- [x] Testability hooks: The validation layer being enabled is itself a testability mechanism — it surfaces the actual VUID that the next round (v140+) will use to identify the bisect root cause. The v139 patch is a one-step forward on the testability chain.

## Security scan

- [x] No hardcoded secrets: The patch contains no API keys, passwords, tokens, or credentials.
- [x] No shell injection (os.system, shell=True): No new shell calls added.
- [x] No eval/exec: No eval/exec added.
- [x] No SQL injection: N/A — no SQL queries.

## Self-review checklist

- [x] Validation: The call is gated by `if (DeviceParams.bEnableNVRHIValidationLayer)` which ORs in the CVar default `g_UseValidationLayers = true`. So validation layer is enabled by default in the test (this is the intended behavior change — v139 enables the validation layer, doesn't add new validation logic).
- [x] Error handling: `nvrhi::validation::createValidationLayer` returns a `DeviceHandle` (Object wrapper around `DeviceWrapper*`). If the call fails (returns null handle), the existing code at `DeviceManagerVk5_Misc.cpp:34-38` already handles this gracefully (`if (m_ValidationLayer) return m_ValidationLayer; return m_NvrhiDevice;`). No new error path introduced.
- [x] Tests: Per the v24 diagnostic and v132 review, the "test" for this patch is the parent-runspace recipe (rebuild + run + VUID inspection). No unit tests are appropriate here.
- [x] Style: The patch matches the project's existing patterns (Allman braces, 4-space indent, v* commit comments in surrounding code, v* tick number in the comment header).

## File-only state verification (re-confirmed this tick)

All 10 patches intact after v139:

| Patch | File:Line | Status | Verified at tick |
|-------|-----------|--------|------------------|
| v131 cases 20/21/22/30/31u discriminator + commitBarriers defense | `GIPathTracing.hlsl:486-491`, `FGIPass.cpp:692` | INTACT | 263 |
| v132 (createValidationLayer hookup) | `DeviceManagerVk4_LifeCycle.cpp` | REVERTED by v136 | 263 |
| v133 (cmake FORCE NVRHI_WITH_VALIDATION=ON) | `Engine/Source/Runtime/CMakeLists.txt:182` | INTACT | 263 |
| v134 (validation TUs in add_library) | `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:213-214` | INTACT | 263 |
| v135 (commitBarriers BEFORE createBindingSet) | `FGIPass.cpp:579` | INTACT | 263 |
| v136 (v132 revert) | `DeviceManagerVk4_LifeCycle.cpp:198` | APPLIED | 263 |
| v137 (UAV binding-offset fix) | `FGIPass.cpp:301-318` | INTACT | 263 |
| v138 (mode 6 bypassEarlyReturn addition) | `GIPathTracing.hlsl:486` | APPLIED | 263 |
| **v139 (re-apply createValidationLayer hookup + include)** | `DeviceManagerVk4_LifeCycle.cpp:7-15, 106-118` | **APPLIED** | 263 (this tick) |

No accidental reverts. No merge conflicts.

## Reasoning

The v139 impl is structurally correct:
- The stub → call edit is at the correct site (gated by `bEnableNVRHIValidationLayer`, inside the createDevice function).
- The include addition addresses the v132 plan's omission (verified via search_files that `<nvrhi/validation.h>` is NOT in the transitive include chain via `<nvrhi/nvrhi.h>`).
- The destructor is correctly left untouched.
- The comment block accurately describes the v134 prerequisite satisfaction.

The patch is the file-only portion of the v24 diagnostic's 4-step recipe (steps 2 + 3). Step 4 (rebuild + run + VUID inspection) is the parent runspace's responsibility per EC-039 (terminal blocked in this runspace).

## Feedback for impler (FIX only)

None. The patch is clean, follows the plan, and addresses the structural gap correctly.

## Verdict

**KEEP.** The v139 patch is sound. The dispatcher should route to Agent #5 (tester) on the next tick.

---

**Per `six-role-pipeline §Role #4 (reviewer)`, this is a file-only verdict based on the diff content + read_file verification of the cited file:line references. Behavioral verification (build + run + VUID inspection) requires terminal+vision — deferred to parent runspace per EC-039.**