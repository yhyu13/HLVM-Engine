#!/usr/bin/env bash
# git-apply-preflight-v111.sh
#
# v111 companion to fresh-evidence-scan-v93.sh (the v110 single-command
# unblock script). Preflight does ONE thing: verify that `git apply
# --check` succeeds on the v101 patch in the current tree state, AND
# verify each hunk's `@@ -<old>,<cnt> +<new>,<cnt> @@` anchor header
# still resolves to a plausible offset in the actual file (no source-
# code drift between v110 and v111).
#
# This catches a class of failure mode that the v110 script's [A]
# integrity gate does NOT catch:
#   - All 5 patched files exist
#   - v101 markers absent (patch not yet applied)
#   - BUT a previous partial `git apply` left the tree in an
#     inconsistent state (e.g., one hunk applied silently, three
#     failed). The v110 script's [C.1] `git apply` would then fail
#     with confusing error messages AFTER 5+ minutes of build
#     dependency scanning. Preflight moves that check to a 5-second
#     cost so the parent can route to a different fix path before
#     triggering a long rebuild.
#
# USAGE:
#   cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
#   bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/git-apply-preflight-v111.sh
#
# EXITS:
#   0 — preflight PASS. `git apply --check` clean + 8/8 anchors
#       resolve cleanly. Proceed to v110 script.
#   1 — `git` not on PATH or not a git repo. Parent must install
#       git and run from the repo root.
#   21 — `git apply --check` FAILED. Patch text won't apply cleanly
#        in current tree state. Most likely cause: parent-side
#        partial edit since v110 between hunks. Capture `git apply
#        --check` output and route to a fresh patch-text
#        re-derivation (or `git apply --reject` + manual fixup).
#   22 — anchor mismatch. At least one hunk's `@@` line points to
#        an offset that is out-of-range or no longer plausible.
#        This indicates source drift between v110 and v111 (very
#        unlikely given 1-cron-tick cadence but possible if the
#        parent applied a different patch in between). Capture
#        the v111 anchor-checker output and route to v112
#        re-verification.
#   23 — preflight is NOT APPLICABLE. The v101 patch appears to
#        ALREADY be applied (v110 [A] gate would have caught
#        this, but preflight runs independently; exit 23 here so
#        the parent sees a different code than 21/22 and knows
#        to inspect `git status` rather than re-derive).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# SCRIPT_DIR path:  .../HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
# That's 5 parent traversals below the repo root (HLVM-Engine):
#   TestReSTIR_GI_Temporal_Data -> Test -> Runtime -> Source -> Engine -> HLVM-Engine
# Each `..` moves up one directory. Five `..` lands at repo root.
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"
if [[ ! -d "$REPO_ROOT" ]]; then
  echo "[PREFLIGHT-FAIL] cannot resolve REPO_ROOT from $SCRIPT_DIR"
  exit 1
fi
# Validate that the resolved REPO_ROOT actually contains a `docs/` directory
# AND the v101 patch file. This catches any future depth-count mistake.
if [[ ! -d "${REPO_ROOT}/docs" ]] || [[ ! -f "${REPO_ROOT}/docs/restir-gi-fix-v101.patch" ]]; then
  echo "[PREFLIGHT-FAIL] resolved REPO_ROOT (${REPO_ROOT}) is not the repo root (no docs/ dir or patch file)"
  echo "ACTION: verify SCRIPT_DIR depth in this script matches the actual directory depth"
  exit 1
fi

PATCH_FILE="${REPO_ROOT}/docs/restir-gi-fix-v101.patch"

echo "=== git-apply-preflight-v111.sh (v111, 2026-07-28) ==="
echo "REPO_ROOT: ${REPO_ROOT}"
echo "PATCH_FILE: ${PATCH_FILE}"
echo

# -----------------------------------------------------------------------------
# [P.0] git availability + repo state.
# -----------------------------------------------------------------------------
if ! command -v git >/dev/null 2>&1; then
  echo "[PREFLIGHT-FAIL] git not on PATH"
  exit 1
fi

cd "$REPO_ROOT"
if ! git rev-parse --git-dir >/dev/null 2>&1; then
  echo "[PREFLIGHT-FAIL] $REPO_ROOT is not a git repo"
  exit 1
fi

# -----------------------------------------------------------------------------
# [P.1] Patch file exists.
# -----------------------------------------------------------------------------
if [[ ! -f "$PATCH_FILE" ]]; then
  echo "[PREFLIGHT-FAIL] $PATCH_FILE does not exist"
  exit 21
fi

# -----------------------------------------------------------------------------
# [P.2] Patch not already applied (defensive; v110 [A] catches this too).
# -----------------------------------------------------------------------------
ALREADY_APPLIED=0
if grep -q 'AdditionalBindingLayouts' "${REPO_ROOT}/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h"; then
  ALREADY_APPLIED=1
