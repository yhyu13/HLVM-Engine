# Pending Tests v62
- plan: docs/PENDING_PLAN_v62.md
- commit: docs/PENDING_COMMIT_v62.md
- tester: tester (single-head autonomous cron — see software-development-practices §"Full auto" anti-pattern caveat)
- timestamp: 2026-07-28T07:15:00Z

## Test strategy
The plan called for the tester to verify (a) README renders sensibly, (b) no source-code change.

## Test files (in working tree)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/README.md` — the patched doc
- (No new test files needed; README is the test surface)

## Run command (for parent session)

### Static (no rebuild needed)
```bash
# Confirm the cumulative patch inventory remains intact
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh

# Confirm the README contains the 21-mode table + Helper scripts section
grep -c "^|" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/README.md     # expect >= 25
grep "Helper scripts" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/README.md   # expect 1 match

# Confirm no source-code (C++/HLSL) change
git diff --stat Engine/Source/Runtime/Private Engine/Source/Runtime/Public Engine/Source/Runtime/Test/*.cpp
# Expect: empty (or only README.md if you include Test/ in the diff set)
```

### Runtime (parent-driven; terminal blocked in cron)
```bash
# These will remain PENDING until parent supplies terminal access
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected runtime behavior:
- Renderer behavior is identical to v61 (README change is doc-only).
- If parent rebuilds AND supplies `rgi_evidence.txt` or alpha-channel evidence, cron's next tick routes to the matching v32/v33/v42/v13a branch.

## Part B / Part A static test table

| # | Probe | Type | Status |
|---|-------|------|--------|
| T1 | README has the "Helper scripts" section | static | PASS (read_file confirms) |
| T2 | README has the 15-row mode table | static | PASS (read_file confirms) |
| T3 | All 4 helper script names appear in README | static | PASS (validate_restir_gi.py / dump_pixelstats.py / decode_v38_evidence.py / fresh-evidence-scan.sh / run_rgi_diagnostic.sh all present in the new section) |
| T4 | Forward-references in README point to docs/PENDING_PLAN_v32/v33/v42/v13a | static | PASS |
| T5 | forward-reference to docs/PIPELINE_HEALTH_2026-07-28.md in README | static | PASS |
| B1 | Build cleanliness | runtime | PENDING (terminal blocked) |
| B2 | Fresh HLVM_DUMP_RGI run produces 7 PNG dumps | runtime | PENDING |
| B3 | No command-list warnings in fresh log | runtime | PENDING |
| B4 | No Vulkan ERROR/VUID in fresh log | runtime | PENDING |
| B5 | Validator 4/4 PASS on newest dump group | runtime | PENDING |
| B6 | Display visibly contains recognizable non-uniform Sponza geometry | runtime | PENDING |

5/5 static T1-T5 PASS; 0/6 runtime B1-B6 PENDING (terminal blocked).

## Risk / caveat
The cron session has tirith blocking all terminal commands. Static checks confirm README accuracy; runtime behavior is identical to v61 (doc-only cycle). If parent chooses to rebuild, the renderer output is expected to be unchanged from v61; if parent changes something, that's a separate event not related to v62.
