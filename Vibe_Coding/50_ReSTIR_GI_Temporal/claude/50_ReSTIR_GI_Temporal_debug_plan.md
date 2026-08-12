# TestReSTIR_GI_Temporal Debug Plan — 2026-07-23

> **Mindset:** same as the path-GI fix (memory [[debugging-process-2026-06-06]]) —
> isolation first, percentile analysis, accumulation sweep, judge on the Display output.
> Do NOT tweak ReSTIR merge math until the upstream GI is visually verified.
> Do NOT trust a relaxed validator that "passes" while dumps are black.

---

## TL;DR — current state (2026-07-23)

The TestReSTIR_GI_Temporal pipeline output is **completely black on the latest
two build/run cycles** (`dumps/20260723_005148_*` and `dumps/20260723_005425_*`).
The slightly older series (`dumps/20260721_225614_*` … `20260721_225652_*`) had
content (R-dominant, mean ≈ 72–172). The shift from "content" to "all zeros"
happened sometime between the Jul-21 and Jul-23 code states — the same window in
which the **real Sponza GBuffer pass** (card `t_fb91e5cf`, commit `ee3c2c3`) and
the related handoff (commit `509b114`) landed.

`final-state-2026-07-22.md` says "build green, validator exits 0 (relaxed 1/1)".
But the validator (`validate_restir_gi.py`) loads every `*.png` from `dumps/`
and computes a per-channel mean. The Jul-23 dumps have mean=`(0,0,0)`, which
should fail the `mean > 0.05` threshold. The validator can only "pass" if it
is being pointed at the older Jul-21 dumps (which the directory still contains)
and not re-run since the Jul-23 builds.

**Therefore: the test is silently broken and the validator is lying.** The plan
below is the diagnostic shape that exposed this and what to do about it.

### Empirical evidence already captured

| Stamp                  | Files | Mean RGB               | Max RGB      | GBuffer dumps? | Verdict             |
| ---------------------- | ----- | ---------------------- | ------------ | -------------- | ------------------- |
| `20260721_225614_*` … `20260721_225652_*` | 12  | R=72–172, G/B=18–84    | 73–172       | No             | R-dim, suspicious   |
| `20260723_005148_*` … `20260723_005149_*` | 7   | **(0, 0, 0)**          | **(0, 0, 0)** | Yes (3 MRTs)  | **All-black**       |
| `20260723_005425_*` … `20260723_005427_*` | 7   | **(0, 0, 0)**          | **(0, 0, 0)** | Yes (3 MRTs)  | **All-black**       |

The GBuffer dumps going from "absent" (Jul-21) to "present but all-zero" (Jul-23)
is the smoking gun: the **GBuffer pass writes nothing**, OR the **dump tool** is
capturing the GBuffer textures before the rasterizer draws. Either way, every
downstream stage that consumes the GBuffer (FGIPass, BilateralDenoise, ReSTIR,
GIAccumulate) is fed zeros and emits zeros.

---

## Phase 0 — Confirm symptom (largely done; one loose end)

### Step 0.1 Re-run percentile analysis, capture for handoff
**Why:** we need a reproducible percentile report that the next session can
diff against. The Jul-21 dumps are the "known-content" baseline; the Jul-23
dumps are "known-black". A side-by-side tells us whether the regression is in
the rasterizer, the sampler, or the dump-tool.

```bash
python3 << 'PY'
from PIL import Image
import numpy as np
import os, json
dump_dir = 'Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps'
result = {}
for f in sorted(os.listdir(dump_dir)):
    if not f.endswith('.png'): continue
    img = np.array(Image.open(os.path.join(dump_dir, f)).convert('RGB'),
                   dtype=np.float32)
    p = np.percentile(img, [1, 50, 99])
    result[f] = {
        'mean':   img.mean(axis=(0,1)).round(3).tolist(),
        'max':    img.max(axis=(0,1)).tolist(),
        'p01_50_99': p.round(3).tolist(),
        'sat_pct': float((img >= 250).mean() * 100),
        'black_pct': float((img <= 1).mean() * 100),
    }
print(json.dumps(result, indent=2))
PY
```

