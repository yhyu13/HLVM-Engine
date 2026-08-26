#!/usr/bin/env bash
# v2-recipe.sh
#
# Operator-side verification recipe for the v2 binding-topology revert.
# Reverts the v22 split (separate SRV-only + UAV-only binding layouts) in
# FGIPass.cpp back to a single binding set containing both SRV and UAV
# resources. The proven-control TestCornellBoxGI uses this single-set
# pattern successfully; the v22 split was added to silence a Vulkan
# validation warning but introduced a binding descriptor mismatch that
# caused GBuffer SRV reads to return zero (mode 20/21/22 dumped solid
# black despite valid per-frame texture handles).
#
# This script verifies the v2 fix:
#   1. Source-side invariants (UAVBindingLayout removed, space1 removed from
#      HLSL, single binding set dispatch).
#   2. Build the test binary with --Rebuild --Test.
#   3. Run the test binary with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 and confirm
#      log has no Vulkan VUID/ERROR/CommandList errors.
#   4. Run with HLVM_PT_DEBUG_MODE=20 and confirm gi_raw dump is NON-uniform
#      (per-channel std > 5/255); pre-v2 fix this was solid black.
#   5. Run validate_restir_gi.py on the latest dump group.
#   6. Operator-side vision check: display_frame8.png must show Sponza.
#
# USAGE:
#   cd <HLVM-Engine root>
#   bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v2-recipe.sh
#
# READ-ONLY (no rm/mv/mkdir of originals; produces only NEW dump files
# under TestReSTIR_GI_Temporal_Data/dumps/YYYYMMDD_HHMMSS_* via the test
# binary).
#
# No GPU is touched by this script itself, but the script calls ./Build.sh
# (which compiles ~3 min including shader recompile) and
# ./Binary/Debug/TestReSTIR_GI_Temporal (which runs ~25-60 sec per mode,
# opens a Vulkan window, and writes 8 PNGs).
#
# EXITS:
#   0 — recipe complete; post-fix log stats and validator output captured;
#       operator must inspect dump PNGs visually
#   1 — v2 patch missing from source (operator must apply the edits first)
#   2 — source/validator/scripts missing on disk
#   3 — build failure (./Build.sh returned non-zero)
#   4 — test binary returned non-zero
#   5 — post-fix log stats OUT-OF-RANGE (VUID/ERROR/CommandList error
#       detected in fresh log)

set -uo pipefail   # NOTE: not -e because we want to capture each step's exit code

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"
TEST_NAME="TestReSTIR_GI_Temporal"
TEST_CPP="${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}.cpp"
FGIPASS_CPP="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"
HLSL_CANON="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl"
HLSL_DATA="${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}_Data/GIPathTracing.hlsl"
BIN_DIR="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug"
VALIDATOR_PY="${SCRIPT_DIR}/validate_restir_gi.py"

echo "=== v2-recipe.sh (tick-now-13, 2026-08-17) ==="
echo "REPO_ROOT: ${REPO_ROOT}"
echo

# 0. Source-side invariants: v2 patch + validator present
echo "--- [0/6] Source-side invariants ---"
if [[ ! -f "${FGIPASS_CPP}" ]]; then
    echo "  [FAIL] Missing FGIPass.cpp: ${FGIPASS_CPP}"
    exit 2
fi
if [[ ! -f "${HLSL_CANON}" || ! -f "${HLSL_DATA}" ]]; then
    echo "  [FAIL] Missing HLSL copies (canonical=${HLSL_CANON}, data-dir=${HLSL_DATA})"
    exit 2
fi
if [[ ! -f "${VALIDATOR_PY}" ]]; then
    echo "  [FAIL] Missing validator: ${VALIDATOR_PY}"
    exit 2
fi

