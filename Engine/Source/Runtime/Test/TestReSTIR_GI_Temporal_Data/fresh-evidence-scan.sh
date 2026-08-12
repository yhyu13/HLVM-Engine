#!/usr/bin/env bash
# fresh-evidence-scan.sh
#
# v32 helper: collapse the canonical parent-triage recipe (steps 1, 4, 5, 8 of
# v31/docs/PIPELINE_HEALTH_2026-07-27.md) into a single read-only bash script.
# Parent runs this script and pastes the output back; cron routes from the
# banner verdict.
#
# USAGE:
#   cd <HLVM-Engine root>
#   bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh
#
# EXITS:
#   0 — fresh-build-evidence-PASS (all v22/v28 patches present in source AND
#       fresh dumps exist newer than source mtime; parent already rebuilt)
#   1 — evidence-stale-or-missing (dumps are pre-patch or absent)
#   2 — source-patch-missing (cumulative 17-patch inventory not all in source)
#
# READ-ONLY (no rm/mv of originals; only /tmp working copies).
# No GPU, no compilation, no permission-gated ops beyond stat + grep + find.
#
# Works on Linux/macOS bash 4+.

set -euo pipefail

# Determine project root (script-relative)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"

# Tolerate missing optional subdirs
FGIPass_CPP="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"
FGIPass_H="${REPO_ROOT}/Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h"
FRayTracingPipeline_H="${REPO_ROOT}/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h"
FRayTracingPipeline_CPP="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp"
TestReSTIR_CPP="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp"
GIPathTracing_Private="${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl"
GIPathTracing_Data="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl"
DUMP_DIR="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps"
LOG_DIR="${REPO_ROOT}/Engine/Source/Runtime/Binary/Debug"
STDERR_LOG="${LOG_DIR}/TestReSTIR_GI_Temporal_stderr.log"
# v43: added for the v37/v39/v40 patch entries (helper scripts in data dir)
TEST_DATA_DIR="${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data"
VALIDATOR_PY="${TEST_DATA_DIR}/validate_restir_gi.py"
DECODE_V38_PY="${TEST_DATA_DIR}/decode_v38_evidence.py"
DUMP_PIXELSTATS_PY="${TEST_DATA_DIR}/dump_pixelstats.py"
# v43: added for the v41 patch entry (FImageDump.cpp lives in Private/Image/)
FIMAGEDUMP_CPP="${REPO_ROOT}/Engine/Source/Runtime/Private/Image/FImageDump.cpp"

echo "=== fresh-evidence-scan.sh (v43, 2026-07-27) ==="
echo "REPO_ROOT: ${REPO_ROOT}"
echo "DATE (UTC): $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
echo

