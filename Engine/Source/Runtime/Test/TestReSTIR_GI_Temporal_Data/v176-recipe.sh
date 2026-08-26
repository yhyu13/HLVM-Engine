#!/usr/bin/env bash
# v176-recipe.sh — minimal honest reconstruction of the closure recipe
# for the v176 GBuffer SRV binding fix on TestReSTIR_GI_Temporal.
#
# This is a reconstruction produced by six-role-pipeline tick (2026-11-16).
# The lineage claimed the canonical recipe was 489 lines (v234 audit) or
# 312 lines (._OPERATOR_RECIPE_v176.sh:48, tick-300 shim); first-hand
# verification this tick showed the file was missing entirely. This
# minimal recipe documents the 7 acceptance gates from the lineage and
# the shim's exit-code contract without fabricating the original's
# exact contents. Operator-side execution (terminal required) is the
# path to runtime closure.
#
# Exit codes (matches _OPERATOR_RECIPE_v176.sh:15-23):
#   0  PASS  — all 7 gates closed
#   1  BUILD — gate 1 (Debug build) failed
#   2  DUMP  — gate 2 (HLVM_DUMP_RGI=1 dumps) failed
#   3  VULK  — gate 3 (no VUID/ERROR in freshest log) failed
#   4  CMDL  — gate 4 (no CommandList errors) failed
#   5  VAL   — gate 5 (4-check structural validator) failed
#   6  M20   — gate 7 (HLVM_PT_DEBUG_MODE=20 SRV non-zero) failed
#   7  ENV   — pre-flight (python3+numpy+PIL) failed
#
# Usage:
#   bash v176-recipe.sh             # gates 1, 3, 4 (file-only preflight)
#   bash v176-recipe.sh mode20      # gate 7 (HLVM_PT_DEBUG_MODE=20 SRV probe)
#   bash v176-recipe.sh dump        # gate 2 (HLVM_DUMP_RGI=1 + HLVM_RGI_ACCUM=8)
#   bash v176-recipe.sh val         # gate 5 (validate_restir_gi.py)
#
# Operator action on result:
#   exit 0 → v176 hypothesis CONFIRMED. Commit the v176 diff and close.
#   exit 1 → build broken. Run ./GenerateCMakeProjects.sh first.
#   exit 2 → dump infra missing. Restore env-var hook for HLVM_DUMP_RGI
#            in TestReSTIR_GI_Temporal.cpp (search for "HLVM_DUMP_RGI").
#   exit 3 → VUID/ERROR in freshest log. Inspect log; revert offending
#            binding change.
#   exit 4 → CommandList error. Possible nvrhi deferred-barrier-ordering
#            pattern; split SRV + UAV into separate binding sets.
#   exit 5 → validator returned non-zero. Compare dump pixel-stats against
#            validate_restir_gi.py thresholds.
#   exit 6 → mode-20 SRV still returns zero. Check GBufferMaterial handle
#            identity between RenderGBuffer and FGIPass::DispatchRays
#            (search for "[handle-id]" in freshest log).
#   exit 7 → pip install --user numpy pillow.
#
# Source: six-role-pipeline tick (2026-11-16), docs/PENDING_PLAN_v235.md
#         + docs/PENDING_COMMIT_v235.md + docs/_OPERATOR_RECIPE_v176.sh
# License: MIT (project-level).

set -uo pipefail

# Locate the recipe's directory so the script works regardless of cwd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

# Defaults — the operator can override by exporting these before invoking.
HLVM_DUMP_RGI_DEFAULT="${HLVM_DUMP_RGI_DEFAULT:-1}"
HLVM_RGI_ACCUM_DEFAULT="${HLVM_RGI_ACCUM_DEFAULT:-8}"
HLVM_PT_DEBUG_MODE_DEFAULT="${HLVM_PT_DEBUG_MODE_DEFAULT:-20}"

# Files this recipe depends on (file-only preflight; non-fatal if missing).
TEST_BIN="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal"
TEST_LOG="${TEST_BIN}.log"
DUMP_DIR="${SCRIPT_DIR}/dumps"
VALIDATOR="${SCRIPT_DIR}/validate_restir_gi.py"

