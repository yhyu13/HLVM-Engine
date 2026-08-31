# Pending Tests v144
- commit: docs/PENDING_COMMIT_v144.md
- tester: tester (single-profile self-check)
- timestamp: 2026-08-05

## Tests written

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/test_validate_restir_gi.py::test_nvrhi_validation_archive_is_forced_into_runtime_link`: module-direct/read-only source-contract test. It checks that the PyCMake source and generated Runtime CMake both contain `$<LINK_LIBRARY:WHOLE_ARCHIVE,nvrhi>`, retain `nvrhi_vk` ordering, and no longer emit the old plain `nvrhi_vk nvrhi` dependency.
- Existing five v143 selector/lifecycle tests remain in the same file and are intentionally retained.
- Behavioral test recipe from `PENDING_COMMIT_v144.md`: Debug rebuild, default dump run with `HLVM_RGI_ACCUM=8`, mode-20 dump run, newest-group validator, fresh log scan, numpy statistics, and visual inspection.

## Coverage summary

- Module-direct/source-contract: 6 Python behaviors (5 retained v143 cases + 1 v144 linkage case)
- TestClient-layer: 0 (not applicable)
- Router-wiring: 0 (not applicable)
- GPU integration: 7 acceptance checks specified; execution remains externally blocked

## Static cases audited

1. The linkage test reads the generator source at `Runtime/Runtime_cmake.py`, not a guessed path.
2. It reads the generated `Runtime/CMakeLists.txt` and verifies the current working tree is directly buildable before regeneration.
3. It checks the exact CMake 3.29.3 compiler-cache evidence used to justify the `LINK_LIBRARY` feature.
4. It rejects the old plain-link strings, preventing a test that passes while the original linkage contract is restored.
5. It does not mutate files, invoke subprocesses, depend on environment state, or inspect stale runtime artifacts as if they were fresh.

## TDD red-phase notes

- Before v144, the new linkage assertion would fail because both source/generated files contained only the plain `nvrhi_vk nvrhi` form; this pins the build-link hypothesis rather than an imagined API.
- If either CMake file regresses to the old dependency, the test fails before a costly GPU run.
- The test cannot prove that the linker extracts the symbol; the real Debug rebuild remains a mandatory behavioral check and is explicitly not claimed in this marker.

## Testability gaps

Terminal, CMake regeneration, linker execution, GPU execution, numpy analysis, and vision are unavailable in this scheduled runspace. The marker records the exact parent-runspace recipe rather than fabricating a pass. No test file under a `tests/` directory was added; the existing project test-data module is the appropriate discovery surface.