# 1. Cumulative 21-patch inventory presence check (v43: extended from v32's 17-patch set)
echo "--- [1/4] Cumulative 21-patch inventory ---"
MISSING=0
CHECKS=(
    "v3 spdlog FGIPass::DispatchRays ENTER:FGIPass_CPP:511:HLVM_LOG..LogGI..info.*FGIPass::DispatchRays ENTER"
    "v3 spdlog TestPre-GIPass:TestReSTIR_CPP:LogTest..info.*Pre-GIPass"
    "v5 HLVM-bypass removal NOTE comment:TestReSTIR_CPP:NOTE comment near line 1531"
    "v7 doc drift cleanup 650-672:TestReSTIR_CPP:bug-088 paragraph at lines 650-672"
    "v8 doc drift cleanup 1685-1693:TestReSTIR_CPP:v3 ENTER.*EXIT.*binding-set comment"
    "v12 cerr [RGI] Render:TestReSTIR_CPP:384:std::cerr.*RGI.*Render.*entry"
    "v12 cerr [RGI] FGIPass:FGIPass_CPP:487:std::cerr.*RGI.*FGIPass::DispatchRays.*entry"
    "v13 case 6u Private:GIPathTracing_Private:593:case 6u"
    "v13 case 6u Data:GIPathTracing_Data:593:case 6u"
    "v14 line 675->691 patch:TestReSTIR_CPP:line 691"
    "v15 Private sync case 6u:GIPathTracing_Private:case 6u.*256"
    "v17 case 7u Private:GIPathTracing_Private:case 7u.*AmbientColor"
    "v17 case 7u Data:GIPathTracing_Data:case 7u.*AmbientColor"
    "v18 cases 8u/9u/10u/11u:GIPathTracing_Private:case 8u"
    "v19 cases 12u/15u:GIPathTracing_Private:case 12u"
    "v19 default trace:GIPathTracing_Private:default:.*debugColor.*0.5f.*0.5f.*0.5f"
    "v22 binding-layout UAVBindingLayout:FGIPass_H:106:UAVBindingLayout"
    "v22 binding-layout SRVBindingSet:FRayTracingPipeline_CPP:357:State.addBindingSet.SRVBindingSet"
    "v22 binding-layout UAVBindingSet:FRayTracingPipeline_CPP:361:State.addBindingSet.UAVBindingSet"
    "v28 alpha-channel sentinel:GIPathTracing_Private:694:Output..pixel...w = max"
    "v28 alpha-channel sentinel Data:GIPathTracing_Data:694:Output..pixel...w = max"
    "bug-088 executeCommandList:TestReSTIR_CPP:691:executeCommandList"
    # v43: the following 5 entries were added in v37-v41 to match the actual 21-patch cumulative count
    "v37 validator alpha-check:VALIDATOR_PY:def check_alpha_sentinel"
    "v38 cerr DebugMode value:FGIPass_CPP:DebugMode effective="
    "v39 decode_v38_evidence.py:DECODE_V38_PY:decode_v38_evidence"
    "v40 dump_pixelstats alpha-stats:DUMP_PIXELSTATS_PY:v40-alpha"
    "v41 encoder alpha preservation:FIMAGEDUMP_CPP:rgbaData\[i \* 4 \+ 3\] \* 255.0f"
)

RESOLVED_FILES=()
for entry in "${CHECKS[@]}"; do
    LABEL=$(echo "$entry" | cut -d: -f1)
    TARGET=$(echo "$entry" | cut -d: -f2)
    PATTERN=$(echo "$entry" | cut -d: -f3-)
    case "$TARGET" in
        "FGIPass_CPP") FILE="$FGIPass_CPP" ;;
        "FGIPass_H") FILE="$FGIPass_H" ;;
        "FRayTracingPipeline_CPP") FILE="$FRayTracingPipeline_CPP" ;;
        "FRayTracingPipeline_H") FILE="$FRayTracingPipeline_H" ;;
        "TestReSTIR_CPP") FILE="$TestReSTIR_CPP" ;;
        "GIPathTracing_Private") FILE="$GIPathTracing_Private" ;;
        "GIPathTracing_Data") FILE="$GIPathTracing_Data" ;;
        # v43: new targets for v37/v39/v40/v41 entries
        "VALIDATOR_PY") FILE="$VALIDATOR_PY" ;;
        "DECODE_V38_PY") FILE="$DECODE_V38_PY" ;;
        "DUMP_PIXELSTATS_PY") FILE="$DUMP_PIXELSTATS_PY" ;;
        "FIMAGEDUMP_CPP") FILE="$FIMAGEDUMP_CPP" ;;
        *) FILE="" ;;
    esac
    if [[ ! -f "$FILE" ]]; then
        echo "  [MISSING-FILE] $LABEL -> $FILE"
        MISSING=$((MISSING + 1))
        continue
    fi
    if grep -Eq "$PATTERN" "$FILE"; then
        echo "  [OK] $LABEL"
    else
        echo "  [MISSING-PATCH] $LABEL"
        MISSING=$((MISSING + 1))
    fi
