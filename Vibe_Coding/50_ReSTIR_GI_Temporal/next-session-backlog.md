# Next-Session Backlog (2026-07-20)

Pending items discovered while finishing `50_ReSTIR_GI_Temporal`. Each is
mechanically-actionable; nothing here is `requires_human` and nothing is
currently `blocked`. Anything subjective stays out of this list.

## Ranked by value-to-effort

### 1. (HIGH) `TestReSTIR_GI_Temporal` runtime path bug
- **Where:** `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
- **Symptom:** `FGIPass::Initialize` fails with "Failed to read GIPathTracing.sblob at %s" — the `%s` is printed verbatim by fmt due to a const-char-pointer check. After the compile loop was fixed, the test is *one* runtime fix away from green.
- **Next steps for the next session:**
  1. Patch `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:148` to use `{}` + `TO_TCHAR_CSTR(SblobPath.c_str())` so the path actually prints.
  2. Re-run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=4 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` and read the resolved path.
  3. Pick the appropriate fix based on what surfaces:
     - Wrong path → adjust `MakeShaderDataDir()` in the test cpp.
     - `FPath::Exists` returns false despite the symlink → likely a `char8_t` ↔ `std::string` mismatch in `FPath::Combine`.
     - `ReadBinaryFile` opens empty → ifstream literal issue, `std::filesystem::path::string()` round-trip needed.
  4. Once green, run `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` against the dumped PNG.

### 2. (MEDIUM) Cross-link `Vibe_Coding/51_PathTraceGI_Debug` from `50_ReSTIR_GI_Temporal` README
- **Where:** `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/README.md`
- **Symptom:** README mentions the path-trace-debug session in passing but doesn't link to it explicitly. Future readers benefit from a direct link and a one-line summary.
- **Next steps:** Add a `## References` section linking `../51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md` and a 3-line TL;DR.

### 3. (LOW) Untracked working-tree clutter
- **Where:** `~/.memory/`, `.kilo/`, `.lingma/`, `.sisyphus/`, `.opencode/`, `.git-historian/` etc. all untracked.
- **Symptom:** Many third-party agent / IDE state directories at the repo root. Not a bug, just repo hygiene.
- **Next steps:** Add a `.gitignore` pattern for them. No code change.

### 4. (LOW) ReBLUR alpha-channel mismatch (from ReSTIR_Implementation.md §Known Limitations)
- **Where:** `Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h` + `ReBLUR_cs.hlsl`.
- **Symptom:** Documented limitation since 2026-05-29 — Spatial output alpha is `W` (reservoir weight), not hit distance. ReBLUR's `GetNormHitDist` clamps to 1 because the input is huge.
- **Next steps:** Either fix (output hit distance alongside W in a separate channel), or document the intentional choice. Out of scope for this session; the next session can decide.

## What is NOT in this backlog (deliberately)
- Anything in `50_ReSTIR_GI_Temporal/claude.md` (the original "REBUILD FROM ASH" diagnosis) — superseded by the corrected compute shaders now on disk.
- Items in `51_PathTraceGI_Debug/` — already completed and committed; backlinks only.
- Any `requires_human` / `blocked` state — intentionally not touched.