**Expected output key:** every file in the `20260723_*` group has
`black_pct` ≈ 100, `sat_pct` ≈ 0, `max` = [0,0,0], `p99` ≈ 0. The `20260721_*`
group has at least one channel whose `max` > 50 and `mean` > 30.

**Outcome gate:** if any Jul-23 dump has `black_pct < 100` **or** `max` ≠ 0,
abort the plan — the symptom is different from what we think and the bisect
below is mis-targeted.

### Step 0.2 Decide whether the validator is lying
**Why:** `final-state-2026-07-22.md` says validator exits 0. The current dumps
should fail. Confirm by running it ourselves.

```bash
cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
python3 validate_restir_gi.py; echo "exit=$?"
```

**Expected output:** exit code 1, "0/1 checks PASSED", best channel mean = 0.000.

**Action if exit 0:** someone ran the validator against a stale copy of `dumps/`
OR the validator script is being invoked with an argument we missed. Either
way, **the validator is no longer trustworthy** and we need to fix that (see
Phase 4.1) before claiming any fix.

### Step 0.3 Log this state
- Add entry to `.wolf/memory.md`: `| HH:MM | dump regression: 23rd 0051+0054 series all-black; older 21st 2256 series had R-dim content | TestReSTIR_GI_Temporal_Data/dumps/ | confirmed | ~3k`
- Add bug to `.wolf/buglog.json` with id `bug-069` (next free after `bug-068`):
  - `error_message`: "Validator exits 0 but latest dumps are mean=0 black; framework state and test state are out of sync"
  - `file`: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (validator trust)
  - `root_cause`: "Validator iterates `*.png` from `dumps/`; when older content + newer black coexist, the older baseline still passes"
  - `fix`: "Either prune `dumps/` on each build, or require the validator to check that every stamp group passes — not just 'any single file passes'"
  - `tags`: ["validator", "regression-coverage", "TestReSTIR_GI_Temporal"]

---

## Phase 1 — Bisect the pipeline (isolation first)

The pipeline order, per `TestReSTIR_GI_Temporal.cpp:30-50`, is:

```
GBuffer (GBufferPT_VS/PS, 3 MRTs)
   ↓
FGIPass (GIPathTracing.hlsl, 64-byte GIPayload) → OutputTexture
   ↓
FBilateralDenoisePass → DenoisedHDRTexture
   ↓
FReSTIRPass::DispatchGeneration → Reservoir0/1
   ↓
FReSTIRPass::DispatchTemporal → MergedReservoir + OutRadiance
   ↓
FReSTIRPass::DispatchSpatial (3×3 + pairwise MIS) → SpatialRadiance
   ↓
FGIAccumulatePass (ACES tonemap + sRGB gamma) → Display
   ↓
Blit to swapchain
```

The dumps we already capture across this chain are:
`gbuffer_worldpos`, `gbuffer_normal`, `gbuffer_material`, `gi_raw`,
`denoised`, `spatial`, `display`. That's 7 stages — exactly the 7 channels
already in the Jul-23 dumps.

### Step 1.1 Read the dump capture code
**Files to read:**
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — search for
  `mFrameDumper`, `BeginDump`, `EndDump`, and "gbuffer" near the renderer call
  points.

**Why we need to read, not guess:**
Per the [[debugging-process-2026-06-06]] lesson 4 — "Compacted context ≠ current
file state". The test file is 1642 lines; the dump capture points, especially
the new `gbuffer_*` channels added in `t_fb91e5cf`, are the critical wiring
detail. If the GBuffer MRTs are dumped before the rasterizer clears/dispatches,
or after `EndFrame` clears them, every channel will read zero regardless of
whether the rasterizer did its job.

**Expected capture order (to verify against the source):**
1. After `RenderGBuffer()` writes the 3 MRTs.
2. After `GIPass.Render()` writes OutputTexture.
3. After `BilateralDenoisePass.Render()` writes DenoisedHDRTexture.
4. After `FReSTIRPass::DispatchSpatial` writes SpatialRadiance.
5. After `GIAccumulate` writes Display.