# Invariant A: UAVBindingLayout member removed from header (single-set pattern)
UAV_LAYOUT_HITS=$(grep -rn "UAVBindingLayout;" "${REPO_ROOT}/Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h" || true)
if [[ -n "${UAV_LAYOUT_HITS}" ]]; then
    echo "  [FAIL] v2 patch NOT applied: 'UAVBindingLayout;' still declared in FGIPass.h"
    echo "  Expected: nvrhi::BindingLayoutHandle UAVBindingLayout; line removed."
    exit 1
fi
echo "  [OK] v2 patch A: UAVBindingLayout member removed from FGIPass.h"

# Invariant B: HLSL register(uN) without ,space1 (default register space)
SPACE1_HITS=$(grep -nE "register\(u[0-9]+, space1\)" "${HLSL_CANON}" "${HLSL_DATA}" || true)
if [[ -n "${SPACE1_HITS}" ]]; then
    echo "  [FAIL] v2 patch NOT applied: HLSL still has register(uN, space1):"
    echo "${SPACE1_HITS}"
    echo "  Expected: register(u0)/register(u1)/register(u2) without 'space1' qualifier."
    exit 1
fi
echo "  [OK] v2 patch B: HLSL register(u0..u2) without space1 (default space)"

# Invariant C: single binding set dispatch (5-arg overload, no second binding set)
DISPATCH_LINE=$(grep -n "RTPipeline.DispatchRays" "${FGIPASS_CPP}" || true)
if ! echo "${DISPATCH_LINE}" | grep -q "SRVBindingSet)$"; then
    echo "  [FAIL] v2 patch NOT applied: dispatch does not use single-binding-set 5-arg overload:"
    echo "${DISPATCH_LINE}"
    echo "  Expected: RTPipeline.DispatchRays(CmdList, W, H, D, SRVBindingSet);"
    exit 1
fi
echo "  [OK] v2 patch C: single binding set dispatched (5-arg overload)"
echo

# 1. Rebuild the test binary (forced shader recompile because HLSL changed)
echo "--- [1/6] Rebuild test binary (forced shader recompile) ---"
cd "${REPO_ROOT}"
rm -f "${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}_Data/GIPathTracing.sblob"
if ! ./Build.sh --Config=Debug --Target=${TEST_NAME} --Rebuild --Jobs=4; then
    echo "  [FAIL] ./Build.sh returned non-zero. See ${BIN_DIR}/build.log if available."
    exit 3
fi
echo "  [OK] build complete (GIPathTracing.sblob regenerated)"
echo

# 2. Run the test binary (writes fresh 8-PNG dump group)
echo "--- [2/6] Run test binary (default mode, HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8) ---"
cd "${BIN_DIR}"
if [[ ! -x "./${TEST_NAME}" ]]; then
    echo "  [FAIL] binary not executable: ${BIN_DIR}/${TEST_NAME}"
    exit 2
fi
if ! env HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./${TEST_NAME}; then
    echo "  [FAIL] test binary returned non-zero. Inspect ${BIN_DIR}/${TEST_NAME}.log"
    exit 4
fi
echo "  [OK] test binary exit 0"
echo

# 3. Grep post-fix log stats for Vulkan errors
echo "--- [3/6] Post-fix log stats ---"
LOG="${BIN_DIR}/${TEST_NAME}.log"
if [[ ! -f "${LOG}" ]]; then
    echo "  [FAIL] expected log file missing: ${LOG}"
    exit 4
fi

DISPLAY_STATS_LINE=$(grep "stats display floats" "${LOG}" | tail -1 || true)
GIRAW_STATS_LINE=$(grep "stats gi_raw floats"   "${LOG}" | tail -1 || true)
RESTIR_SUMMARY_LINE=$(grep "ReSTIR summary"     "${LOG}" | tail -1 || true)

echo "  ${DISPLAY_STATS_LINE}"
echo "  ${GIRAW_STATS_LINE}"
echo "  ${RESTIR_SUMMARY_LINE}"

