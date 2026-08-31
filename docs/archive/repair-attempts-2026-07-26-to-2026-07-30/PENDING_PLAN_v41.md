# Pending Plan v41 — fix FImageDump::DumpToPNG to preserve source alpha channel

## State-machine routing decision
- Read every `docs/PENDING_*.md` marker. v40 cycle is complete at audit `ALL_KEEP`.
- Rule 9 (audit exists → next item from PICK) fires. Topmost unchecked item in `PENDING_PICK.md` was the v41 staging line ("next mechanically actionable file-only fix").
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 15+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset remains file-only.
- Cron's prompt explicitly authorizes: "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop."

## NEW FINDING (discovered during v41 read-only static audit)

**`FImageDump::DumpToPNG` hardcodes alpha = 255 for every pixel**, regardless of the source `rgbaData[3]` value.

```cpp
// Engine/Source/Runtime/Private/Image/FImageDump.cpp:19
pixels[idx + 3] = 255;   // <-- hardcoded, NEVER reads rgbaData[i*4+3]
```

### Why this invalidates the v28 alpha sentinel diagnostic surface

The v28 patch added to `GIPathTracing.hlsl` (line 694, both HLSL copies):
```hlsl
Output[pixel].w = max(Output[pixel].w, 0.99994f);
```

The intent: regardless of debugMode (default 0 or any of 1u-15u), the alpha channel of the OutputTexture UAV write is biased toward 0.99994 (~254/255 in RGBA8 unorm). This produces a recognizable "alpha saturated" pattern in the dumped PNG that disambiguates:
- **saturated alpha** → dispatch body ran (bug is downstream of this line)
- **zero alpha** → dispatch body never ran (bug is upstream)
- **mixed** → partial dispatch (likely barrier issue)
- **low** → pre-v28 binary (parent hasn't rebuilt)

But `FImageDump::DumpToPNG` discards the source alpha entirely and always writes 255. The v28 sentinel's diagnostic value is therefore **lost at the dump boundary** before either `validate_restir_gi.py` (v37) or `dump_pixelstats.py` (v40) ever inspects the PNG.

This is **exactly anti-pattern #6** from the `gpu-rendering-bisect-debug` skill ("Dump-encoder normalization bugs that look like data bugs"). The skill warns:
> "A 'pattern' with high std and many unique values is real data with bad visualization; a flat pattern with low std is the sentinel."
> Symptom: pixel-stats show real variation but the dump looks like solid color blocks.

The inverse of that symptom applies here: **alpha stats show saturation (255), but the underlying alpha value is meaningless because the encoder hardcoded it**. The v37/v40 alpha verdicts are UNRELIABLE — every PNG ever produced by `FImageDump::DumpToPNG` reports `alpha=saturated PASS` regardless of whether the v28 sentinel was in the shader, the dispatch body ran, or anything at all.

The v40 audit verdict "ALL_KEEP, 21/21 static tests pass" was technically correct about the *patch shape* (dump_pixelstats.py correctly inspects alpha when present) but did NOT verify the underlying alpha data is meaningful. The static test at A17 said "v28 alpha-channel sentinel at GIPathTracing.hlsl:694 unchanged. PASS." — but the test never asked "does the alpha channel reach the PNG?" because that requires a build + run + dump + pixel-stats cycle that the cron can't execute.

### Why this isn't surfaced by the v37/v40 tests

Both helpers inspect alpha correctly. The bug is one layer down: **the alpha value being inspected is hardcoded at the encoder, not from the GPU output.** A "PASS" verdict from v37/v40 today means "the encoder wrote 255" — true on every frame of every run, including pre-v28 binaries. Without a fix to the encoder, the alpha diagnostic surface is structurally broken.

### Decision: v41 is the encoder fix

`FImageDump::DumpToPNG` should read `rgbaData[i*4+3]` (the source alpha) and write that, instead of always writing 255. This restores the v28 sentinel's diagnostic value and makes v37's validator alpha-check + v40's dump_pixelstats.py alpha-check actually meaningful on the next parent rebuild.

## Files produced
- `Engine/Source/Runtime/Private/Image/FImageDump.cpp` (modified, +9/-1 lines)
- `Engine/Source/Runtime/Public/Image/FImageDump.h` (unchanged — API signature unchanged)
- `docs/PENDING_PLAN_v41.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v41.md` (new)
- `docs/PENDING_COMMIT_v41.md` (new)
- `docs/PENDING_IMPL_REVIEW_v41.md` (new)
- `docs/PENDING_TESTS_v41.md` (new)
- `docs/PENDING_TEST_AUDIT_v41.md` (new)
- `docs/PENDING_PICK.md` (modified — v40 marked [x], v41 staged with verdict)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v41 tick section)

