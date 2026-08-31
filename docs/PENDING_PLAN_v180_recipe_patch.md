# v176-recipe.sh extension patch (SUPERSEDED by v181 — applied directly to disk)

> **STATUS (2026-08-29, tick-now-490):** This file is **SUPERSEDED by v181**. The
> patch was applied directly to `v176-recipe.sh` via the `patch` file tool during
> the v181 cycle (`docs/PENDING_COMMIT_v181.md`). Operator does NOT need to `git
> apply` this diff. The recipe now has `--mode-30`, `--mode-31`, BLUE-MID +
> gray-mid envelopes, gates 8 and 9, and per-leaf discriminator verdict text.
>
> The text below is preserved as historical v180 staging rationale. Read
> `docs/PENDING_PLAN_v181.md` for the current authoritative recipe extension
> contract; read `docs/PENDING_TEST_AUDIT_v181.md` for the 10/10 PASS verifier.

---

# Original v180 staging

This file documents the staged patch that v180 cycle proposes for `v176-recipe.sh`.
The cron runspace CAN apply file patches to v176-recipe.sh directly via `patch`
tool (file-only). The operator can still apply manually if preferred.

The operator can apply this patch at the keyboard by `git apply` or by manual edit.

## Diff (applies to `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh`)

```diff
@@ -49,6 +49,7 @@
 VALIDATOR="${TEST_DATA_DIR}/validate_restir_gi.py"
 SKIP_BUILD=0
 RUN_MODE_20=0
+RUN_MODE_31=0

 # ---- args -------------------------------------------------------------------
 for arg in "$@"; do
   case "$arg" in
     --skip-build) SKIP_BUILD=1 ;;
     --mode-20)    RUN_MODE_20=1 ;;
+    --mode-31)    RUN_MODE_31=1 ;;
     --help|-h)
       sed -n '2,40p' "${BASH_SOURCE[0]}"
       exit 0
       ;;
@@ -203,10 +204,30 @@
       *)
         echo "  [DIAGNOSIS] variance-failure (non-uniform but no scene structure)."
         ...
         ;;
+    esac
+    # v181 discriminator: BLUE-MID (mode 31 SRV-alive-but-value-zero) and
+    # gray-mid (slangc dead-strip) envelopes. Mode 31 returns either
+    # non-uniform RGB (binding works + value real) or BLUE (binding works
+    # + value zero — discriminator pivot to RenderGBuffer). The
+    # validation probe must catch the blue-mid envelope (mean=1/3, sd<0.005)
+    # explicitly, not fall through to "variance".
+    MEAN_CHK=$(python3 -c "from PIL import Image; import numpy as np; \
+      a=np.asarray(Image.open('${DISPLAY_PNG_PROBE}').convert('RGBA'),dtype=np.float32)/255.0; \
+      rgb=a[...,:3]; print(f'{float(rgb.mean()):.4f} {float(rgb.std()):.4f}')" 2>/dev/null || echo "0 0")
+    MEAN_VAL=$(echo "$MEAN_CHK" | awk '{print $1}')
+    STD_VAL=$(echo "$MEAN_CHK" | awk '{print $2}')
+    MU_1_3=$(python3 -c "print(abs(float('${MEAN_VAL}') - 0.3333) < 0.05)")
+    if [[ "${STD_VAL%.*}" == "0" ]] && [[ "$MU_1_3" == "True" ]]; then
+      echo "  [DIAGNOSIS] blue-mid discriminator — SRV alive but value is zero."
+      echo "              Per v180 hypothesis tree leaf 1, binding works; upstream"
+      echo "              raster pass or sentinel writes zero. Bisect pivot: RenderGBuffer"
+      echo "              in TestReSTIR_GI_Temporal.cpp:2022-2267."
+    elif [[ "${STD_VAL%.*}" == "0" ]] && [[ "${MEAN_VAL%.*}" == "0" || "${MEAN_VAL%.*}" == "1" ]]; then
+      # v25-uniform-white / v24-uniform-zero — already handled above
+      :
+    fi
+    case "${SIG}" in
     esac
   fi
   fail "validate_restir_gi.py exit ${VALIDATOR_EXIT} ..."
 fi
@@ -286,3 +307,30 @@
   warn "Gate 7 (HLVM_PT_DEBUG_MODE=20) skipped. Re-run with --mode-20 to check GBufferMaterial."
 fi

+# ---- gate 8 (optional): HLVM_PT_DEBUG_MODE=31 discriminator -----------------
+# v180 discriminator: runs the SRV-bound GBufferMaterial Load with a non-trivial
+# +0.1f offset so the read is observable to slangc's reachability analysis.
+# Per GIPathTracing.hlsl:782-791 (case 31u):
+#   aliveSentinel = GBufferMaterial.Load(int3(pixel,0)).rgb * 0.5f + 0.1f
+#   if any aliveSentinel > 0.1: write aliveSentinel  (binding alive + value real)
+#   else: write float3(0,0,1) — BLUE (binding alive + value zero)
+# The default branch (line 803) returns gray (0.5, 0.5, 0.5) — slangc
+# dead-stripped the entire case. So 4 distinct visual signatures:
+#   BLUE  (mean=1/3, sd<0.005) — binding works + value zero — Scenario 1
+#   NON-UNIFORM (varied)       — binding works + value real — Scenario 2
+#   GRAY  (mean=0.5, sd<0.005) — slangc dead-strip — Scenario 3
+#   BLACK (mean=0, sd<0.005)   — binding universally broken — Scenario 5
+if [[ "$RUN_MODE_31" -eq 1 ]]; then
+  gate 7 "HLVM_PT_DEBUG_MODE=31 discriminator (gi_raw dump, +0.1f offset)"
+  PRE_MODE31_TS="$(date +%Y%m%d_%H%M%S)"
+  PRE_MODE31_LOG="${LOG_FILE}_${PRE_MODE31_TS}_pre_mode31"
+  [[ -f "${LOG_FILE}" ]] && mv -f "${LOG_FILE}" "${PRE_MODE31_LOG}"
+  cd "${LOG_DIR}"
+  HLVM_PT_DEBUG_MODE=31 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 300 ./TestReSTIR_GI_Temporal 2>&1 | tail -5 \
+    || warn "non-zero exit (may be window-driven)"
+  MODE31_TS=$(find "${DUMP_DIR}" -maxdepth 1 -name "[0-9]*_[0-9]*_gi_raw_frame*.png" -newer "${PRE_MODE31_LOG}" 2>/dev/null \
+    | sed -E 's@.*/([0-9]{8}_[0-9]{6})_.*@\1@' | sort -u | tail -1 || true)
+  if [[ -z "${MODE31_TS}" ]]; then
+    fail "no fresh gi_raw dump from mode-31 run" 6
+  fi
+  GIRAW_PNG=$(find "${DUMP_DIR}" -maxdepth 1 -name "${MODE31_TS}_gi_raw_frame*.png" | head -1)
+  echo "  mode-31 gi_raw PNG: ${GIRAW_PNG}"
+  echo "  Vision-interpret signature: BLUE (binding-alive-zero) / NON-UNIFORM (binding-alive-real) / GRAY (slangc dead-strip) / BLACK (binding broken)"
+  echo "  Per v180 hypothesis tree: 5-leaf discriminator. Report leaf to the testing-verifier."
+fi
```

## Operator command to apply

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# Approach 1: manual edit using the diff above (3 hunks, additive only)
# Approach 2: copy-paste the recipe from a clean source re-run after edit
#
# After applying, verify:
grep -c 'mode-31' Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh
# Expect: ≥5 (declaration, --help, gate label, run command, branch name)
```

## Rollback

If the operator wants to back out the patch:
```bash
git checkout HEAD -- Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh
```

The patch is purely additive (no removal of existing lines); the only "delete" hunks
are part of the case-statement refactor where the new BLUE-MID + gray-mid branch is
inserted before the existing `case "${SIG}" in esac` ender. Reverting is safe.

## What this patch does NOT change

- FGIPass.cpp / GIPathTracing.hlsl / TestReSTIR_GI_Temporal.cpp — untouched
- The discriminator scope is entirely in the recipe (a build + run + analyze script)
- No production code path changes
- No CVar value changes
- No shader recompilation (mode 31 is already in source per DIAGNOSTIC_2026-07-30.md)
