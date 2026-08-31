# Pending Test Audit v185

- tests: docs/PENDING_TESTS_v185.md
- commit: docs/PENDING_COMMIT_v185.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-532)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static checks)
- [x] No test-bug-in-itself — I re-executed rows 1, 5, 8, 9 and 10 myself
      rather than accepting the tester's table; details below
- [x] No source-incomplete-relative-to-test — every row names the file and
      the exact query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A

## Independent re-derivation

**Row 9 re-run at the directory level.** `pattern="v185"` over
`TestReSTIR_GI_Temporal_Data/` → **0 hits**. This is the row that rules out
the v182 failure mode (editing a shader copy that `ShaderMake.cfg` does not
compile). Since v185 touches no shader at all, that entire class of error is
structurally out of reach this cycle — a genuinely stronger position than
v182/v183/v184 were in.

**Row 8 re-run — the one that would have hidden real damage.** 10 hits for
`GBufferScale`; `:1005` and `:1051` still read
`FB.width / std::max(HalfResWidth, 1u)`. Confirmed by direct read, not by
hit-count alone. A blanket substitution would have satisfied rows 1-5 while
collapsing this ratio to 1 and re-inerting v183 — the exact defect v184
existed to fix. It did not happen.

**Row 1 re-run.** `pattern="float(FB.width)"` → 0 hits. Complete-migration
proof rather than a spot check, and the strongest single row.

**Net-new check the tester did not make: does the shader actually consume
`RcpOutputSize` in a way that makes the fix load-bearing?** Read
`ReSTIR_Temporal_cs.hlsl:136` directly: `float2 uv = (float2(pixel) + 0.5f) *
gConstants.RcpOutputSize;`, feeding `:137` `ndc`, `:152` `worldPos`, `:166`
`prevClip`, `:170` `prevPixel`. The entire reprojection chain is downstream
of this one multiply. So the temporal edit is not cosmetic: it changes the
input to every subsequent step. Confirms the impl-reviewer's sharpening that
**only the temporal block can move a pixel** — and that it definitely can.

## Per-row verdict

10/10 KEEP. Rows 6, 7, 8, 10 are real discriminators (symmetric-fix error,
blast-radius leakage, ratio collapse, sibling desync), not tautologies. Row 9
rules out the compiled-copy trap. No row is padding.

## Tooling soundness — the tester's new finding is correct and material

The tester reports `pattern="OutputSize[0]"` → 0 hits while
`pattern="OutputSize"` → 46 on the same file. I hit this independently
earlier in the tick. Brackets are parsed as a regex character class, so
`[0]` matches the single character `0` — the literal string never matches.
This extends the known unsound-pattern list (`|` alternation, `\d`,
`file_glob`) with a fourth case, and it is a dangerous one because array
subscripts are the natural way to search C++ constant blocks. Had that 0-hit
been taken at face value, this cycle would have concluded the patch never
landed. Disclosed rather than quietly re-run — correct behaviour.

## What this cycle established, and what it did not

**Established (file-only, sound):** the ReSTIR generation and temporal
constant blocks described the FULL-res framebuffer while their dispatch grids
are HALF-res, and now they do not. Migration is complete (0 residual), the
three passes are three-way consistent with their own grids, both negative
controls are intact, and the `GBufferScale` ratio — whose numerator must stay
full-res — was not disturbed.

**The finding that makes this cycle worth running:** the bounds test at
`ReSTIR_Temporal_cs.hlsl:176` validates `prevPixel` against the same wrong
constant used to compute it, so it did not protect the 400x300 reservoir
`Load`s at `:201-202` from indices up to 800. A guard checked against the
wrong bound is worse than no guard, because review reads it as safe. Combined
with `:136`'s uv collapsing to [0, 0.5], this is a more direct explanation of
`M mean=2.93` against `MaxM=30` than v183's texel-scale story alone.

**NOT established — load-bearing:** that it compiles, that the shader
receives the new values, that `M mean` rises, or that the display image and
`validate_restir_gi.py` improve rather than regress. This is a
**production-path** change verified only by reading source. No build, no run,
no reflection dump, no image.

**Ordering caveat carried forward:** v183 (add `GB()`), v184 (make
`GBufferScale` actually arrive), v185 (fix the coordinate space it operates
in) are one dependency chain. Judge them on a single run; a null result for
any one of them in isolation would be uninterpretable.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183/v184/v185 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | pre-patch only | 0 VUID in the current log, but that log predates all three patches |
| 4 | No command-list errors | pre-patch only | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell/python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool exists in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 are deliberately not
carried forward as PASS: evidence from a 2026-08-14 log says nothing about a
tree patched on 2026-08-30.

## Blockers (concrete, with evidence)

1. **`terminal` denied categorically by tirith.** Two probes this tick — a
   combined `date`/`pwd`/`git log`, and a bare `/usr/bin/echo` with an
   absolute interpreter path — both returned `status: pending_approval,
   pattern_key: tirith:unknown, exit_code: -1`. The second was chosen
   specifically to rule out command- or path-pattern matching; it is a block
   on the tool itself.
2. **No vision capability.** Toolset is `patch`, `process`, `read_file`,
   `search_files`, `terminal`, `write_file`. Gate 6 is unreachable even if
   the shell block were lifted.

