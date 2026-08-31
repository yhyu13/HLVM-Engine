# Ad-hoc Verification — v189 (2026-08-30)

**This is ad-hoc static verification, NOT suite green.** Nothing was compiled,
linked, run, validated or viewed. The canonical gate remains
`./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`.

## Defect found by this verification pass, in v189's own comment

**The first draft of the v189 comment contained stale line-number
cross-references — the exact defect v187 caught and named.** It cited
`:1564-1566` for the input texture, `:1148` for the ReBLUR overwrite,
`:1570-1572` for `DenoisedTexture`, `:849-850` for the guides, plus four
`FBilateralDenoisePass.cpp` / `BilateralDenoise_cs.hlsl` offsets.

**Every one was wrong the moment it was written**: the comment's own 20 inserted
lines shifted the targets it cited. Verified after the fact —
`DenoisedTexture = CreateTexture2D` is at **`:1590`**, not `:1570-1572`;
`ReBLURDesc.OutputTexture` is at **`:1168`**, not `:1148`.

Per tick-534's rule — *line-number cross-references between co-edited regions are
wrong by construction* — the comment was rewritten symbolically ("the HALF-res
InputTexture above", "ReBLUR overwrites it in 5.5") and cut from 20 lines to 12,
matching the density of the surrounding block at `:837-844`. **Assertion [6] was
added to the script so this class cannot recur silently.**

## Execution status: script written, could NOT be run

`/tmp/hermes-verify-v189.py` — 16 assertions, 6 sections. Four distinct
`terminal` invocations this turn, all rejected identically
(`pending_approval / tirith:unknown / exit_code -1 / smart_denied false`):

| Probe | Purpose |
|---|---|
| `echo probe-ok; date -u; pwd` | baseline |
| `/usr/bin/true` (absolute path, no args, `workdir=/tmp`) | rule out command-pattern matching |
| `python3 /tmp/hermes-verify-v189.py` | the script itself |
| `python3 ... ; echo "exit=$?"` (`workdir=/tmp`) | re-attempt after script rewrite |

The bare-`/usr/bin/true` probe is decisive: **the block is on the `terminal`
tool**, not on any command shape. The runtime fired
`same_tool_failure_warning; count=4`; I stopped rather than loop.

**Fallback: all 16 assertions hand-executed via `read_file`/`search_files`.**
Every query below was run this turn. Per tick-526, all are single-term — no `|`.

## Results — 16/16 hold

### [1] The edit landed, and only where intended

`search_files pattern="Bd\."` → **9 hits**, the complete `FDesc` init block:

```
848: Bd.InputTexture  = OutputTexture       851: Bd.OutputTexture = DenoisedTexture
849: Bd.DepthTexture  = LinearDepthTexture  864: Bd.OutputWidth   = HalfResWidth
850: Bd.NormalTexture = GBufferNormal       865: Bd.OutputHeight  = HalfResHeight
866-868: DepthSigma / NormalSigma / SpatialSigma
```

- **[1a]/[1b]** `OutputWidth`/`OutputHeight` on `HalfResWidth`/`HalfResHeight` — PASS
- **[1c]** `InputTexture == OutputTexture` (the half-res texture) — PASS
- **[1d]** **no `Bd.*` field reads `FB.*`** — PASS. All 9 assignments parsed from
  comment-stripped lines, so the check cannot be satisfied or defeated by prose.

### [2] Initialised before use

- **[2a]** `HalfResWidth = HalfW` → **1 hit** (`:1574`). Assigned exactly once.
- **[2b]** `:793 Desc.OutputWidth = HalfResWidth; // Phase D: half-res trace` —
  the GI trace grid consumes it **~70 lines before** the patched site, in the
  same render path. Strong ordering proof: zero there would empty the tracer's
  grid first. PASS

### [3] Grid and TexelSize derive from `OutputWidth`

`FBilateralDenoisePass.cpp:179` `dispatchX = (outputW + 7) / 8`; `:158`
`ConstantsData[0] = 1.0f / static_cast<float>(outputW)`; `createBindingSet`
(`:176`) → `setComputeState` (`:185`) → `dispatch` (`:186`), with `dispatchX/Y`
computed `:179-180` and read only at `:186` — **the flush is grid-independent**;
`BilateralDenoise_cs.hlsl:60` inverts `TexelSize` to recover bounds. 4/4 PASS

### [4] Nothing a gate reads can move

- **[4a]/[4b]** `denoised` → **0 hits** in `validate_restir_gi.py`, **0 hits** in
  `v176-recipe.sh` — PASS
- **[4c]** `AccumInput =` → **exactly 2** (`:1123`, `:1180`) — PASS
- **[4d]** `ReBLURDesc.OutputTexture = DenoisedTexture` at **`:1160`**
  **precedes** the `:1180` assignment — the only path carrying that texture
  toward display does so strictly after ReBLUR overwrote it. PASS

### [5] Blast radius

- **[5a]** Cornell still `DenoiseDesc.OutputWidth = CurrentFBInfo.width`
  (`:1483`) — sibling untouched, self-consistent. PASS
- **[5b]** `:845 if (!bBypass)` encloses `:864-865`. PASS

### [6] No stale line-number cross-references *(new)*

Scanned the v189 comment block (`:852-863`) for `:\d{2,}` → **0 matches**. PASS

## Latent bugs found in the script while reviewing it

Recorded because a script that passes vacuously is worse than none:

1. Stray `return_code = 2` at module scope before `sys.exit(2)`. Removed.
2. **A raw `FB.width` grep would have failed on the patch's own documentation** —
   the comment names `FB.width` verbatim. Assertion [1d] is therefore stated over
   parsed `Bd.<field> = <value>;` assignments from comment-stripped lines.
   `code()` is documented as `//`-only, which suffices for these assertions.
3. Duplicated assignment-parsing between the `Bd` and Cornell checks — factored
   into one `assigns()` helper (DRY), which also made [5a] a value comparison
   rather than a substring match.

## What this does and does not establish

**Does:** the edit is present at both intended sites and nowhere else; no `Bd.*`
field carries a full-res extent; the value is initialised before use with an
upstream consumer proving non-zero; grid and TexelSize derive from the changed
field; the flush is grid-independent; the output is unreachable from every
gate-read artifact on all control paths; the sibling caller is untouched; the
comment carries no line references that can rot.

**Does NOT:** that the file compiles, that the target links, that the dispatch is
correct at runtime, that `VUID-VkDescriptorImageInfo-imageLayout-00344` stays
absent, or that any pixel is unchanged. **0/7 acceptance gates verified.**

## Cleanup

`/tmp/hermes-verify-v189.py` **could not be removed** — deletion requires shell,
which is blocked. Left in place, runnable verbatim once an operator has a
terminal; its exit code is the check.
