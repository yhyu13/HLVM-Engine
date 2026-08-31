# Pending Plan v211

- task: Card T (opened by this cycle's own re-derivation) — a **THIRD, STALE
  COPY** of `BilateralDenoise_cs.hlsl` exists at a path no cycle in 557 ticks
  has named, it is **compiled by its own ShaderMake target**, and it is in the
  pre-v204 shape.
- source: no bundle — direct edit
- skip_plan_review: no
- diff_estimate: +14 / -3

## Why this cycle and not a card from PICK

`search_files pattern="^- \[ \]"` on `PENDING_PICK.md` → **3** actionable
cards (L, M, N). **All three are precondition-gated on the same build**, and
that build is unreachable: `terminal` was probed first-hand this tick
(`pwd`, a no-op builtin) and refused at the tool boundary
(`pending_approval / tirith:unknown / exit_code -1`). Card L states the
precondition explicitly ("do NOT action while the v183-v198 chain is
unbuilt"); cards M and N inherit it by reference.

So Rule 9 has three cards it cannot route. Per v191's precedent — and rather
than emit a 557th closure document (`§Anti-patterns §6` drift) — the planner
re-derived from source, applying **v203's standing rule**: *when a cycle
produces a new invariant, the next cycle's default job is to sweep that
invariant's domain.*

The invariant chosen is **v182's**, which is the one invariant in the entire
lineage that was *discovered* and then **never swept**: v182 found that
`GIPathTracing.hlsl` exists in two copies and that only one is compiled, named
it "the dual-copy hazard", and thereafter **eleven cycles cited it as a reason
to avoid touching shaders** — v192, v193, v195, v197, v207 each recorded "both
copies byte-unchanged, so the v182 hazard is not engaged." Not one of them
asked the obvious next question: **which shaders have copies, and are those
copies in agreement today?**

## What the sweep found

Domain enumerated by query, one filename per query (tick-526's alternation
rule), `path` at a directory (v199), enumerated file lists read (v210):

| Shader | Copies | Status |
|---|---|---|
| `GIPathTracing.hlsl` | 2 | **CLEAN** — both 949 lines / 41,340 bytes; `gbPixel` 12/12, `OutputDirection` 3/3 |
| `GIAccumulate_cs.hlsl` | 2 | in scope, not this cycle |
| `ReBLUR_cs.hlsl` | 2 | v204 verified clean |
| `BilateralDenoise_cs.hlsl` | **3** | **DEFECT** |

The two-copy assumption every prior cycle carried is **false for this shader**.
There is a third at `Engine/Source/Runtime/Shader/BilateralDenoise_cs.hlsl`.

## Root cause

v204 fixed the Phase-D guide-extent defect in this pass, and v205 corrected
the scale's source operand. Both patched the copies under `Test/*_Data/`. The
third copy **never received either change**:

- `GuideScale` → **0 hits** (controlled: `cbuffer` → 1 in the same file, so
  the file is readable and the query shape is sound; and 3 hits in the primary
  copy, 2 in the control copy, same query).
- Slot 5 of its cbuffer is still `float Pad0` (`:21`).
- Both guides are indexed with the **raw dispatch coord** — `t_Depth[pixelCoord]`
  `:66`, `t_Normal[pixelCoord]` `:67`, and the neighbour pair `:97`/`:102` —
  i.e. **exactly the v204 defect, verbatim, in a live compiled file.**

## Why it is compiled, and why that matters

`Engine/Source/Runtime/Shader/ShaderMake.cfg:5` lists
`BilateralDenoise_cs.hlsl -T cs`, and `ShaderMakeBuild.py:449-458`
(`create_common_shadermake`) points the `Common_ShaderMake` target at
`${CMAKE_SOURCE_DIR}/Shader`. Corroborated independently in the generated
build graph: `Build/Debug/build.ninja:2372` invokes ShaderMake on that
directory's cfg. **This is not dead source; it is a build product.**

Its docstring names it the default: *"FCommonRenderPasses uses these by
default; tests can override with SetShaderDataDir()."*

## Severity — stated without inflation

**Latent, not live.** `FBilateralDenoisePass::Initialize` loads
`BilateralDenoise_cs.sblob` from its `InShaderDataDir` argument (`:44`, `:48`),
and both current consumers pass their own `DataDir`
(`TestReSTIR_GI_Temporal.cpp:537`, `TestCornellBoxGI.cpp:901`). So **no
consumer binds this blob today and this cycle moves no pixel.**

What makes it worth a cycle is the *asymmetry of the failure*: it is the
**default** copy, in the **stale** shape, and the defect it carries is silent
by construction — wrong guide texels produce wrong bilateral weights, with no
VUID and no error. A future consumer that omits `SetShaderDataDir` gets the
pre-v204 shader and reintroduces v204's defect **without changing a line of
C++**, and no query shape in the lineage's audit list would flag it: a
`GuideScale` sweep of the *consumers* comes back clean, because the consumers
are clean.

This is the **seventh false-instrument mechanism** and the first that is a
property of the repository's *layout* rather than of the query: every prior
one (526 alternation, 199 path-at-a-file, 200 comment hits, 205 uncontrolled
zero, 208 BRE/ERE, 209 timeout, 210 truncated enumeration) is defeated by
writing the query better. **This one is defeated only by not assuming you know
how many copies a file has.**

## Approach

Bring the third copy into agreement with the primary, in the exact form the
primary uses (v205's post-correction shape), so the three copies differ only
where they are *documented* to differ:

1. `float Pad0` → `float GuideScale` at slot 5. **Layout-neutral** — same
   type, same position, same 8-float total; the v200/v184 cbuffer rule is not
   engaged.
2. Add the `GB()` helper byte-identical to the primary's `:35-39`, including
   `max(int(GuideScale), 1)` so an unfilled constant degrades to the
   **identity map** (v184's silent-zero lesson).
3. Route the four guide reads through `GB()` — `:66`, `:67`, `:97`, `:102`.
   `t_Input` and `u_Output` stay on the raw coord; they are dispatch-res.

**This is an identity transform at `GuideScale == 1`**, which is the only
value any consumer can produce today, so it cannot perturb the v183-v210 chain
awaiting its first build.

## Why not the Cornell copy's `GuideScale_Unused` treatment instead

The control names slot 5 `GuideScale_Unused` and deliberately does **not**
apply `GB()`, correctly, because that target dispatches at its guides'
resolution and the scale is provably 1 (`TestCornellBoxGI.cpp:1592`/`:1645`).
That reasoning **rests on knowing the consumer's extent**. The shared copy's
consumer is unknown by construction — that is what "default" means — so the
correct default is the form that is right at any scale and identity at 1.
Applying the control's treatment here would encode an assumption the file is
in no position to make.

## test_strategy

File-only, and every row falsifiable:
- returned diff read in full, not the line count asserted (v203);
- cbuffer float count 8 before and after, order preserved (v200);
- `GB(` count in the third copy equals the primary's, and the guide-read sites
  enumerated as a **set**, not a count (v191);
- the two already-correct copies proven **byte-unchanged**;
- every zero controlled by a same-shape positive (v205).

## risks

- **The v203 near-miss**: anchor `old_string` on declaration statements, never
  on a comment adjacent to a braced list. The cbuffer here *is* a braced list.
- **Over-reach**: `t_Input`/`u_Output` must NOT go through `GB()`. Rewriting
  them would break the pass at every scale, including 1 — the one change here
  that could turn a latent defect into a live one.
- **Scope**: `GIAccumulate_cs.hlsl`'s two copies are in the same domain and
  are deliberately left for a separate cycle, so this cycle's own
  "three copies, all enumerated" row stays verifiable.
