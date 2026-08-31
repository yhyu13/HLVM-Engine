# Pending Tests v209

- commit: docs/PENDING_COMMIT_v209.md
- tester: agent_5_tester (tick-555)
- mode: **file-only**. Terminal is denied categorically in this runspace
  (a bare `true` returns `pending_approval / tirith:unknown / exit_code -1`),
  so nothing here was compiled, linked, run or rendered. Every row below is a
  source-level check and is labelled as such.

## Rows

| # | Check | Method | Result |
|---|---|---|---|
| 1 | Target symbol gone | `DummyDirectionTexture` on `Engine/Source/Runtime` | **0** PASS |
| 2 | Zero is not vacuous (control A) | `DummyDebugStatsTexture`, same scope | **5** PASS |
| 3 | Zero is not vacuous (control B) | `MaterialPlaceholderTexture`, same scope | **6** PASS |
| 4 | Live sibling's 5 sites all intact | read `FGIPass.cpp:191,613,624,626` + `FGIPass.h:139` | PASS |
| 5 | Adjacent member survived both edits | read `FGIPass.h:146`, `FGIPass.cpp:192` in place | PASS |
| 6 | u2 fallback (the v207 fix) untouched | `DirectionUAV` → 3 hits, `:644` ternary onto `Desc.OutputTexture`, `:647` state, `:649` bind | PASS |
| 7 | No shader file touched | no `.hlsl` in commit `files:` | PASS |
| 8 | Class structure intact | `FGIPass.h` closes `:150`, namespace `:151`, 151 lines total | PASS |
| 9 | Comment's u1 claim is true | shader `:104` decl, `:830` `Params3.z` guard, `:829/:837` `#if` | PASS |
| 10 | Comment's u2 claim is true | shader `:101` decl, `:645` store, no conditional | PASS |

## Row 6 is the one I added and it is the one that matters

The plan and the commit both frame this cycle as removing dead state. The
risk nobody wrote down is the **inverse** of deleting the live sibling:
deleting the dummy is only safe *because* v207's ternary made it dead, so if
this cycle had disturbed that ternary it would have re-armed the very defect
v207 fixed while presenting itself as a no-op cleanup.

`nvrhi::TextureHandle` in the GI directory → **3 hits**: `:605` (u1's local),
`:644` (u2's local), `:679` (the material loop's). Declaration-shaped
enumeration, so it settles the local-handle set rather than counting a name.
u2's local is present and still initialised from
`Desc.OutputDirection ? ... : Desc.OutputTexture`. **The v207 fix is intact.**

## Correction to the commit marker

The commit's change table cites the pre-patch line numbers `FGIPass.h:140`
and `FGIPass.cpp:192` for the deletions. The `.cpp` figure is right by
coincidence only — `:192` was the deleted line pre-patch and is now
`MaterialPlaceholderTexture` post-patch. The table is correct as written
(it says "Line (pre)") but a reader skimming it against the current file will
land on a different statement. **Not a defect in the patch; a marker that
invites a misread.** Recorded rather than smoothed, per the v208 precedent of
logging a count mismatch instead of quietly reconciling it.

## What this tester did NOT do

Did not build, link, run, compile a shader, produce a dump, run
`validate_restir_gi.py`, or view an image. **No acceptance gate is cleared by
this cycle**, and none is claimed.