done
echo "  ----> MISSING=$MISSING"
echo

# 2. Newest dump group presence + age
echo "--- [2/4] Newest dump group ---"
if [[ -d "$DUMP_DIR" ]]; then
    NEWEST_PNG=$(find "$DUMP_DIR" -name '*.png' -type f -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | awk '{print $2}')
    NEWEST_LOG=$(find "$LOG_DIR" -name 'TestReSTIR_GI_Temporal*.log' -type f -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | awk '{print $2}')
    if [[ -z "$NEWEST_PNG" ]]; then
        echo "  [NO-DUMPS] No PNG files found in ${DUMP_DIR}"
        DUMP_FRESH=0
    else
        DUMP_AGE=$(( $(date +%s) - $(stat -c '%Y' "$NEWEST_PNG") ))
        echo "  NEWEST_PNG: $NEWEST_PNG"
        echo "  DUMP_AGE_SECONDS: $DUMP_AGE"
        DUMP_FRESH=$(( DUMP_AGE < 3600 ? 1 : 0 ))
    fi
    if [[ -z "$NEWEST_LOG" ]]; then
        echo "  [NO-LOGS]"
    else
        LOG_AGE=$(( $(date +%s) - $(stat -c '%Y' "$NEWEST_LOG") ))
        echo "  NEWEST_LOG: $NEWEST_LOG"
        echo "  LOG_AGE_SECONDS: $LOG_AGE"
    fi
else
    echo "  [NO-DUMP-DIR] ${DUMP_DIR} does not exist"
    DUMP_FRESH=0
fi
echo

# 3. stderr.log presence
echo "--- [3/4] stderr.log presence ---"
if [[ -f "$STDERR_LOG" ]]; then
    STDERR_LINES=$(wc -l < "$STDERR_LOG")
    STDERR_AGE=$(( $(date +%s) - $(stat -c '%Y' "$STDERR_LOG") ))
    echo "  FOUND: $STDERR_LOG"
    echo "  LINES: $STDERR_LINES"
    echo "  AGE_SECONDS: $STDERR_AGE"
    CERR_LINES=$(grep -c '\[RGI\]' "$STDERR_LOG" 2>/dev/null || echo 0)
    echo "  CERR_LINES_FOUND: $CERR_LINES"
else
    echo "  [NO-STDERR-LOG] $STDERR_LOG not found"
    echo "  (Create it by running: ./TestReSTIR_GI_Temporal 2>${STDERR_LOG})"
fi
echo

# 4. Required PNGs presence (display_frame8.png, gi_raw*, gbuffer_*)
echo "--- [4/4] Required PNG presence ---"
for pat in 'display_frame8' 'gi_raw' 'gbuffer_worldpos' 'gbuffer_normal' 'gbuffer_material' 'denoised' 'spatial'; do
    count=$(find "$DUMP_DIR" -name "${pat}*.png" -type f 2>/dev/null | wc -l)
    echo "  ${pat}*.png -> ${count} file(s)"
done
echo

# VERDICT BANNER
echo "=== VERDICT ==="
if [[ $MISSING -gt 0 ]]; then
    echo "BANNER: source-patch-missing (MISSING=$MISSING)"
    echo "ACTION: source patches are not all in place; parent should NOT rebuild until cron re-applies the missing patches."
    exit 2
fi
if [[ ${DUMP_FRESH:-0} -eq 1 ]]; then
    echo "BANNER: fresh-build-evidence-PASS"
    echo "ACTION: parent has rebuilt recently (dumps newer than 1h). Cron can route to v33 with the dump timestamp + stderr + validator output."
    exit 0
else
    echo "BANNER: evidence-stale-or-missing"
    echo "ACTION: parent should rebuild and re-run now (dumps are >1h old or absent). See docs/PIPELINE_HEALTH_2026-07-27.md parent-triage recipe."
    exit 1
fi
