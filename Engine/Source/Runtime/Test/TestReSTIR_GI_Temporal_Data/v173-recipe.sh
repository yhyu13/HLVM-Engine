#!/usr/bin/env bash
# v173-recipe.sh
#
# Operator-side verification recipe for the v173 MaxM=1.0f display-monochrome
# fix. Bundles the 6-step recipe in PENDING_TESTS_v173.md §Concrete bisect plan
# (Steps 1-6, excluding the optional Step 8 regression-test of the validator)
# into a single bash script.
#
# USAGE:
#   cd <HLVM-Engine root>
#   bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v173-recipe.sh
#
# READ-ONLY (no rm/mv/mkdir of originals; produces only NEW dump files
# under TestReSTIR_GI_Temporal_Data/dumps/YYYYMMDD_HHMMSS_* via the test
# binary).
#
# No GPU is touched by this script itself, but the script calls ./Build.sh
# (which compiles ~3 min) and ./Binary/Debug/TestReSTIR_GI_Temporal (which
# runs ~25 sec, opens a Vulkan window for ~21.83 sec, and writes 8 PNGs).
#
# EXITS:
#   0 — recipe complete; post-fix log stats and validator output captured;
#       operator must inspect dump PNGs visually
#   1 — patch missing from source (operator must apply the 2-line edit first)
#   2 — source/validator/scripts missing on disk
#   3 — build failure (./Build.sh returned non-zero)
#   4 — test binary returned non-zero
#   5 — post-fix log stats OUT-OF-RANGE (one or more acceptance criteria FAIL
#       in the new log)

set -uo pipefail   # NOTE: not -e because we want to capture each step's exit code

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"
TEST_NAME="TestReSTIR_GI_Temporal"
TEST_CPP="${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}.cpp"
BIN_DIR="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug"
VALIDATOR_PY="${SCRIPT_DIR}/validate_restir_gi.py"

echo "=== v173-recipe.sh (tick1636, 2026-08-15) ==="
echo "REPO_ROOT: ${REPO_ROOT}"
echo "DATE (UTC): $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
echo

# 0. Source-side invariants: patch + validator present
echo "--- [0/6] Source-side invariants ---"
if [[ ! -f "${TEST_CPP}" ]]; then
    echo "  [FAIL] Missing test C++: ${TEST_CPP}"
    exit 2
fi
TC_LINE=$(grep -n "TC\.MaxM" "${TEST_CPP}" | head -1 | cut -d: -f1)
SC_LINE=$(grep -n "SC\.MaxM" "${TEST_CPP}" | head -1 | cut -d: -f1)
TC_VAL=$(grep -n "TC\.MaxM" "${TEST_CPP}" | head -1 | sed 's/.*= //;s/;.*//')
SC_VAL=$(grep -n "SC\.MaxM" "${TEST_CPP}" | head -1 | sed 's/.*= //;s/;.*//')
echo "  TC.MaxM = ${TC_VAL} (line ${TC_LINE})"
echo "  SC.MaxM = ${SC_VAL} (line ${SC_LINE})"
if [[ "${TC_VAL}" != "1.0f" || "${SC_VAL}" != "1.0f" ]]; then
    echo "  [FAIL] v173 patch NOT applied (expected 'TC.MaxM = 1.0f' AND 'SC.MaxM = 1.0f')."
    echo "  Run the manual edit below then re-invoke this script."
    cat <<'EDIT'
    # In ${TEST_CPP}:
    #   Line ${TC_LINE}: change 'TC.MaxM = 30.0f' to 'TC.MaxM = 1.0f;     // v173: small M -> W~1'
    #   Line ${SC_LINE}: change 'SC.MaxM = 30.0f' to 'SC.MaxM = 1.0f;     // v173: matching cap downstream'
EDIT
    exit 1
fi
echo "  [OK] v173 patch INTACT"

if [[ ! -f "${VALIDATOR_PY}" ]]; then
    echo "  [FAIL] Missing validator: ${VALIDATOR_PY}"
    exit 2
fi
VALIDATOR_LINES=$(wc -l < "${VALIDATOR_PY}")
echo "  [OK] validator INTACT (${VALIDATOR_LINES} lines)"
echo

# 1. Rebuild the test binary
echo "--- [1/6] Rebuild test binary ---"
cd "${REPO_ROOT}"
if ! ./Build.sh --Config=Debug --Target=${TEST_NAME} --Rebuild --Jobs=4; then
    echo "  [FAIL] ./Build.sh returned non-zero. See ${BIN_DIR}/build.log if available."
    exit 3
fi
echo "  [OK] build complete"
echo

# 2. Run the test binary (writes fresh 8-PNG dump group)
echo "--- [2/6] Run test binary ---"
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

# 3. Grep post-fix log stats
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
echo "  Compare: pre-v173 fix this was a uniform sentinel value."
echo
echo "Open the latest gi_raw dump:"
LATEST_GIRAW=$(ls -t "${REPO_ROOT}/Engine/Source/Runtime/Test/${TEST_NAME}_Data/dumps/"*gi_raw_frame8.png 2>/dev/null | head -1 || true)
if [[ -n "${LATEST_GIRAW}" ]]; then
    echo "  ${LATEST_GIRAW}"
fi
echo

echo "=== recipe complete (exit 0) ==="
echo "Operator-side next steps:"
echo "  1. Visual-diff the new display PNG vs the previous baseline."
echo "  2. Pixel-stats diff for gi_raw (numpy or PIL)."
echo "  3. If all 7 acceptance criteria PASS: flip PICK card line 118 to [x] per"
echo "     the AUTO_RESOLVE_DO_NOT:yes contract (operator is the only authority)."
echo "  4. If any criterion fails: see tick1636.md §If the recipe FAILS on one"
echo "     criterion table for the compound-fix prescriptions."
exit 0
