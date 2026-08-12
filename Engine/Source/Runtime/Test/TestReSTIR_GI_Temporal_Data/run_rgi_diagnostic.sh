#!/usr/bin/env bash
# run_rgi_diagnostic.sh
#
# v20 + v23 evidence-capture protocol: 1 Debug build + 10 mode runs
# (default/6/7/8/9/10/11/12/15/99) + 1 validator. Dumps rotated per mode
# (archive AFTER each run, named with the mode that PRODUCED the output).
#
# USAGE:
#   cd <HLVM-Engine root>
#   bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
#
# REQUIRES:
#   - Working directory at HLVM-Engine root (script auto-detects)
#   - HLVM_DUMP_RGI=1 enabled by default (HLVM_DUMP_RGI=0 to disable)
#   - Vulkan-capable GPU on host (NVIDIA RTX 3090 verified)
#   - For results: writes rgi_evidence.txt in CWD for paste-back to cron

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"
cd "${REPO_ROOT}"

# --- Pre-flight: clean stale dumps_YYYYMMDD_HHMMSS archives ---
# v23 pattern: archive AFTER the run, named with the mode. Pre-clean only
# removes stale archives from aborted prior runs.
echo "Pre-flight: cleaning stale dumps_* archives from aborted runs..."
for stale in "${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data"/dumps_*; do
    if [[ -d "$stale" ]]; then
        echo "  REMOVE: $stale"
        rm -rf "$stale"
    fi
done

# --- Step 1: build ---
echo "[1/12] Building Debug target..."
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal

# --- Step 2-11: one run per mode ---
TEST_BIN="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal"
DATA_DIR="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data"
DUMP_DIR="${DATA_DIR}/dumps"

declare -A MODE_NAMES=(
    [0]="default"
    [6]="mode06_uav_sentinel"
    [7]="mode07_traceray_bypass"
    [8]="mode08_traceray_only"
    [9]="mode09_diffuse_only"
    [10]="mode10_cbuffer_reach"
    [11]="mode11_view_cbuffer_reach"
    [12]="mode12_ambientcolor_only"
    [15]="mode15_debugmode_raw"
    [99]="mode99_unused_path"
)

mkdir -p "$DUMP_DIR"

for mode in 0 6 7 8 9 10 11 12 15 99; do
    name="${MODE_NAMES[$mode]}"
    echo "[2/12] Mode $mode ($name) ..."

    # Run with the appropriate env var; HLVM_DUMP_RGI=1 enables PNG dumps
    if [[ "$mode" -eq 0 ]]; then
        HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
            "./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal" 2>&1 \
            | tee "/tmp/rgi_mode${mode}.log" || true
    else
        HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE="$mode" \
            "./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal" 2>&1 \
            | tee "/tmp/rgi_mode${mode}.log" || true
    fi

    # v23 archive-after-run: move dumps to per-mode dir AFTER the run, named
    # with the mode that PRODUCED them.
    STAMP="$(date -u +'%Y%m%d_%H%M%S')"
    ARCHIVE_DIR="${DATA_DIR}/dumps_${name}_${STAMP}"
    if [[ -d "$DUMP_DIR" ]] && [[ -n "$(ls -A "$DUMP_DIR" 2>/dev/null)" ]]; then
        # Use mv if cross-device-friendly, fall back to cp -r + rm
        if mv "$DUMP_DIR" "$ARCHIVE_DIR" 2>/dev/null; then
            echo "  ARCHIVED: $ARCHIVE_DIR"
        else
            mkdir -p "$ARCHIVE_DIR"
            cp -r "$DUMP_DIR"/* "$ARCHIVE_DIR"/ 2>/dev/null || true
            rm -rf "$DUMP_DIR"/*
            echo "  ARCHIVED (cp+rm fallback): $ARCHIVE_DIR"
        fi
        mkdir -p "$DUMP_DIR"  # restore for next run
    fi
done

# --- Step 12: validator + evidence summary ---
echo "[12/12] Running validator + collecting evidence..."
EVIDENCE_FILE="${REPO_ROOT}/rgi_evidence.txt"
{
    echo "=== rgi_evidence.txt ==="
    echo "Generated: $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    echo "Repo: ${REPO_ROOT}"
    echo
    echo "--- Fresh-dump archives produced this run ---"
    ls -la "${DATA_DIR}"/dumps_* 2>/dev/null | head -40 || echo "  (none)"
    echo
    echo "--- Validator output (most recent stamp group) ---"
    if [[ -d "$DUMP_DIR" ]] && [[ -n "$(ls -A "$DUMP_DIR" 2>/dev/null)" ]]; then
        # Default-mode dump group was archived last; use the last archive
        LATEST_ARCHIVE=$(ls -td "${DATA_DIR}"/dumps_default_* 2>/dev/null | head -1 || echo "")
        if [[ -n "$LATEST_ARCHIVE" ]]; then
            python3 "${DATA_DIR}/validate_restir_gi.py" || true
            echo
            echo "--- dump_pixelstats.py output (latest archive) ---"
            python3 "${DATA_DIR}/dump_pixelstats.py" "$LATEST_ARCHIVE" 2>/dev/null || true
        else
            echo "  (no default-mode archive found; validator cannot run)"
        fi
    fi
    echo
    echo "--- Per-mode log tail (mode=15 + mode=6 most important) ---"
    for mode in 6 15 0; do
        if [[ -f "/tmp/rgi_mode${mode}.log" ]]; then
            echo "--- mode $mode tail ---"
            tail -30 "/tmp/rgi_mode${mode}.log"
        fi
    done
} > "$EVIDENCE_FILE" 2>&1 || true

echo "DONE. Evidence: ${EVIDENCE_FILE}"
echo "  paste rgi_evidence.txt back to cron (or run docs helper fresh-evidence-scan.sh first)."