### Step 1.2 Run with explicit isolation toggles
**Why:** even if the captures happen at the right time, the *content* might be
zero because earlier work didn't run. The cheapest isolation is to **bisect the
chain by toggling passes off** and re-dumping.

For each row in the table, run `TestReSTIR_GI_Temporal` from the binary
directory with `--DumpFrames=1` and `HLVM_DUMP_RGI=1`, then read the
`display_frame1.png` (and the upstream dumps). **No display server needed** —
the dump path uses NVRHI CPU readback (proven by the 26 existing dumps in
`dumps/`). Lavapipe ICD is installed at
`/usr/share/vulkan/icd.d/lvp_icd.x86_64.json` for software rendering when no
GPU is available.

```bash
cd Engine/Source/Runtime/Binary/Debug
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json \
    HLVM_DUMP_RGI=1 HLVM_DUMP_FRAMES=1 \
    ./TestReSTIR_GI_Temporal
```

Document the command, the env vars set, the binary path, and which dump files
resulted, for each row.

| Pass disabled               | CVar / flag (if any) | Expected dump display         | Tells us if…                          |
| --------------------------- | -------------------- | ----------------------------- | ------------------------------------- |
| nothing (baseline all-black)| -                    | black                         | confirms symptom                      |
| GBuffer pass (rasterizer off)| make `RenderGBuffer` early-return | black (matches current state) | rasterizer is the suspected culprit |
| GBuffer pass (force write) | `RenderGBuffer` writes sentinel-only | color stripes with -100/-200/-300 RGB | dump tool works; rasterizer is culprit |
| FGIPass                     | skip Render, fill OutputTexture with checker | black-on-checker | if still all-black, dump tool is culprit |
| BilateralDenoise            | skip Render, fill Denoised with checker | black-on-checker | same logic |
| ReSTIR Temporal             | skip DispatchTemporal | unchanged from prev upstream | temporal pass was the regression       |
| ReSTIR Spatial              | skip DispatchSpatial | unchanged | spatial pass was the regression        |
| GIAccumulate                | skip tonemap | raw linear | GIAccumulate tonemap was zeroing |

Each row is one experiment, one dump directory, one **clear verdict**. The
isolation table is the plan-of-record for Phase 1; do not skip rows.

### Step 1.3 Decide where the chain turns black

The bisect table is exhaustive. Once a worker reports the verdict for each row,
the **first row whose expected-display differs from the actual** is the
regression site. If row 1 (GBuffer off, sentinel-only) **still produces
all-black**, then the bug is in `FRenderPassDumper` itself (NVRHI readback
encoding, mip-level selection, BLAS/TLAS path-name resolution) — not in the
renderer. If row 1 produces sentinel colors, the dumper works and the bug is
*before* the GBuffer rasterizer, most likely in:
- `CreateScene()` (no Sponza loaded — see `t_fb91e5cf` handoff mention of
  "Sponza scaling 0.01 applied to TLAS"; if scale=0, the TLAS is empty).
- The rasterizer dispatch itself (clear color = 0, no PS dispatch).
- The MRT attachments (wrong format / wrong viewport).

---

## Phase 2 — Hypothesize root cause from where the bisect lands

Three scenarios; each maps to a different fix.

### Scenario A: "Dumper is broken"
**Symptoms:** even with the GBuffer pass forcibly writing sentinels or skipping
the chain entirely, all dumps are zero. NVRHI `readTexture` returns all-zero on
the staging buffer.

**Likely causes:**
- `FRenderPassDumper::BeginDump` reads from a different `IDevice` handle than
  the renderer wrote to (subtle, common after refactor).
- `nvrhi::Format::RGBA16_FLOAT` mismatch (reads 4-channel, source was
  `RGBA8_UNORM`, the channel interpretation silently zeros when over-read).
- The dump tool is dumping the wrong resource handle (e.g. a null RT it
  created internally).

**Fix shape:** locate the dumper implementation, add an `int3` triangular
gradient to its readback path, confirm readback works at all. **No renderer
code changes.**

