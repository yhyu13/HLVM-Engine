# Pending Plan v209

- task: Card R — remove `DummyDirectionTexture`, dead private state left by v207
- source: no bundle — direct edit
- approach: v207 replaced u2's 1x1-dummy fallback with a ternary onto
  `Desc.OutputTexture`, so nothing assigns `FGIPass::DummyDirectionTexture` any
  more. Drop the member declaration (`FGIPass.h:140`) and the `Shutdown()`
  null-out (`FGIPass.cpp:192`). Two lines, two files, zero behaviour change,
  because the field is never written and never read.
- diff_estimate: +0 / -2 functional (plus comment)
- skip_plan_review: no
- test_strategy: file-only verifier — post-patch symbol count must be 0
  tree-wide, with a same-shape positive control on the live sibling
  `DummyDebugStatsTexture` so the zero is not vacuous.
- risks:
  1. **Deleting the live sibling by symmetry.** The card names this explicitly.
     `DummyDebugStatsTexture` is LIVE: 5 hits — declaration `FGIPass.h:139`,
     null-out `FGIPass.cpp:191`, null-test `:614`, **assignment `:625`
     (`Device->createTexture`)**, use `:627`. It is guarded by `Params3.z` and
     must keep its dummy. Only the `Direction` one goes.
  2. **A false-clean reference sweep.** `search_files` over
     `Engine/Source` TIMED OUT twice this cycle and returned
     `total_count: 0, truncated: true, limit_reason: search_timeout` — a zero
     that is an artefact of the timeout, not of absence. Per the v205 row
     (never believe a zero without reading the query's error field) every
     load-bearing query must be scoped to a directory that completes.
  3. Header edit ⇒ recompiles every TU including `FGIPass.h`. Two of them
     (`TestPathTraceGI.cpp`, `TestReSTIR_GI_Temporal.cpp`); neither can
     reference a private member.

## Why the reference set is closed STRUCTURALLY, not by grep

This is the load-bearing argument and it does not depend on any sweep
completing. `DummyDirectionTexture` is declared under `private:`
(`FGIPass.h:116` opens the private section; the member is at `:140`).
`FGIPass` declares no `friend`. Therefore the only translation unit that
*can* name it is `FGIPass.cpp`. A grep over the whole tree is corroboration,
not the proof — which matters precisely because the whole-tree grep is the
query that times out here.

Corroborating counts, each from a directory-scoped query that completed:

| Scope | `DummyDirectionTexture` | note |
|---|---|---|
| `Runtime/Private/Renderer/GI` | 1 | `FGIPass.cpp:192`, the null-out |
| `Runtime/Public/Renderer/GI` | 1 | `FGIPass.h:140`, the declaration |
| `Engine/Source/Runtime` | 2 | the same two, no third |
| `Engine/Source/Common` | 0 | completed, not timed out |
| `Engine/Source/Runtime/Test` | 0 | controlled by `OutputDirection` → 4 |

`createTexture` in the GI directory → **2 hits**, `:625`
(`DummyDebugStatsTexture`) and `:665` (`MaterialPlaceholderTexture`).
**Neither is the direction dummy** — so the member has no construction site
at all, which is the positive form of "nothing assigns it."

## Acceptance

- `DummyDirectionTexture` → 0 in `Runtime/Public/Renderer/GI` and
  `Runtime/Private/Renderer/GI`, each controlled by a same-shape positive.
- `DummyDebugStatsTexture` still → 5, all five sites unchanged.
- `MaterialPlaceholderTexture` untouched (adjacent line to the deletion).
- No shader file touched — the v182 dual-copy hazard is not engaged.