## skip_plan_review: no
- This is a behavior change to a shared utility (`FImageDump::DumpToPNG` is called from 8+ sites across 4 test files + `FRenderPassDumper.cpp` + the v3 `WriteGBufferSentinels` path). Every PNG dump in the project will produce different alpha values after this patch. The full audit trail must be preserved.

## produces_test_files: no
- Modifying an existing encoder, not creating a test file. HARD INVARIANT #2 does NOT fire.

## skip_impl_review: no
- Behavior change to a shared encoder used by multiple tests. Full audit trail invoked.

## Test strategy
1. **Static tests (this tick, file-only)**:
   - File syntax valid C++ (verified via existing function signature intact)
   - `DumpToPNG` reads `rgbaData[i*4+3]` and writes it (with NaN→255 fallback)
   - Header signature unchanged (no caller updates needed)
   - 0 source-code (C++/HLSL) changes outside FImageDump.cpp
   - All v3-v40 patches still intact
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent runs rebuild + `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`; expects alpha channel in dumped PNGs to reflect source alpha (not all-255).
   - On a v28-or-later binary: expects `display_frame8.png` alpha saturated near 254-255 (v28 sentinel fires); on a pre-v28 binary: expects alpha to reflect `avgFirstHitDist` (the legitimate alpha used by the GI pass — non-saturated values).
   - On TestCornellBoxGI/TestRTReflections: expects alpha values to match the source `rgbaData[3]` (currently 255 always; after v41 may be 255 still if those tests don't use the alpha channel, OR a different value if they do).
   - Cross-validate: `validate_restir_gi.py` v37 alpha-check + `dump_pixelstats.py` v40 alpha-check should produce consistent verdicts on the same dump group.

## Risks
- **Behavior change**: every PNG dump in the project will produce a different alpha value. Mitigation: API signature unchanged so no caller updates; default behavior (RGB visualizations) unchanged; alpha-only consumers (none currently in the project tree) gain meaningful alpha.
- **NaN propagation**: source `rgbaData[3]` could be NaN/inf. Mitigation: clamp the byte result with `std::clamp` (already present for RGB channels); NaN → clamp gives 0 in current C++ semantics; document the behavior.
- **FRenderPassDumper.cpp**: same call site, also gets the fix transitively.
- **Other tests using FImageDump::DumpToPNG** (TestCornellBoxGI, TestPathTraceGI, TestRTReflections, TestRTShadowsGBuffer): all use the encoder for diagnostic visualization only; alpha was always 255 (garbage); they now get whatever source data they pass in.
- **FImageDump::DumpTestPattern**: hardcodes alpha=255 (lines 72); unchanged.
- **Single-head host caveat**: same model writes all 6 roles. Verdicts are self-checks. Patch is mechanical so verdicts are reproducible.

## Decision matrix (post-parent-rebuild, post-v41)
- Parent rebuilds + runs `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`. Two outcomes:
  1. **`display_frame8.png` alpha = saturated ~100%** → v28 sentinel was in the compiled binary AND the dispatch body ran. The v37/v40 alpha verdicts are now meaningful. If the rest of the renderer is correct (validator 3/3 PASS, vision shows Sponza), PIPELINE_GOAL_DONE. If validator still fails, the bug is in lighting/payload/accumulate, not the alpha sentinel.
  2. **`display_frame8.png` alpha = low/zero/mixed** → v28 sentinel either wasn't compiled in, the dispatch didn't run, or the dispatch ran but the GPU-side write didn't land. The v37/v40 alpha verdicts now correctly distinguish these cases and route to v41a/b/c.

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

After v41 lands, the alpha diagnostic surface is reliable for the first time. No `PIPELINE_GOAL_DONE_<date>.md` written.

## Why this is the right next fix (priority argument)

Three reasons:

1. **Correctness**: v37/v40's "PASS" verdicts on alpha are currently meaningless (every dump has alpha=255 by encoder default). v41 fixes the underlying data flow so the alpha verdicts become real signals.
2. **Diagnostic surface completion**: this is the LAST file-only fix that materially advances the renderer's debuggability. After v41, the diagnostic surface has 5 independent signals: v12 cerr, v37 alpha-check, v38 cerr-line, v39 decoder, v40 alpha-stats — all of which now produce meaningful data.
3. **Blast radius**: FImageDump::DumpToPNG is the SINGLE chokepoint for all PNG dumps in the project. Fixing the encoder once fixes all 13+ call sites. No risk of one caller being out-of-sync.

The v40 audit's "ALL_KEEP" verdict was technically correct about the patch shape but missed the encoder's role as a data source. v41 closes the gap that v40 couldn't see without executing the runtime tests that tirith blocks.