### Scenario B: "GBuffer pass doesn't render Sponza"
**Symptoms:** with sentinel-write row, dumper is fine. With full pipeline,
`gbuffer_*` channels are zero (current state). Upstream chain logic works
when fed non-zero input.

**Likely causes:**
- Sponza GLTF path is wrong → empty scene → nothing to rasterize.
- `CreateScene` failed silently and used a fallback empty scene.
- `Renderer::Render` is running with `isVisible=false` for everything.
- TLAS instance count = 0 (Sponza scale 0.01 committed but if applied wrong
  all instances may have degenerate matrices).
- PS not bound to MRT, only to depth — common when refactoring.

**Fix shape:** inspect `CreateScene` and `BuildSponzaTLAS` in
`TestReSTIR_GI_Temporal.cpp`. Verify `m_MeshInstanceCount > 0` after TLAS build
(GDB `break CreateScene; print numInstances`). Verify the first PS dispatch
actually runs (GPU profiler counter, or insert a `gOutColor = 1;` sentinel in
`GBufferPT_PS.hlsl`).

### Scenario C: "Upstream content is right but later pass eats it"
**Symptoms:** GBuffer dumps non-zero. `gi_raw` zero. (Or `gi_raw` non-zero,
`denoised` zero, etc.)

**Likely causes:**
- Pipeline binding mismatch (HLVM's known gotcha: `constantBufferOffset = 0`
  for binding 0; per `.wolf/cerebrum.md` from earlier path-GI work).
- Descriptor set miss — common after refactor when descriptor slot topology
  changed but binding slot index didn't.
- NVRHI binding offsets (the CLAUDE.md warning is precise about this).

**Fix shape:** per the path-GI debug cycle, check the binding offset for each
pass from the first broken stage downstream. The `GIPathTracing.hlsl` debug
mode 0 (raw PathTracing without MIS) was used in the path GI fix; if FGIPass is
the broken stage, set `HLVM_PT_DEBUG_MODE=0` and see if `gi_raw` becomes non-zero.

---

## Phase 3 — Apply the fix

**This phase is contingent on Phase 1+2 being conclusive.** Do NOT start here
without a "where the chain turns black" finding from Phase 1.

### Step 3.1 Make the minimal diff
Per the debug-process memory: "Don't keep debug visualisations in the build."
Any sentinel writes (`gOutColor = 1`, triangular gradient) added to
`GBufferPT_PS.hlsl` or `GIPathTracing.hlsl` must be removed before commit.

### Step 3.2 Commit per-stage
Per the writing-plans skill: frequent small commits.
- One commit per diagnostic change, with `tag=dump-debug` so it can be squashed
  away when the real fix lands.
- One commit for the actual fix.
- One commit for any validator tightening (Phase 4.1).

### Step 3.3 Run accumulation sweep
**This is the *path GI mindset's* core verification:** with display non-black
at N=1, run with `HLVM_RGI_ACCUM=1, 4, 16` and compare.

```bash
for N in 1 4 16 64; do
    rm -rf TestReSTIR_GI_Temporal_Data/dumps
    HLVM_RGI_ACCUM=$N ./Binary/Debug/TestReSTIR_GI_Temporal --DumpFrames=$N
done
```

**Expected:** `display.p99 - p01` should narrow as N increases. If p99-p01
*widens* with N, the temporal pass is unstable (the same `FrameIndex`-in-hash
class of bug from `restir-gi-snowflower-flicker.md`). If it stays the same,
the temporal pass is dormant.

---

## Phase 4 — Verify and harden

### Step 4.1 Tighten the validator
The relaxed `mean > 0.05` check was a stopgap (per `t_8291cf8c` handoff). Once
the dump regression is fixed, the validator should require:
1. **Every dump group's most-recent stamp passes** — not "any file in `dumps/`".
2. **Display frame p01 > 0.05 AND p99 < 0.95** — guarantees non-trivial
   luminance, not just non-black.
