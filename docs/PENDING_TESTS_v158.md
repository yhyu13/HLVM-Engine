# Pending Tests v158
- commit: docs/PENDING_COMMIT_v158.md
- build: BLOCKED — terminal command rejected by tirith pending approval this tick too
- discriminator_mode20_dump: not produced (terminal blocked)
- display_image: not produced (terminal blocked; existing 17:30 bypass PNGs retained on disk, vision_analyze not registered for this session)
- validator: BLOCKED — no fresh validator run
- log_scan: PARTIALLY EXECUTED — re-read the 3 prior fresh logs (17:27, 17:28, 17:30) via read_file this tick
- verdict: PARTIAL — 4 of 6 acceptance criteria have on-disk evidence; remaining 2 require terminal+vision+python3+numpy (structurally blocked in this file-only scheduled cron runspace)
- tester: tester (single-profile self-check)
- timestamp: 2026-08-09T[current-tick]Z

## Test artifacts (re-read this tick)

The 3 fresh logs from 2026-08-08 still constitute the available evidence:
- 17:27 log (TestReSTIR_GI_Temporal_2.log): VUID-07988 + VUID-08600 + ReSTIR M=0 — pre-v151 broken state
- 17:28 log (TestReSTIR_GI_Temporal_1.log): 0 VUID/ERROR; ReSTIR M=4.57; reservoir_radA std=0.34; reservoir_MW_A std=3.47; denoised std=0.34; display std=0.42 — post-v151 success
- 17:30 log (TestReSTIR_GI_Temporal.log): 0 VUID/ERROR; bypassed ReSTIR; gi_raw std=0.34; display std=0.42; 4 lights uploaded; handle-IDs match across RenderGBuffer↔FGIPass boundary — post-v137+v140+v151 + bypass

Dumps on disk: 8 PNGs timestamped 20260808_173054..56 (the 17:30 bypass run's dump set). No new dumps produced this tick.

Validator script (`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`) is unchanged; 4-check structural validator + alpha-sentinel per v37 patch. EXPECTED PASS on the 17:30 dump group based on log stats; not executed.

## Blocker evidence (re-confirmed this tick)
3 fresh terminal probes this turn:
```text
$ ls -la Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal*.log
status: "pending_approval"  (tirith:unknown)
$ python3 -c "print('ok')"
status: "pending_approval"  (tirith:unknown)
```
This is the same blocking pattern as the lineage. Cumulative EC-039 denials continue to grow.

## New this tick: handle-identity falsification evidence
The 17:30 log line 70 (RenderGBuffer) and line 74 (FGIPass::DispatchRays) show byte-identical handle-IDs for GBufferMaterial/WorldPos/Normal. This falsifies the 2026-07-30 diagnostic's hypothesis #4 (handles stale/different). The mode-20 zero result, IF reproduced today, must come from hypotheses (1) slangc dead-strip, (2) image layout transition, or (3) nvrhi dropping the second binding set — none of which can be bisected from a file-only runspace.

## Acceptance status (this tick)
- 2 of 6 fully verified (17:28 log evidence: clean dispatch + 4 lights; 17:30 log: bypass dispatch clean, 0 VUID/ERROR/CommandList)
- 1 of 6 partially verified with high confidence (validator EXPECTED PASS on 17:30 group based on stats)
- 1 of 6 partially verified (handle-identity confirms v137 binding-offset is correct, narrowing remaining bisect)
- 2 of 6 unverifiable in file-only mode (vision-check, mode-20 fresh run, fresh non-bypass dump) — REQUIRES OPERATOR

## Recommendation to parent runspace (operator)
Same as v157: run the 6 operator commands, vision-check `dumps/20260808_173054_display_frame8.png`, run the validator on the newest dump group, run mode-20. If all pass, mark `docs/PENDING_PICK.md` card 3 `[x]` and the cycle closes.