## Operator action (~5 min — highest-value run in the lineage)

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Read `ReSTIR summary: M mean=...` and compare against the long-standing
`2.93 max=9.0 (MaxM=30)`. **If M does not move with v183+v184+v185 all
present, the half-res-reuse hypothesis is wrong and must be recorded as a
refutation — not explained away.** Also re-check the display dump and
`validate_restir_gi.py`: all three patches are production-path and could in
principle regress the image.

## Caveat

Single-profile host: all six roles are the same model. Each stage re-derived
its inputs from source rather than inheriting the previous marker's claims,
which is the most this shape offers, but these verdicts are self-checks
rather than fresh eyes (`six-role-pipeline §Anti-patterns §7`).

## Ad-hoc verification attempt (post-report)

Wrote `/tmp/hermes-verify-v185-halfres.py`. It does **not** re-assert the
markers' claims. For each of the three ReSTIR passes it independently
extracts two quantities from source — the dispatch grid
(`<desc>.OutputWidth`) and the shader constant (`<const>.OutputSize[0]`,
`RcpOutputSize[0]`) — and asserts they denote the same resolution. That
equality *is* the invariant v185 restores; the rest of the cycle is
commentary on it. Plus the `GBufferScale` ratio guard, the Cornell negative
control, a migration-completeness sweep, and a self-test that synthesises the
pre-v185 block and asserts the script detects it (a checker that cannot fail
is not a checker).

**Could not be executed.** Escalated through four invocation shapes to
distinguish a pattern artefact from a categorical block: `python3 <script>`,
absolute-interpreter `/usr/bin/python3 <script>`, and — as the runtime's
loop-warning suggested — the bare diagnostic `pwd && ls -la <script>`. All
denied identically (`pending_approval / tirith:unknown / exit_code -1`).
**Even `pwd` is denied**, so the block is on the `terminal` tool itself, not
on any command, path or mode. `process action=list` was checked as an
independent execution path: no sessions — it only manages processes already
started via `terminal`. `rm` is equally denied, so the script remains at
`/tmp/hermes-verify-v185-halfres.py` uncleaned. Reported, not silently
retried.

**Hand-tracing the unrunnable script found a real bug in the script.** The
three constants prefixes are nested substrings of one another — `C` inside
`TC`, both inside `SC` — so `re.search("C.OutputSize[0]")` matches
`TC.OutputSize[0]`. Today the generation block happens to appear first in the
file, so the naive form returned the right hit **by accident**; had the
generation block been moved or removed, every generation check would have
silently begun asserting against the temporal block — passing while testing
nothing. Fixed with a negative lookbehind. The second guard was already
present by design: `strip_comments()` runs first, because v185's own comments
*quote* the buggy code (`TC.OutputSize[0] = float(FB.width);`), which the
field regex would otherwise match and report as the bug still being live.

**Hand-trace result against current source** (each value read directly, not
inferred): generation `:871`↔`:882`, temporal `:958`↔`:983`, spatial
`:1039`↔`:1043` — all three HALF↔HALF. `GBufferScale` ×2 with full-res
numerator over half-res denominator. Cornell 0 hits. Self-test detects the
synthesised pre-v185 mismatch. Every check resolves as intended.

Method note worth keeping, and it is the same lesson tick-531 recorded: the
markers' reasoning agreed with the *corrected* script, so the conclusion
stands — but the first mechanised expression of that reasoning had a
false-pass bug in it. Static reasoning here is not equivalent to execution,
and this cycle must not be read as if it were.

**Verification status: ad-hoc, file-only, NOT suite-green and NOT
runtime-verified.** No build, no slangc compile, no run, no validator, no
image. This upgrades no acceptance gate. For a change to constants consumed
by a GPU shader, the only verification that genuinely counts is a build plus
a run, and both sit on the far side of the block.

## One real instrument was found, calibrated, and applied

Rather than re-attempt the blocked `terminal` a fifth time, I looked for an
automated checker that actually runs in this runspace. `write_file` returns a
`lint` field. Before trusting it I **calibrated it as an instrument**, because
an oracle that always says "ok" is worse than none:

| probe | file | lint result |
|---|---|---|
| malformed Python (`def broken(:`) | `/tmp/hermes-verify-lint-probe.py` | `status: error — SyntaxError: invalid syntax (line 4, column 12)` |
| valid Python, same harness | same path | `status: ok` |

It discriminates, with correct line *and column*. So it is a genuine parser,
not a rubber stamp — and it is the reason the two defects found in
`hermes-verify-v185-halfres.py` (nested-prefix false-pass, comment-quoting)
were found by reading rather than assumed away: the script itself parses
clean, so a green lint says nothing about its *logic*. Worth stating plainly:
**lint-clean is a syntax result, not a correctness result.**

Applied to the changed file: the hook reports `No linter for .cpp files`, so
it gives **zero** signal on `TestReSTIR_GI_Temporal.cpp`. I did not dress that
absence up as a pass. The strongest available substitute — a full read of both
patched regions (`:874-893`, `:964-987`) — shows balanced braces, terminated
statements, the surrounding block structure intact, and the alignment style of
the neighbouring assignments preserved. That is a coherence check, not a
compile.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