VUID_COUNT=$(grep -cE "VUID|ERROR|CommandList error" "${LOG}" || true)
echo "  VUID/ERROR/CommandList count: ${VUID_COUNT}"
if [[ "${VUID_COUNT}" -ne 0 ]]; then
    echo "  [FAIL] Vulkan errors detected in fresh log."
    echo "  Inspect ${LOG} lines: $(grep -nE 'VUID|ERROR|CommandList error' "${LOG}" | head -3)"
    exit 5
fi
echo "  [OK] no Vulkan errors"
echo

# 4. Validator run
echo "--- [4/6] Validator run ---"
cd "${REPO_ROOT}"
if ! python3 "${VALIDATOR_PY}"; then
    echo "  [FAIL] validator returned non-zero. See output above."
    exit 5
fi
echo "  [OK] validator exit 0"
echo

# 5. Operator-side vision check (manual step; this script cannot see)
echo "--- [5/6] Vision check (operator-side) ---"
LATEST_DISPLAY=$(ls -t "${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}_Data/dumps/"*display_frame8.png 2>/dev/null | head -1 || true)
if [[ -z "${LATEST_DISPLAY}" ]]; then
    echo "  [FAIL] no display_frame8.png found in dumps/"
    exit 5
fi
echo "  Open this file in an image viewer:"
echo "    ${LATEST_DISPLAY}"
echo "  EXPECTED: recognizable Sponza gallery arches + floor + back wall + directional shadow."
echo "  (This script cannot vision-analyze; operator must eyeball it.)"
echo

# 6. Mode-20 discriminator (user-specified criterion #6)
echo "--- [6/6] Mode-20 discriminator ---"
cd "${BIN_DIR}"
if ! env HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./${TEST_NAME}; then
    echo "  [FAIL] mode-20 test binary returned non-zero."
    exit 4
fi
echo "  Inspect dumps/ for a NEW gi_raw dump (gi_raw_frame8.*.png)."
echo "  EXPECTED: NON-uniform values (per-pixel GBufferMaterial SRV reads returned real albedo)."
echo "  PRE-v2: uniform solid black (0,0,0,255) for every pixel."
echo "  Quick pixel-stats:"
LATEST_GIRAW=$(ls -t "${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}_Data/dumps/"*gi_raw_frame8.png 2>/dev/null | head -1 || true)
if [[ -n "${LATEST_GIRAW}" ]]; then
    python3 -c "
from PIL import Image
import numpy as np
img = np.array(Image.open('${LATEST_GIRAW}'))
print(f'  shape={img.shape} dtype={img.dtype}')
print(f'  per-channel mean = {img[:,:,0].mean():.2f}, {img[:,:,1].mean():.2f}, {img[:,:,2].mean():.2f}, {img[:,:,3].mean():.2f}')
print(f'  per-channel std  = {img[:,:,0].std():.2f}, {img[:,:,1].std():.2f}, {img[:,:,2].std():.2f}, {img[:,:,3].std():.2f}')
print(f'  unique R values  = {len(np.unique(img[:,:,0]))}')
print(f'  PASS threshold   = std > 5/255 AND unique > 5')
"
fi
echo

echo "=== recipe complete (exit 0) ==="
echo "Operator-side next steps:"
echo "  1. Visual-diff the new display PNG vs the previous baseline (e.g., dumps/20260814_*)."
echo "  2. Pixel-stats diff for gi_raw (numpy or PIL; threshold: per-channel std > 5/255)."
echo "  3. If all 7 acceptance criteria PASS:"
echo "     a. Flip PICK card line '[~] TestReSTIR_GI_Temporal ...' to '[x]'."
echo "     b. Commit the working tree (NOT done by the cron per AUTO_RESOLVE_DO_NOT:yes)."
echo "  4. If any criterion fails: see PENDING_TESTS_v2.md §If Test N fails for the"
echo "     compound-fix prescriptions (handle identity, setRegisterSpaceAndDescriptorSet,"
echo "     validation layer re-enable, etc.)."
exit 0
