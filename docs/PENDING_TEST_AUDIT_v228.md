# Pending Test Audit v228

- tests: docs/PENDING_TESTS_v228.md
- commit: docs/PENDING_COMMIT_v228.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-581)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] **No `|` alternation in any pattern** (tick-526) — every query single-term
- [x] **No `path=` at a directory in file-listing mode** (tick-526) — respected
- [x] **NEW: no load-bearing zero from a dot-directory** (this cycle) — the one such zero (row 11) is explicitly marked INCONCLUSIVE rather than PASS
- [x] **Every load-bearing zero paired with a same-scope positive control** (v217) — rows 4+5, 6+7+8, 15
- [x] **No count inherited across markers without re-derivation** (v211) — tester re-ran all 17
- [x] **No runtime result fabricated** — nothing built, run, executed, or viewed by any role
- [x] **Patch-tool diff read before declaring done** (v203/v224) — N/A, zero patches
- [x] **No self-granting permission change** — `~/.hermes/config.yaml` read-only, byte-unchanged

## Per-test verdict

17 rows / 16 KEEP / 1 KEEP-as-INCONCLUSIVE (row 11).

**Row 11 is the row I want to commend rather than pass over.** The tester had a result (0 hits) that matched the lineage's standing claim and would have been accepted by 18 prior ticks. It declined to record it as PASS because the *method* had just been shown unsound in this very cycle, and it went further by observing the row is irrelevant given Finding 1. Applying a newly-discovered tool bug retroactively against one's own convenient result is the behaviour these gates exist to produce, and its absence is what let ticks 563-580 accumulate confident advice about dead code.

## What this cycle established

1. **The lineage's blocker diagnosis was wrong for ~18 ticks, and is now settled by control flow.** `approvals.cron_mode` and `security.tirith_fail_open` sit inside the `:2698` non-interactive block. That block returns unconditionally at `:2762`, and none of its four exits can emit `status: pending_approval`. We receive that status, therefore we never enter the block, therefore neither setting is consulted. This is a proof from the observed envelope, requiring no assumption about `HERMES_CRON_SESSION` — the question v227 left open is dissolved, not answered.
2. **`search_files` returns false zeros on dot-directories** — the third distinct unsoundness in this tool (after `|` alternation and regex metacharacters) and the most damaging, because `~/.hermes` is the tree holding all cron and approval state. Tick-564's "no per-job `cron_mode` override" was a vacuous query; re-derived soundly at file scope, the conclusion holds.
3. **`command_allowlist` contains a dangerous-pattern *description*, not command text**, so it cannot match any command. The mechanism is live and correctly wired; its only entry is structurally incapable of matching. This is the concrete reason every probe falls through.
4. **The acceptance command is allowlist-eligible** — zero shell operators per the `:1660` regex. Tick-569's "categorically ineligible" is false; tick-580's rebuttal stands, now confirmed by live probe of the bare form.
5. **This session is cron job 3** (`jobs.json:97`, `enabled: true`, `scheduled`, 3567 completed runs, `enabled_toolsets: ["terminal","file"]`). The pipeline is genuinely running; the toolset is genuinely granted. The block is at the approval layer, downstream of both.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | BLOCKED | `terminal` refused (2 fresh probes this tick, compound + bare) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 46+ unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

**This cycle's contribution to the gates is zero, and I decline to present it otherwise.** What it repaired is the *diagnosis of why the gates are unreachable*, which had been wrong since tick-563. The operator action below is the first one in the lineage aimed at a branch that actually executes.

## What no role in this cycle did

Did not build, run, compile, validate, or view any image. Did not commit or push. Did not modify engine source — `FGIPass.cpp`, `FGIPass.h`, both `GIPathTracing.hlsl` copies, `TestReSTIR_GI_Temporal.cpp`, `validate_restir_gi.py`, `v176-recipe.sh` all byte-unchanged. Did not modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, `~/.hermes/config.yaml`, `jobs.json`, or any governance file. **Did not fabricate any runtime result.**

## Operator action — CORRECTED, and different from every prior tick's

Prior ticks asked for tirith installation, `tirith_enabled: false`, `tirith_fail_open`, or `cron_mode` reconciliation. **All four target the unreachable `:2698` block. Do not bother with them.**

The allowlist short-circuit at `:2689-2690` fires *before* every context predicate, so it is the one lever on our path. It already works — it just holds a description string instead of a command. Edit `~/.hermes/config.yaml:478`:

```yaml
command_allowlist:
  - script execution via -e/-c flag        # existing; inert, see below
  - ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

The added line is exact-matched at `:1690`. It contains no character in `\n && || ; & | < > ` $(`, so it clears `_has_allowlist_shell_operator` at `:1678`. Do **not** prepend `cd <root> &&` — the `&&` makes it categorically ineligible, and it is unnecessary because `jobs.json` already sets the job's `workdir` to the project root.

If `Build.sh`'s own sub-invocations trip further prompts, add them one at a time, or use a glob (`./Build.sh *`) — globs are honoured at `:1692`.

The existing first entry may be deleted; it is a persisted `pattern_key` description that the matcher (which compares command text) can never match. See card O.

## Verdict

**ALL_KEEP.** Seventeen rows independently re-derived, every zero controlled, the one unsound row correctly demoted rather than quietly kept. The cycle's central claim is a modus tollens on directly-read code, not an inference from configuration — which is why it settles a question 18 ticks of configuration-reading could not.