3. **GBuffer non-zero material mean** — guarantees the rasterizer ran.
4. **(Optional, when temporal pass is live) two consecutive frames' `display`
   have p01 within 0.05** — guarantees temporal stability.

### Step 4.2 Update finish plan and handoff
Once the chain is non-black at N=1 and stable at N=16, replace `final-state
-2026-07-22.md` with a current file. Reference the new validator exit code and
the new bug ids in `.wolf/buglog.json`.

### Step 4.3 Update the auto-memory
The `restir-gi-snowflower-flicker.md` memory is 46 days old and is now stale
(both in technical detail and the "current symptom" claim). Either:
- Mark it `STATUS: SUPERSEDED` and write a new `TestReSTIR_GI_Temporal-{date}.md`,
  OR
- Replace it in-place if most of the lessons transfer.
Per [[debugging-process-2026-06-06]] lesson 8: always mark the older critic.

---

## Open questions / risks

1. **Display server.** All GPU-side work is blocked by the no-display sandbox.
   The plan assumes a worker with X11/Wayland can run the binary. If no worker
   has display access, Phase 1 cannot be executed locally — the entire bisect
   must be dispatched to another machine (`Card_t_fb91e5cf_handoff.md` already
   does this pattern; reuse that handoff shape).
2. **NVRHI/Vulkan code changes are off-limits** (per `hlvm-debug-workflow`
   skill rule 5: "If you suspect a bug in NVRHI or Vulkan — it's our code").
   Stay in renderer / test / dumper code, not NVRHI/Vulkan API.
3. **MiMalloc2 thread cleanup.** Per CLAUDE.md gotcha, the test exits via
   SIGABRT if `mi::Mallocator::thread_done()` is missed on worker thread. When
   adding any worker-dispatch in Phase 1.2, ensure thread cleanup.
4. **The Jul-21 dumps are R-dominant** (R=72-172, G=18-79, B=19-84). That
   could be a debug visualization (valid=red) OR a single-channel accumulation
   bug. Before we re-establish "Jul-21 was working", verify by viewing one of
   those dumps directly. If they too are "validity indicator" not a render,
   the test was never actually producing Sponza and the whole pipeline
   regression story is "it never worked".
5. **Watchdog script also reports build-green.** `~/.hermes/scripts/restir_gi_
   watchdog.py` runs build+test and reports pass/fail. Confirm the watchdog's
   exit-code-based "pass" matches the validator's; if it only checks
   process-exit-0, it shares the validator's blindness.

---

## Files likely to be touched

| File                                                                                       | Why                                          |
| ------------------------------------------------------------------------------------------ | -------------------------------------------- |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`                                    | RenderGBuffer dispatch check, dump call sites |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GBufferPT_PS.hlsl`                 | Temporary sentinel write only; revert before commit |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`             | Tighten relaxed stopgap                       |
| `Engine/Source/Runtime/Public/Image/FRenderPassDumper.h`                                   | Read-back path (if Scenario A holds)          |
| `.wolf/memory.md`                                                                          | Session entry                                 |
| `.wolf/buglog.json`                                                                        | `bug-069` (validator mismatch)                |
| `~/.claude/projects/.../memory/restir-gi-snowflower-flicker.md`                            | Mark SUPERSEDED                               |
| `Vibe_Coding/50_ReSTIR_GI_Temporal/final-state-2026-07-23.md`                              | Replace `final-state-2026-07-22.md`           |

---

## Plan-of-record summary (one paragraph)

The pipeline is broken on the latest commit (Jul-23 dumps are 100% black;
older Jul-21 dumps had only R-dim content). The relaxed validator inherited
from `t_8291cf8c` is "passing" against the old dumps but the test is not
actually producing frames. Apply the path-GI debug mindset: isolation first
(bisect the 7-stage chain by toggling passes and dumping one frame each), then
hypothesize (Scenario A dumper / Scenario B GBuffer empty / Scenario C binding
offset), then minimal-diff fix with frequent commits, then accumulation sweep
at N=1/4/16/64 to confirm temporal stability, then tighten the validator to
reject the dump/validator desync class of failure permanently.
