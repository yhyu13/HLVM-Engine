# Pending Tests v85
- commit: docs/PENDING_COMMIT_v85.md
- tester: tester (file-only)
- timestamp: 2026-07-28T23:10:00Z

## Part A — patch-presence spot-checks (file-only)

v85 is a cron-RESUMED documentation-only tick. The patch-presence probes for this cycle are 2 NEW sites (not previously probed by v25-v84):

| Test | Subject | Search probe | Expected | Actual | Verdict |
|------|---------|--------------|----------|--------|---------|
| A1 | v22 SRV-only binding layout chain at FGIPass.cpp:284-295 | `read_file` window 280-296 | 11 `Add*` entries (b0/b1/t1/t2/t3/t5/t6/t7/t8/s2) chained off `Builder.SetVisibility(nvrhi::ShaderType::All)` | All 11 entries visible at expected lines; full chain reads: `Builder.SetVisibility(nvrhi::ShaderType::All).AddConstantBuffer(0).AddConstantBuffer(1).AddRayTracingAccelStruct(0).AddTextureSRV(1).AddTextureSRV(2).AddTextureSRV(3).AddStructuredBufferSRV(5).AddStructuredBufferSRV(6).AddStructuredBufferSRV(7).AddStructuredBufferSRV(8).AddSampler(2);` | **PASS** |
| A2 | v22 UAV-only binding layout at FGIPass.cpp:301-316 | `read_file` window 298-317 | `nvrhi::BindingLayoutDesc UAVLayoutDesc;` + `nvrhi::BindingLayoutItem UAVItems[2];` + `UAVItems[0].slot=0; [0].type=Texture_UAV; [0].size=1;` + `[1]` slot-1 entries + `UAVLayoutDesc.bindings.assign(...)` + `Device->createBindingLayout(UAVLayoutDesc);` + error log | All 6 expected entries visible at expected lines; UAVBindingLayout creation chain intact | **PASS** |

Both probes confirm the v22 SRV+UAV binding-layout split (the fix that addresses the nvrhi-deferred-barrier-ordering anti-pattern documented in `gpu-rendering-bisect-debug/references/nvrhi-deferred-barrier-ordering.md`) is intact on disk at the v22-introduced lines.

## Part B — terminal-required verification (BLOCKED this tick)

| Test | Subject | Required action | Status |
|------|---------|-----------------|--------|
| B1 | `bash fresh-evidence-scan.sh` | terminal `bash` of read-only triage script | **UNVERIFIED** — terminal blocked by tirith (`pending_approval: tirith:unknown`) |
| B2 | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` | terminal build | **UNVERIFIED** — terminal blocked |
| B3 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | terminal run with frame dumps | **UNVERIFIED** — terminal blocked |
| B4 | `python3 validate_restir_gi.py` | python script execution | **UNVERIFIED** — terminal blocked |
| B5 | Inspect display_frame8.png | vision-analyze the freshly-dumped display PNG | **UNVERIFIED** — no vision tool available in cron runspace |
| B6 | Inspect `dump_pixelstats.py` output on gi_raw + gbuffer_worldpos | python script execution | **UNVERIFIED** — terminal blocked |
| B7 | Inspect stderr.log for VUID-00344 + command-list warnings | grep `rgi_evidence.txt` | **UNVERIFIED** — terminal blocked |
| B8 | Inspect git status / on-disk source tree state | terminal `git status`, `stat` | **UNVERIFIED** — terminal blocked |

## Part C — out-of-band evidence (cumulative carry-forward from prior ticks)

- Cumulative 22-patch inventory was last verified end-to-end at v83 cross-tick spot-check; v85 re-verifies 2 NEW sites (A1+A2 above) without disturbing the other 22.
- Newest dumps directory stamp still `20260727_000706`-`000708`; the parent has not rerun the test since that timestamp.
- Newest log still `TestReSTIR_GI_Temporal.log:76` with `gi_raw R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]` — the user's "broken visual" symptom. This matches the prior v25-v84 cumulative state.
- v85 cumulative patch count: 22 (unchanged — v85 is documentation-only).

## Verdict
Part A 2/2 PASS, Part B 8/8 UNVERIFIED (terminal-blocked). v85 is the cron-RESUMED tick; ready for testing-verifier.
