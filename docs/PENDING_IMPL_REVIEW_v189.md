# Pending Impl Review v189

- plan: docs/PENDING_PLAN_v189.md
- commit: docs/PENDING_COMMIT_v189.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-536)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly on code: `Bd.OutputWidth/OutputHeight` →
`HalfResWidth/HalfResHeight`, one hunk, one file, nothing else touched.

One deviation declared: 20 comment lines against a budgeted short note. **Justified
and I would have required it if it were absent.** This patch deliberately leaves
the pass incoherent (input now half-res; guides and output still full-res), and
that is defensible *only* by the output's deadness. A future reader who finds a
bare two-line change will either re-derive the five deadness queries or, worse,
read the patch as having fixed the pass. Putting the limitation and the "do not
cite this patch as evidence the pass is correct" caveat in the source is the
correct call. Not a design change — no functional effect.

## Load-bearing claim re-derived independently

The impler nominated query 7 (deadness of `DenoisedTexture`) as the claim the
whole verdict rests on, and asked for the `bBypass` and `!bReBLURInitialized`
paths to be checked separately. I did. **All three paths hold:**

| Path | `AccumInput` at `:1211` | Bilateral output reaches display? |
|---|---|---|
| `bBypass` (`:619` from `HLVM_RGI_BYPASS`) | `FullResGIRaw` (`:1111`) | **No — and the bilateral dispatch never runs at all**, `:845` is `if (!bBypass)` |
| `!bBypass`, ReBLUR **off** (`HLVM_RGI_REBLUR=0`, `:551`) | `FullResSpatial` (`:1111`); `:1168` is inside `if (bReBLURInitialized && !bBypass)` so it does not execute | **No** |
| `!bBypass`, ReBLUR **on** (default) | `DenoisedTexture` (`:1168`) — but assigned *after* ReBLUR fully overwrote it at `:1148` with a `FB.width/height` grid | **No** |

`search_files pattern="AccumInput"` → 8 hits; exactly two are assignments
(`:1111`, `:1168`). There is no third. The impler's claim is sound and I found
no path it missed.

Corroborating: the accumulate dispatch (`:1221`) and its constants (`:1204-1205`)
are full-res and read `AccumInput` — which is never the bilateral result. The
display chain `:1213` → `:1227` → `:1234` blit carries no bilateral content.

## Barrier-flush ordering re-checked against the right dispatch

The impler asked me to confirm the flush argument concerns the correct pass. It
does. The comment at `:838-844` says the bilateral *execution* forces pending
layout transitions out before the **ReSTIR** binding sets are created. The
bilateral block is `:845-858`; ReSTIR generation begins `:880`. In
`FBilateralDenoisePass::Dispatch`: `writeBuffer` `:164`, binding set `:167-176`,
`setComputeState` `:185`, `dispatch` `:186`. `dispatchX/Y` are computed at
`:179-180` and read **only** at `:186`. The set of textures bound, their required
states, and therefore the emitted barriers are byte-identical before and after
this patch. **The side effect is preserved.**

## Guard-scope check

`:872-873` sit inside the `if (!bBypass)` block opened at `:845` — confirmed by
reading `:845-873` contiguously. The bypass path is untouched.

## Claim-verification pass (the v188 lesson)

v188 caught an impler asserting a query result it had not run. I re-ran the two
that most affect the verdict, as separate single-term queries per tick-526:

- `search_files pattern="denoised"` on `validate_restir_gi.py` → **0 hits**
  (confirmed; `:190` requires `display`/`spatial`/`gi_raw`/`gbuffer_material`)
- `search_files pattern="denoised"` on `v176-recipe.sh` → **0 hits** (confirmed;
  gate 7 reads `gi_raw` at `:294`/`:299`)
- `search_files pattern="Bd\.Output"` → **3 hits**, matching the marker exactly

**No false claims found in this marker.** Every row in the impler's table
reproduced.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no `system`/`popen` introduced)
- [x] No eval/exec
- [x] No SQL injection
- [x] No new allocation, no new pointer arithmetic, no lifetime change — two
      `uint32_t` member reads substituted for two `uint32_t` struct-field reads

## Self-review checklist

- [x] **Validation:** `FBilateralDenoisePass::Dispatch:149-153` rejects zero
      dimensions with a warning and early-returns, so a hypothetical
      `HalfResWidth == 0` degrades to a logged no-op, not UB. It is not zero:
      `:793` in the same function already uses it for the GI trace grid.
- [x] **Error handling:** unchanged; no new failure mode introduced.
- [x] **Tests:** none required — `produces_test_files: no`, and HARD INVARIANT #2
      is satisfied (`skip_impl_review: no`, so this gate ran).

## Correctness note the marker gets right and should not be softened later

Post-fix, ~3/4 of `DenoisedTexture` is simply **not written** by this pass
(previously it was written with a blur of out-of-bounds zeros). Since
`DenoisedTexture` has `keepInitialState=true` and is fully overwritten by ReBLUR
at `:1148` before any read, this is inert. But the `denoised` PNG at `:2519` will
differ from the pre-patch one. **That difference is expected and is neither a
regression nor an improvement.** No gate reads it.

## Not established

That the file compiles. That the test runs. That any pixel is unchanged. No
build, no run, no image — `terminal` denied (`tirith:unknown`).

## Caveat

Single-profile host: impler and reviewer are the same model. This review
re-executed the marker's queries against source and traced all three
`AccumInput` paths independently rather than reasoning about the marker's prose
(`six-role-pipeline §Anti-patterns §7`).
