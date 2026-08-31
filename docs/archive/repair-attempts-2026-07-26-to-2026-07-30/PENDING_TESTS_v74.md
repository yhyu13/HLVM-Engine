# Pending Tests v74
- commit: docs/PENDING_COMMIT_v74.md
- task: file-only structural-standby tick v74

## Tests

### Part A (file-only, executed this tick)
- **A1**: `read_file Engine/Source/Runtime/Private/Image/FImageDump.cpp:14-28` confirms v41 alpha-encoder fix (`pixels[idx + 3] = std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0, 255)`) intact at line 27, with v41 comment block at lines 19-26 unchanged from v73. PASS.

### Part B (parent-driven; tirith-blocked in cron)
- **B1-B6**: Per PENDING_TESTS_v73 staging; no new tests this cycle (no source change).
