# Pending Commit v186

- plan: docs/PENDING_PLAN_v186.md
- files: Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl,
  Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl
- source: no bundle
- target: working tree (NOT committed — job instruction forbids commit/push)
- task: make `FReSTIRConstants` trailing padding agree in kind between C++ and
  both HLSL copies
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
  then `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- notes: three files, +21/-3. No behaviour change intended or possible — see
  the inertness proof below.

## What changed

| file | before | after |
|---|---|---|
| `FReSTIRPass.h:28` | `TFP32 Pad[2]` | `TFP32 Pad0; TFP32 Pad1;` |
| `TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl:22` | `float2 Pad` | `float Pad0; float Pad1;` |
| `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl:12` | `float2 Pad` | `float Pad0; float Pad1;` |

## Inertness proof (both directions, from source)

- **Write side.** `FReSTIRPass.cpp:354-363` marshals field-by-field into
  `float ConstantsData[64]`, incrementing `offset` nine times and stopping at
  `Constants.DebugVis` (offset 8). Floats 9 and 10 are left at the
  `memset(0)` from `:352` and are never written.
- **Read side.** Full read of both 72/115-line generation shaders: neither
  names `Pad` anywhere outside the struct declaration.
  `search_files pattern="gConstants.Pad"` over the whole Runtime tree → 0 hits.

Write-never and read-never, so the field cannot influence a rendered pixel.
This is the strongest claim available without a build, and it is deliberately
weaker than "verified": no compiler and no slangc ran.

## Plan Deviations

**One deviation, and it is the substantive part of this cycle.** The plan
scoped two files. I changed three.

`FReSTIRPass.h` is shared C++ — `search_files` shows `ReSTIR_Generate_cs.hlsl`
exists in **two** data directories, `TestReSTIR_GI_Temporal_Data/` and
`TestCornellBoxGI_Data/`, and both declared `float2 Pad` against the one C++
struct. Patching only the file the card named would have left
`TestCornellBoxGI_Data` disagreeing with the header — i.e. it would have
**relocated** the inconsistency rather than removed it, while the marker
claimed it was fixed. That is a worse outcome than not patching at all,
because the next reader would trust the claim.

This is exactly the failure mode v182 hit from the other side (it patched a
shader copy that `ShaderMake.cfg` does not compile). The lesson generalises to
a rule worth carrying: **before editing one copy of a duplicated shader
struct, enumerate all copies.** Justified under
`six-role-pipeline §Impler deviation policy` — the plan's intent was "make the
declarations agree", and two files could not achieve it.

`TestCornellBoxGI` is the lineage's known-good control
(`software-development-practices §Path-Tracing Methodology` rule 4), so it
must build. The Cornell edit is the same shape as the other two and its own
marshaller is the same shared code path, but **this is unverified** — the
control test was not built either.

## Deferred finding — NOT patched this cycle

A net-new live defect, found while verifying the card. Recording it with
evidence and leaving it for a cycle that can build:

**The bilateral denoise dispatch is sized full-res while its input is
half-res.** At `TestReSTIR_GI_Temporal.cpp:852-853`, `Bd.OutputWidth =
FB.width` (800, from `:106 WIDTH = 800`). But `Bd.InputTexture =
OutputTexture` (`:848`), and `OutputTexture` is created at `HalfW = W/2` =
400x300 (`:1564-1566`). `FBilateralDenoisePass::Dispatch` derives both the
grid (`:179-180`, 800/8 = 100 groups) and `TexelSize` (`:158-159`, 1/800)
from that width, and the shader recovers `outputSize` by inverting TexelSize
(`BilateralDenoise_cs.hlsl:60`) and uses it for its early-out (`:62`) and its
5x5 neighbour bounds test (`:87`). So three quarters of the launched threads
index a 400x300 input with coordinates up to 800x600. `DepthTexture` and
`NormalTexture` *are* full-res, so the guide textures and the input disagree
about what a pixel means.

**Why I did not patch it blind.** The correct value is genuinely ambiguous
from source: the input is half-res but `OutputTexture`/`DenoisedTexture`
(`:1570-1572`) is full-res, so both `HalfResWidth` and `FB.width` are
defensible and they populate different regions of a dump that
`validate_restir_gi.py` and gate 6 both read. Worse, the comment at `:837-844`
says this dispatch is deliberately retained as a **barrier-flushing side
effect** and that "its output is not consumed by ReSTIR" — `DenoisedTexture`
is later fully overwritten by ReBLUR at `:1148`. So the pass may be
load-bearing only for its barriers, in which case changing its grid could
perturb the very layout transitions it exists to flush, for no image gain.
`DenoisedTexture` *is* dumped (`:2519`), so a wrong choice corrupts an
acceptance artifact.

Deciding that needs a build and a run. Guessing would be the fabrication the
job instruction forbids. Queued to PICK instead.

## Not done

No commit, no push, no governance-file edits. No build, no shader compile, no
run, no image inspection — `terminal` is denied categorically on this host
(see the tester/verifier markers for the probe evidence).
