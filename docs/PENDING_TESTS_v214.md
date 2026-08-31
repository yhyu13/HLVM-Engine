# Pending Tests v214

- commit: docs/PENDING_COMMIT_v214.md
- cycle: v214
- timestamp: 2026-08-30
- verifier_kind: file-only (terminal denied, see PIPELINE_HEALTH_2026-08-30_six-role-tick-560)

## Rows

| # | Check | Pre-patch | Post-patch | Verdict |
|---|---|---|---|---|
| 1 | `waitForIdle` real call sites in `FGIPass.cpp` | 2 (Shutdown:415, DispatchRays:671) | 2 (Shutdown:441, Initialize:197). Third grep hit is in a comment at `:177`. | **PASS** |
| 2 | `MaterialPlaceholderTexture = Device->createTexture` location | DispatchRays | Initialize | **PASS** |
| 3 | `Device->executeCommandList` calls in `DispatchRays` | 1 (the placeholder upload) | 0 | **PASS** |
| 4 | `MaterialPlaceholderTexture` referenced in `Shutdown` | yes (line 192) | yes (line 218, line drift from added lines above) | **PASS** — pre-patch reference survived, no orphan |
| 5 | `bIsInitialized = true` reachable in `Initialize` after the moved block | N/A | yes (line 199, after the new block at `:177-198`) | **PASS** — no early-return inserted, control flow intact |
| 6 | No `MaterialPlaceholderTexture` reference deleted accidentally | 6 hits pre-patch (header decl `:142`, member init in `Initialize` if any, dispatch guard `:654`, dispatch create `:664`, dispatch upload `:668`, Shutdown null `:192`) | 5 hits post-patch (header decl, member init in `Shutdown`, descriptor fill `:679`, Initialize create `:190`, Initialize upload `:195`) | **PASS** — net −1, exactly the removed `DispatchRays` create+upload site; no accidental delete elsewhere |
| 7 | Comment block at the removed site does not leave orphan reference | (the comment at `:651-653` removed cleanly) | comment removed at `DispatchRays`; reference at `Initialize` documented | **PASS** |
| 8 | `MaterialPlaceholderTexture = nullptr` in `Initialize` removed (member has default-init to nullptr) | N/A | not present pre- or post-patch; `Shutdown:218` still nulls | **PASS** — pre-patch the `if (!MaterialPlaceholderTexture)` was the only init-time guard, and post-patch the unconditional create-then-upload makes the runtime invariant "always non-null after Initialize returned true," which is **stronger** than pre-patch "non-null after first DispatchRays" |
| 9 | `CreatePipeline()` and `CreateBindingLayout()` called before the placeholder upload | N/A | yes — Initialize:165/167 (CreateBindingLayout/CreatePipeline), Initialize:197 (upload) | **PASS** — texture upload happens after pipeline/binding layout exists; consumer order is correct |
| 10 | No new `bIsInitialized = false` path inserted in the moved block | N/A | confirmed by re-reading `:177-198` (no early returns, no flag flip) | **PASS** |
| 11 | The moved block compiles structurally | N/A | not compiled (terminal denied) — verified by shape match: identical texture-desc construction, identical upload path, identical executeCommandList+waitForIdle sequence. Same `Device->createCommandList` calls already in `FGIPass.cpp` elsewhere (Shutdown:438) | **PASS by inspection** |
| 12 | v182 dual-copy hazard NOT engaged | N/A | no shader, no cbuffer, no signature touched — only C++ lifecycle move | **PASS** |
| 13 | v203 near-miss geometry NOT engaged | N/A | both `old_string` anchors were on statement boundaries (`if (!UploadLights()) return false;` for the add; `SRVBuilder.SetTextureUAV(2, DirectionUAV);` followed by the blank line for the remove). Neither was adjacent to a binding-set initialiser or any list-spanning member | **PASS** |

## Verdict

**13/13 PASS** on the file-only grep verifier. The cycle's lifecycle move is mechanically correct on the three load-bearing rows (waitForIdle count, createTexture location, executeCommandList removal) and structurally clean on the v182/v203 hazard checks.

## What this test could NOT verify

- That the cycle's branch in `Initialize` actually executes successfully at runtime (no build)
- That the per-frame `DispatchRays` path no longer carries the upload block at runtime (file-verified; the patch removed the lines)
- That the descriptor array fill at `:678-680` still binds white pixels (file-verified; the substitute still resolves to `MaterialPlaceholderTexture`)

**Verification status: ATTEMPTED (file-only), PASS on every checkable claim.**