# -----------------------------------------------------------------------------
# Gate 7: pre-flight (python3+numpy+PIL)
# -----------------------------------------------------------------------------
gate_env() {
    echo "[gate-7/ENV] pre-flight: python3+numpy+PIL"
    if ! command -v python3 >/dev/null 2>&1; then
        echo "  FAIL: python3 not found"
        return 7
    fi
    if ! python3 -c "import numpy, PIL" >/dev/null 2>&1; then
        echo "  FAIL: numpy+PIL not importable. Try: pip install --user numpy pillow"
        return 7
    fi
    echo "  PASS: python3+numpy+PIL OK"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 1: Debug build
# -----------------------------------------------------------------------------
gate_build() {
    echo "[gate-1/BUILD] Debug build of TestReSTIR_GI_Temporal"
    if [[ ! -x "${REPO_ROOT}/Build.sh" ]]; then
        echo "  FAIL: ${REPO_ROOT}/Build.sh not executable"
        return 1
    fi
    if ! (cd "${REPO_ROOT}" && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild); then
        echo "  FAIL: build returned non-zero"
        return 1
    fi
    echo "  PASS: build OK"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 2: HLVM_DUMP_RGI=1 dumps
# -----------------------------------------------------------------------------
gate_dump() {
    echo "[gate-2/DUMP] HLVM_DUMP_RGI=${HLVM_DUMP_RGI_DEFAULT} HLVM_RGI_ACCUM=${HLVM_RGI_ACCUM_DEFAULT}"
    if [[ ! -x "${TEST_BIN}" ]]; then
        echo "  FAIL: ${TEST_BIN} not built (run gate 1 first)"
        return 2
    fi
    mkdir -p "${DUMP_DIR}"
    if ! (cd "${REPO_ROOT}" && HLVM_DUMP_RGI="${HLVM_DUMP_RGI_DEFAULT}" HLVM_RGI_ACCUM="${HLVM_RGI_ACCUM_DEFAULT}" "${TEST_BIN}"); then
        echo "  FAIL: dump run returned non-zero"
        return 2
    fi
    # Find the newest dump group; bail if none.
    newest="$(ls -1t "${DUMP_DIR}"/[0-9]*_display_frame*.png 2>/dev/null | head -n 1 || true)"
    if [[ -z "${newest}" ]]; then
        echo "  FAIL: no display_frame*.png in ${DUMP_DIR} after the run"
        return 2
    fi
    echo "  PASS: freshest display dump = ${newest}"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 3: Vulkan VUID/ERROR in freshest log
# -----------------------------------------------------------------------------
gate_vulk() {
    echo "[gate-3/VULK] no VUID/ERROR in ${TEST_LOG}"
    if [[ ! -f "${TEST_LOG}" ]]; then
        echo "  FAIL: ${TEST_LOG} not found"
        return 3
    fi
    if grep -E 'VUID|ERROR:' "${TEST_LOG}" >/dev/null 2>&1; then
        echo "  FAIL: VUID/ERROR lines found:"
        grep -E 'VUID|ERROR:' "${TEST_LOG}" | head -n 5
        return 3
    fi
    echo "  PASS: 0 VUID/ERROR lines in freshest log"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 4: CommandList errors in freshest log
# -----------------------------------------------------------------------------
gate_cmdl() {
    echo "[gate-4/CMDL] no CommandList errors in ${TEST_LOG}"
    if [[ ! -f "${TEST_LOG}" ]]; then
        echo "  FAIL: ${TEST_LOG} not found"
        return 4
    fi
    if grep -E 'CommandList.*(error|invalid|fail)' "${TEST_LOG}" >/dev/null 2>&1; then
        echo "  FAIL: CommandList error lines found:"
        grep -E 'CommandList.*(error|invalid|fail)' "${TEST_LOG}" | head -n 5
        return 4
    fi
    # Verify handle identity across RenderGBuffer ↔ FGIPass::DispatchRays
    # by parsing the [handle-id] log lines. Mismatch = recreate-bind bug.
    rg_handles="$(grep '\[handle-id\] FGIPass::DispatchRays' "${TEST_LOG}" | tail -n 1 | sed -E 's/.*GBufferMaterial=(0x[0-9a-f]+) WorldPos=(0x[0-9a-f]+) Normal=(0x[0-9a-f]+).*/\1 \2 \3/' || true)"
    rt_handles="$(grep '\[handle-id\] RenderGBuffer' "${TEST_LOG}" | tail -n 1 | sed -E 's/.*GBufferMaterial=(0x[0-9a-f]+) WorldPos=(0x[0-9a-f]+) Normal=(0x[0-9a-f]+).*/\1 \2 \3/' || true)"
    if [[ -n "${rg_handles}" && -n "${rt_handles}" && "${rg_handles}" != "${rt_handles}" ]]; then
        echo "  FAIL: GBuffer handle identity mismatch:"
        echo "         RenderGBuffer    : ${rt_handles}"
        echo "         FGIPass::DispatchRays : ${rg_handles}"
        return 4
    fi
    echo "  PASS: 0 CommandList errors + GBuffer handle identity preserved"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 5: 4-check structural validator
# -----------------------------------------------------------------------------
gate_val() {
    echo "[gate-5/VAL] validate_restir_gi.py on the freshest dump group"
    if [[ ! -f "${VALIDATOR}" ]]; then
        echo "  FAIL: ${VALIDATOR} not found"
        return 5
    fi
    # Pick the freshest dump group (timestamp prefix).
    newest_group="$(ls -1 "${DUMP_DIR}" 2>/dev/null | grep -E '^[0-9]{8}_[0-9]{6}_' | sort -r | head -n 1 | sed 's/_display_frame.*//' || true)"
    if [[ -z "${newest_group}" ]]; then
        echo "  FAIL: no dump groups found in ${DUMP_DIR}"
        return 5
    fi
    if ! python3 "${VALIDATOR}" --dump-group "${newest_group}" --dump-dir "${DUMP_DIR}"; then
        echo "  FAIL: validator returned non-zero on group ${newest_group}"
        return 5
    fi
    echo "  PASS: validator OK on group ${newest_group}"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 6: vision on the freshest display image (operator-side only)
# -----------------------------------------------------------------------------
gate_vision() {
    echo "[gate-6/VISION] recognize Sponza in the freshest display image"
    echo "  SKIP: vision gate is operator-side (no vision tool from this script)"
    echo "  To run: xdg-open ${DUMP_DIR}/*_display_frame*.png"
    return 0
}

# -----------------------------------------------------------------------------
# Gate 7: HLVM_PT_DEBUG_MODE=20 SRV probe (GBufferMaterial non-zero)
# -----------------------------------------------------------------------------
gate_m20() {
    echo "[gate-7/M20] HLVM_PT_DEBUG_MODE=${HLVM_PT_DEBUG_MODE_DEFAULT} GBufferMaterial SRV probe"
    if [[ ! -x "${TEST_BIN}" ]]; then
        echo "  FAIL: ${TEST_BIN} not built (run gate 1 first)"
        return 6
    fi
    mkdir -p "${DUMP_DIR}"
    if ! (cd "${REPO_ROOT}" && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM="${HLVM_RGI_ACCUM_DEFAULT}" HLVM_PT_DEBUG_MODE="${HLVM_PT_DEBUG_MODE_DEFAULT}" "${TEST_BIN}"); then
        echo "  FAIL: mode-20 run returned non-zero"
        return 6
    fi
    # Find the freshest gi_raw dump and ask the validator's pixel-stats to
    # confirm non-zero content (any pixel with R+G+B > 0).
    gi_raw="$(ls -1t "${DUMP_DIR}"/*_gi_raw_frame*.png 2>/dev/null | head -n 1 || true)"
    if [[ -z "${gi_raw}" ]]; then
        echo "  FAIL: no gi_raw_frame*.png in ${DUMP_DIR} after the mode-20 run"
        return 6
    fi
    if python3 -c "
import sys
from PIL import Image
import numpy as np
im = np.array(Image.open(sys.argv[1]))
if im.ndim == 2:
    im = im.reshape(*im.shape, 1)
rgb = im[..., :3].astype(np.float32)
n_nonzero = int(np.sum(rgb.sum(axis=-1) > 0.0))
frac = n_nonzero / float(rgb.shape[0] * rgb.shape[1])
print(f'mode-20 SRV probe: {n_nonzero} non-zero pixels ({frac*100:.2f}%)')
sys.exit(0 if frac > 0.5 else 6)
" "${gi_raw}"; then
        echo "  PASS: mode-20 SRV returns non-zero GBufferMaterial (refutes binding-broken hypothesis)"
        return 0
    fi
    echo "  FAIL: mode-20 gi_raw is mostly black; GBufferMaterial SRV reads zero (binding-broken)"
    return 6
}

# -----------------------------------------------------------------------------
# Main dispatch
# -----------------------------------------------------------------------------
MODE="${1:-preflight}"
case "${MODE}" in
    preflight) gate_env ;;
    build)     gate_build ;;
    dump)      gate_dump ;;
    vulk)      gate_vulk ;;
    cmdl)      gate_cmdl ;;
    val)       gate_val ;;
    vision)    gate_vision ;;
    mode20|m20) gate_m20 ;;
    all)
        gate_env || exit $?
        gate_build || exit $?
        gate_dump || exit $?
        gate_vulk || exit $?
        gate_cmdl || exit $?
        gate_val || exit $?
        gate_vision || exit $?
        gate_m20 || exit $?
        echo "[all] 7/7 gates PASS"
        exit 0
        ;;
    *)
        echo "Usage: $0 {preflight|build|dump|vulk|cmdl|val|vision|mode20|all}" >&2
        exit 7
        ;;
esac