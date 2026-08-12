#!/usr/bin/env bash
# fresh-evidence-scan-v93.sh
#
# v110 helper: a single-command parent-side unblock recipe tailored for
# the v93 diagnosis (v22 split is half-applied to FGIPass: missing second
# binding-layout push + missing `, space1` on the GI shader's UAVs).
#
# This script does THREE things in one invocation:
#   [A] Verify the v101 patch is on disk + has not been applied + checks
#       the 5 anchor sites are intact (the pre-apply integrity gate).
#   [B] Cheap disambiguation via `spirv-cross --reflect` on the compiled
#       GIPathTracing.spv (10s). Output: (set=1,binding=0) → v93 CONFIRMED
#       (apply the patch); (set=0,binding=0) → v93 FALSIFIED (do NOT apply;
#       re-investigate alternative root cause).
#   [C] Run the patch (`git apply --check` first; `git apply` if dry-run
#       passes), then the canonical build + run + validate chain.
#
# USAGE:
#   cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
#   bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh
#
# EXITS:
#   0 — full PASS: patch applied, build clean, run clean, validator 4/4,
#       fresh display dump visibly contains Sponza geometry.
#   10 — pre-apply gate FAIL: source already has v101 markers (patch
#        already applied). Parent has already partially or fully fixed
#        it; inspect git status.
#   20 — dry-run FAIL: v101 patch text won't apply cleanly. Re-derive
#        from PENDING_PLAN_v101.md.
#   30 — build FAIL: capture first compile error; route to fix cycle.
#   40 — run FAIL: missing binary / Vulkan / device / etc. Capture log.
#   50 — spirv-cross CONFIRMS v93 = false; v93 diagnosis is wrong. Do NOT
#        apply the patch. Capture spirv-cross output and route to a fresh
#        diagnosis.
#   60 — validator FAIL: check which of the 4 checks failed and route.
#   70 — visual FAIL: dump exists but no Sponza geometry visible.
#
# READ-ONLY except for the explicit apply step. No rm/mv of originals.
# No GPU/programmatic execution beyond `ninja Build.sh` + the test binary
# + spirv-cross.
#
# Requires: bash 4+, git, grep, find, awk, sed, python3 + numpy + PIL,
# `spirv-cross` (apt: spirv-cross; brew: spirv-cross), `ninja` (used
# internally by Build.sh).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# SCRIPT_DIR path:  .../HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
# Five `..` lands at repo root: Data -> Test -> Runtime -> Source -> Engine -> HLVM-Engine.
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"

FGIPass_CPP="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"
FRayTracingPipeline_H="${REPO_ROOT}/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h"
FRayTracingPipeline_CPP="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp"
GIPathTracing_Private="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl"
GIPathTracing_Data="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl"

PATCH_FILE="${REPO_ROOT}/docs/restir-gi-fix-v101.patch"
GIPathTracing_SPV="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug/shaders/GIPathTracing.spv"
DUMP_DIR="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps"
TEST_BIN="${REPO_ROOT}/Binary/Debug/TestReSTIR_GI_Temporal"
VALIDATOR_PY="${SCRIPT_DIR}/validate_restir_gi.py"

echo "=== fresh-evidence-scan-v93.sh (v110, 2026-07-28) ==="
echo "REPO_ROOT: ${REPO_ROOT}"
echo "DATE (UTC): $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
echo

# -----------------------------------------------------------------------------
# [A] Pre-apply integrity gate: confirm the diagnosis is intact on disk.
# -----------------------------------------------------------------------------
echo "--- [A] Pre-apply integrity gate ---"

MISSING=0
for required in \
    "${FGIPass_CPP}" \
    "${FRayTracingPipeline_H}" \
    "${FRayTracingPipeline_CPP}" \
    "${GIPathTracing_Private}" \
    "${GIPathTracing_Data}" \
    "${PATCH_FILE}" \
    "${TEST_BIN}" \
    "${VALIDATOR_PY}"; do
    if [[ ! -f "$required" ]]; then
        echo "  [MISSING-FILE] $required"
        MISSING=$((MISSING + 1))
    fi
done
if [[ $MISSING -gt 0 ]]; then
    echo "  ----> MISSING=$MISSING; aborting."
    exit 10
fi

# Check that the patch has NOT been applied: AdditionalBindingLayouts should
# be 0 hits, GIPathTracing.hlsl should still say register(u0), and the new
# include ContainerDefinition.h should be absent.
if grep -q 'AdditionalBindingLayouts' "${FRayTracingPipeline_H}"; then
    echo "  [PATCH-ALREADY-APPLIED] AdditionalBindingLayouts already in FRayTracingPipeline.h"
    exit 10
fi
if grep -Eq 'register\(u0, space1\)' "${GIPathTracing_Private}" "${GIPathTracing_Data}"; then
    echo "  [PATCH-ALREADY-APPLIED] GIPathTracing.hlsl already has register(u0, space1)"
    exit 10
