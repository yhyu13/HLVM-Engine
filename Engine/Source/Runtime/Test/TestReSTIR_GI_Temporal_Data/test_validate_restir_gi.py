#!/usr/bin/env python3
"""Regression tests for newest-run selection in validate_restir_gi.py."""

import unittest
from pathlib import Path

from validate_restir_gi import select_newest_dump_group


class NewestDumpGroupTests(unittest.TestCase):
    def test_returns_all_files_when_no_display_anchor_exists(self):
        files = [
            "/dumps/20260803_084039_gi_raw_frame8.png",
            "/dumps/20260803_084041_gbuffer_material_frame8.png",
        ]

        self.assertEqual(select_newest_dump_group(files), files)

    def test_selects_latest_run_when_each_run_spans_multiple_seconds(self):
        files = [
            "/dumps/20260803_083951_denoised_frame8.png",
            "/dumps/20260803_083951_display_frame8.png",
            "/dumps/20260803_083952_gi_raw_frame8.png",
            "/dumps/20260803_083954_gbuffer_material_frame8.png",
            # Same-second denoised sorts before display; both must survive.
            "/dumps/20260803_084038_denoised_frame8.png",
            "/dumps/20260803_084038_display_frame8.png",
            "/dumps/20260803_084038_gi_raw_frame8.png",
            "/dumps/20260803_084041_gbuffer_material_frame8.png",
        ]

        self.assertEqual(
            select_newest_dump_group(files),
            files[4:],
        )

    def test_excludes_partial_stale_files_before_latest_display(self):
        files = [
            "/dumps/20260803_084030_res_tmp0_frame8.png",
            "/dumps/20260803_084031_gbuffer_worldpos_frame8.png",
            "/dumps/20260803_084038_display_frame8.png",
            "/dumps/20260803_084038_denoised_frame8.png",
            "/dumps/20260803_084041_gbuffer_depth_frame8.png",
        ]

        self.assertEqual(
            select_newest_dump_group(files),
            files[2:],
        )

    def test_current_dump_directory_matches_latest_display_timestamp(self):
        dump_dir = Path(__file__).with_name("dumps")
        files = sorted(str(path) for path in dump_dir.glob("*frame8.png"))
        if not files:
            self.skipTest("no rendered dumps are present")

        selected = select_newest_dump_group(files)
        display_files = [path for path in files if "display_frame8.png" in path]
        if not display_files:
            self.skipTest("rendered dumps contain no display frame")
        latest_display_timestamp = Path(display_files[-1]).name[:15]

        self.assertTrue(selected)
        self.assertTrue(all(Path(path).name[:15] >= latest_display_timestamp for path in selected))
        self.assertTrue(any("display_frame8.png" in path for path in selected))
        self.assertTrue(any("gi_raw_frame8.png" in path for path in selected))

    def test_device_validation_is_configured_before_creation(self):
        source_path = Path(__file__).resolve().parent.parent / "TestReSTIR_GI_Temporal.cpp"
        source = source_path.read_text(encoding="utf-8")
        create_pos = source.index("CreateWindowDeviceAndSwapChain(WindowProps)")

        self.assertLess(source.index("DeviceParams.bEnableDebugRuntime = true;"), create_pos)
        self.assertLess(source.index("DeviceParams.bEnableNVRHIValidationLayer = true;"), create_pos)
        self.assertLess(source.index("DeviceParams.bEnableRayTracingExtensions = true;"), create_pos)
        self.assertEqual(source.count("DeviceParams.bEnableDebugRuntime = true;"), 1)

    def test_nvrhi_validation_archive_is_forced_into_runtime_link(self):
        runtime_source = Path(__file__).resolve().parents[2] / "Runtime_cmake.py"
        generated_cmake = Path(__file__).resolve().parents[2] / "CMakeLists.txt"
        runtime_text = runtime_source.read_text(encoding="utf-8")
        generated_text = generated_cmake.read_text(encoding="utf-8")
        whole_archive = "$<LINK_LIBRARY:WHOLE_ARCHIVE,nvrhi>"

        self.assertIn(whole_archive, runtime_text)
        self.assertIn(whole_archive, generated_text)
        self.assertIn("nvrhi_vk", runtime_text)
        self.assertIn("nvrhi_vk", generated_text)
        self.assertNotIn("values=['nvrhi_vk', 'nvrhi']", runtime_text)
        self.assertNotIn("target_link_libraries(Runtime PUBLIC nvrhi_vk nvrhi)", generated_text)

        compiler_cache = Path(__file__).resolve().parents[2] / "Build" / "Debug" / "CMakeFiles" / "3.29.3" / "CMakeCXXCompiler.cmake"
        self.assertIn('CMAKE_CXX_COMPILER_VERSION "17.0.6"', compiler_cache.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
