#!/usr/bin/env python3
"""Regression tests for newest-run selection in validate_restir_gi.py.

v234: rewritten against the current API. The pre-v234 tests imported
`select_newest_dump_group`, a function the validator no longer has (the
group logic is now find_dump_groups / find_frame_in_group /
find_group_for_frame), so the whole file failed at import time. The
semantics under test are unchanged: a dump run's files straddle multiple
wall-clock seconds, and validation must resolve every texture of the
NEWEST run's newest frame, never a stale older run's file.
"""

import tempfile
import unittest
from pathlib import Path

from validate_restir_gi import (
    find_dump_groups,
    find_frame_in_group,
    find_group_for_frame,
    find_dump_file,
)

try:
    import numpy as np
    from validate_restir_gi import check_scene_content
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False


def _touch_files(root: Path, names) -> None:
    for name in names:
        (root / name).touch()


class NewestDumpGroupTests(unittest.TestCase):
    def test_groups_sorted_newest_last(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _touch_files(root, [
                "20260803_083951_display_frame8.png",
                "20260803_084039_gi_raw_frame8.png",
                "20260803_084041_gbuffer_material_frame8.png",
            ])
            self.assertEqual(
                find_dump_groups(root),
                ["20260803_083951", "20260803_084039", "20260803_084041"],
            )

    def test_selects_latest_run_when_each_run_spans_multiple_seconds(self):
        # The newest run's files land in 084038 and 084041; the newest
        # timestamp group is 084041 and its frame number anchors the run.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _touch_files(root, [
                "20260803_083951_denoised_frame8.png",
                "20260803_083951_display_frame8.png",
                "20260803_083952_gi_raw_frame8.png",
                "20260803_083954_gbuffer_material_frame8.png",
                "20260803_084038_denoised_frame16.png",
                "20260803_084038_display_frame16.png",
                "20260803_084038_gi_raw_frame16.png",
                "20260803_084041_gbuffer_material_frame16.png",
            ])
            groups = find_dump_groups(root)
            newest_ts = groups[-1]
            self.assertEqual(newest_ts, "20260803_084041")
            frame = find_frame_in_group(root, newest_ts)
            self.assertEqual(frame, 16)
            # Every texture of frame 16 resolves, from ITS group even when
            # the group is not the newest one (display lives in 084038).
            for name, ts in [("display", "20260803_084038"),
                             ("denoised", "20260803_084038"),
                             ("gi_raw", "20260803_084038"),
                             ("gbuffer_material", "20260803_084041")]:
                self.assertEqual(find_group_for_frame(root, frame, name), ts)

    def test_fresh_run_wins_over_older_higher_frame(self):
        # An old run reached frame 32; a fresh run only reached frame 16.
        # Validation must still pick the fresh run's frame 16.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _touch_files(root, [
                "20260803_083951_display_frame32.png",
                "20260803_083951_gi_raw_frame32.png",
                "20260803_084038_display_frame16.png",
                "20260803_084038_gi_raw_frame16.png",
            ])
            groups = find_dump_groups(root)
            newest_ts = groups[-1]
            frame = find_frame_in_group(root, newest_ts)
            self.assertEqual(frame, 16)
            picked = find_dump_file(root, find_group_for_frame(root, frame, "gi_raw"), "gi_raw")
            self.assertEqual(picked.name, "20260803_084038_gi_raw_frame16.png")

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


@unittest.skipUnless(HAS_NUMPY, "numpy not available")
class SceneContentGateTests(unittest.TestCase):
    """v235: the anti-'white wall' gate (FAIL_LOG_2026-08-26.md).

    Between 2026-08-10 and 2026-08-26 every validator gate passed on a
    featureless exterior-wall framing. These tests pin the gate that catches
    it: >=1% of pixels with HSV saturation > 0.15.
    """

    def _rgba(self, rgb):
        """(H, W, 3) float array -> (H, W, 4) with alpha=1."""
        h, w = rgb.shape[:2]
        out = np.ones((h, w, 4), dtype=np.float32)
        out[..., :3] = rgb
        return out

    def test_smooth_gray_gradient_fails(self):
        # The exact failure mode: a smooth gray gradient has variance (passes
        # gates 2/4) but zero saturation.
        grad = np.linspace(0.3, 0.7, 64, dtype=np.float32)
        rgb = np.broadcast_to(grad[None, :, None], (64, 64, 3)).copy()
        ok, frac = check_scene_content(self._rgba(rgb))
        self.assertFalse(ok)
        self.assertAlmostEqual(frac, 0.0, places=6)

    def test_saturated_scene_passes(self):
        # Gray scene with a 16x16 red patch (6% of pixels) — like Sponza's
        # red carpet or Cornell's walls.
        rgb = np.full((64, 64, 3), 0.5, dtype=np.float32)
        rgb[:16, :16] = (0.8, 0.1, 0.1)
        ok, frac = check_scene_content(self._rgba(rgb))
        self.assertTrue(ok)
        self.assertGreater(frac, 0.01)

    def test_tiny_saturated_speck_fails(self):
        # A lone saturated pixel (firefly) must not satisfy the gate.
        rgb = np.full((64, 64, 3), 0.5, dtype=np.float32)
        rgb[0, 0] = (1.0, 0.0, 0.0)
        ok, frac = check_scene_content(self._rgba(rgb))
        self.assertFalse(ok)
        self.assertLess(frac, 0.01)

    def test_wall_era_evidence_fails_and_real_content_passes(self):
        # Regression test on the actual incident images (skipped when the
        # evidence checkout is absent).
        evidence = Path(__file__).resolve().parents[5] / \
            "Vibe_Coding/50_ReSTIR_GI_Temporal/evidence/v215_cornell"
        wall = evidence / "sponza_default_display.png"
        cornell = evidence / "cornell_restir_display.png"
        if not wall.is_file() or not cornell.is_file():
            self.skipTest("v215_cornell evidence not present")
        from validate_restir_gi import _load_png_as_float
        ok_wall, frac_wall = check_scene_content(_load_png_as_float(wall))
        ok_cornell, frac_cornell = check_scene_content(_load_png_as_float(cornell))
        self.assertFalse(ok_wall, f"wall framing must fail (frac={frac_wall})")
        self.assertTrue(ok_cornell, f"Cornell must pass (frac={frac_cornell})")


if __name__ == "__main__":
    unittest.main()