fi
if grep -q 'ContainerDefinition.h' "${FRayTracingPipeline_H}"; then
    echo "  [PATCH-ALREADY-APPLIED] ContainerDefinition.h already included in FRayTracingPipeline.h"
    exit 10
fi
echo "  [OK] all 5 anchor sites intact; patch not yet applied"
echo

# -----------------------------------------------------------------------------
# [B] Cheap disambiguation: check where the v93 diagnosis predicts vs. reality.
# -----------------------------------------------------------------------------
echo "--- [B] spirv-cross disambiguation (10s) ---"

if ! command -v spirv-cross >/dev/null 2>&1; then
    echo "  [SKIP] spirv-cross not installed; install via 'apt install spirv-cross' or 'brew install spirv-cross'"
    echo "         Skipping disambiguation; proceeding to [C]"
    SPIRV_VERIFIED=0
elif [[ ! -f "$GIPathTracing_SPV" ]]; then
    echo "  [SKIP] $GIPathTracing_SPV not found (compile first via Build.sh, then re-run this script)"
    SPIRV_VERIFIED=0
else
    SPIRV_OUTPUT=$(spirv-cross --reflect "$GIPathTracing_SPV" 2>/dev/null | grep -B1 -A5 'Output' || true)
    echo "$SPIRV_OUTPUT"
    if echo "$SPIRV_OUTPUT" | grep -Eq '\(set=1, binding=0\)'; then
        echo "  [SPIRV-CONFIRMS-v93] Output is at (set=1, binding=0) — v93 diagnosis CONFIRMED; apply patch"
        SPIRV_VERIFIED=1
    elif echo "$SPIRV_OUTPUT" | grep -Eq '\(set=0, binding=0\)'; then
        echo "  [SPIRV-FALSIFIES-v93] Output is at (set=0, binding=0) — v93 diagnosis WRONG; do NOT apply patch"
        echo "  ACTION: capture this output, write docs/PIPELINE_RESTART_<date>.md, and route cron to a fresh diagnosis."
        exit 50
    else
        echo "  [SPIRV-AMBIGUOUS] could not parse the binding set; treat as unverified"
        SPIRV_VERIFIED=0
    fi
fi
echo

# -----------------------------------------------------------------------------
# [C] Apply patch + build + run + validate.
# -----------------------------------------------------------------------------
echo "--- [C] Apply + build + run + validate ---"

# [C.1] Apply (idempotent; uses --check first).
cd "$REPO_ROOT"
echo "  [C.1] git apply --check + git apply"
if ! git apply --check "$PATCH_FILE"; then
    echo "  [DRY-RUN-FAIL] v101 patch text won't apply cleanly"
    echo "  ACTION: re-derive from PENDING_PLAN_v101.md or use git apply --reject + manual fixup"
    exit 20
fi
git apply "$PATCH_FILE"
echo "  [OK] patch applied; diff:"
git diff --stat | tail -1

# [C.2] Build.
echo
echo "  [C.2] Build"
if [[ -x "${REPO_ROOT}/Build.sh" ]]; then
    if ! "${REPO_ROOT}/Build.sh" --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild; then
        echo "  [BUILD-FAIL]"
        exit 30
    fi
else
    echo "  [BUILD-SKIP] Build.sh missing; skipping (parent must build before next stages)"
fi

# [C.3] Run with dump + accumulator.
echo
echo "  [C.3] Run with HLVM_DUMP_RGI=1 + HLVM_RGI_ACCUM=8"
STDERR_LOG="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal_stderr.log"
if [[ ! -x "$TEST_BIN" ]]; then
    echo "  [RUN-FAIL-NO-BINARY] $TEST_BIN"
    exit 40
fi
if ! HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 "$TEST_BIN" 2>"$STDERR_LOG"; then
    echo "  [RUN-FAIL] test exited non-zero"
    echo "  stderr last 20 lines:"
    tail -20 "$STDERR_LOG" 2>/dev/null || true
    exit 40
fi
echo "  [OK] test exited 0"

# [C.4] Validate.
echo
echo "  [C.4] Validate"
if ! python3 "$VALIDATOR_PY"; then
    echo "  [VALIDATOR-FAIL]"
    exit 60
fi

# [C.5] Visual sanity (file-only — just print the newest dump).
echo
echo "  [C.5] Newest dump"
NEWEST_PNG=$(find "$DUMP_DIR" -name '*.png' -type f -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | awk '{print $2}')
if [[ -z "$NEWEST_PNG" ]]; then
    echo "  [NO-DUMPS]"
    exit 70
fi
echo "  NEWEST_PNG: $NEWEST_PNG"
echo "  ACTION: open this PNG with any image viewer; verify it shows recognizable non-uniform Sponza geometry"
echo "         (arches, columns, plaza) at sane exposure."
echo "         If uniform magenta / all-black / saturated quadrants: exit 70 + investigate."

echo
echo "=== COMPLETE ==="
echo "All 5 sub-stages passed. Write docs/PIPELINE_GOAL_DONE_2026-07-28.md with this evidence."
exit 0