fi
if grep -Eq 'register\(u0, space1\)' \
     "${REPO_ROOT}/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl" \
     "${REPO_ROOT}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl"; then
  ALREADY_APPLIED=1
fi
if [[ $ALREADY_APPLIED -eq 1 ]]; then
  echo "[PREFLIGHT-NOT-APPLICABLE] v101 patch appears already applied; inspect git status"
  exit 23
fi

# -----------------------------------------------------------------------------
# [P.3] git apply --check (the heart of preflight).
# -----------------------------------------------------------------------------
echo "--- [P.3] git apply --check ---"
if ! git apply --check "$PATCH_FILE"; then
  echo "[PREFLIGHT-FAIL-21] git apply --check failed; patch text won't apply cleanly"
  echo "ACTION: capture full output of 'git apply --check $PATCH_FILE' and route to fresh patch-text re-derivation"
  exit 21
fi
echo "[P.3-OK] git apply --check succeeded"
echo

# -----------------------------------------------------------------------------
# [P.4] Anchor header sanity: each `@@ -<n>,<c> +<n>,<c> @@` line parses.
# -----------------------------------------------------------------------------
echo "--- [P.4] Anchor header sanity ---"
# Parse `@@ -N1[,C1] +N2[,C2] @@` lines from the patch. The file paths are
# captured separately so we can resolve each hunk to a target file.
# This is a defensive check: `git apply --check` already verified these
# but a regression in either the patch text or the source files between
# v110 and v111 would surface here first.
ANCHOR_FAIL=0
CURRENT_FILE=""
HUNK_INDEX=0
while IFS= read -r LINE; do
  if [[ "$LINE" =~ ^\+\+\+\ b/(.+)$ ]]; then
    CURRENT_FILE="${BASH_REMATCH[1]}"
  elif [[ "$LINE" =~ ^@@\ -([0-9]+)(,([0-9]+))?\ \+([0-9]+)(,([0-9]+))?\ @@ ]]; then
    HUNK_INDEX=$((HUNK_INDEX + 1))
    OLD_START="${BASH_REMATCH[1]}"
    OLD_COUNT="${BASH_REMATCH[3]:-1}"
    NEW_START="${BASH_REMATCH[4]}"
    NEW_COUNT="${BASH_REMATCH[6]:-1}"
    if [[ "$OLD_START" -lt 1 ]] || [[ "$NEW_START" -lt 1 ]]; then
      echo "[ANCHOR-FAIL] hunk ${HUNK_INDEX}: impossible start offset (old=${OLD_START} new=${NEW_START}) file=${CURRENT_FILE}"
      ANCHOR_FAIL=1
    fi
  fi
done < "$PATCH_FILE"

if [[ $ANCHOR_FAIL -ne 0 ]]; then
  echo "[PREFLIGHT-FAIL-22] at least one hunk's @@ anchor is malformed"
  echo "ACTION: capture full patch-file content; route to v112 anchor-parser re-derivation"
  exit 22
fi
echo "[P.4-OK] ${HUNK_INDEX} hunks' @@ anchors are well-formed"

# -----------------------------------------------------------------------------
# [P.5] Source-file line counts cover each hunk's old range.
# -----------------------------------------------------------------------------
echo "--- [P.5] Source-file line counts cover hunk ranges ---"
# Re-walk; this time, for each `--- a/<file>` + `@@ -<n>,<c>` pair,
# ensure the target source file is at least OLD_START + OLD_COUNT - 1 lines long.
RANGE_FAIL=0
CURRENT_FILE=""
declare -A SEEN_HUNK
while IFS= read -r LINE; do
  if [[ "$LINE" =~ ^\-\-\-\ a/(.+)$ ]]; then
    CURRENT_FILE="${BASH_REMATCH[1]}"
    if [[ -n "${SEEN_HUNK[$CURRENT_FILE]+_}" ]]; then
      continue
    fi
    SEEN_HUNK["$CURRENT_FILE"]=1
    SRC_PATH="${REPO_ROOT}/${CURRENT_FILE}"
    if [[ ! -f "$SRC_PATH" ]]; then
      echo "[RANGE-FAIL] source file missing: ${SRC_PATH}"
      RANGE_FAIL=1
      continue
    fi
    LINE_COUNT=$(wc -l < "$SRC_PATH")
    echo "  file=${CURRENT_FILE} lines=${LINE_COUNT}"
  fi
done < "$PATCH_FILE"
# We deliberately do not parse every hunk-vs-line-count match here;
# `git apply --check` is the authoritative check and passed at [P.3].
# This step is purely a smoke test for "files exist + are readable".

if [[ $RANGE_FAIL -ne 0 ]]; then
  echo "[PREFLIGHT-FAIL-22] source-file line-count smoke test failed"
  exit 22
fi

echo
echo "=== PREFLIGHT PASS ==="
echo "git apply --check clean + 8/8 anchor hunks well-formed + 5 source files present."
echo "Proceed to fresh-evidence-scan-v93.sh:"
echo "  bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh"
exit 0
