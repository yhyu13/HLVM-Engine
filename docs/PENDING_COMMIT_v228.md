# Pending Commit v228

- plan: docs/PENDING_PLAN_v228.md
- files: docs/PENDING_PLAN_v228.md, docs/PENDING_PLAN_REVIEW_v228.md, docs/PENDING_COMMIT_v228.md, docs/PENDING_TESTS_v228.md, docs/PENDING_TEST_AUDIT_v228.md, docs/PIPELINE_HEALTH_2026-08-21.md
- source: no bundle
- target: (no branch — nothing committed)
- task: Decide the approval-branch question by control flow; report the operator remedy
- verify: `read_file tools/approval.py offset=2685 limit=80` — confirm `:2762 return {"approved": True, ...}` terminates the non-interactive block
- skip_impl_review: no
- produces_test_files: no
- notes: **Zero engine source touched. Zero governance files touched.** No commit, no push.

## Finding 1 — the two standing remedies are not merely inert, they are UNREACHABLE, and this is proved by control flow rather than conjectured from an env var

`check_all_command_guards` (`tools/approval.py:2635`) reaches the cron block only inside:

```
:2692  is_cli     = _is_interactive_cli()
:2693  is_gateway = _is_gateway_approval_context()
:2694  is_ask     = env_var_enabled("HERMES_EXEC_ASK")
:2698  if not is_cli and not is_gateway and not is_ask:
:2700      if env_var_enabled("HERMES_CRON_SESSION"):
:2701          ... cron_mode deny handling ...
:2745          ... tirith_fail_open handling ...
:2762      return {"approved": True, "message": None}
```

**`:2762` is an unconditional `return` at the end of the `:2698` block.** Every path that enters that block returns *approved* unless it hits one of the explicit deny returns (`:2705`, `:2724`, `:2750`) — and all three of those return `{"approved": False, "message": ...}` with **no `status`, no `approval_pending`, no `pattern_key`, no `allow_permanent`** field.

The envelope this session actually receives is:

```
status: pending_approval | approval_pending: True | exit_code: -1
pattern_key: "tirith:unknown" | smart_denied: false | allow_permanent: true
```

That field set is produced **only** at `:2999-3012`, which lies *after* and *outside* the `:2698` block. Reaching it therefore entails that `:2698` was **false** — i.e. one of `is_cli` / `is_gateway` / `is_ask` is **True** for this session.

**Consequence, and it is the cycle's headline:** `cron_mode` (`:2701`) and `tirith_fail_open` (`:2745`) are both *inside* the `:2700` `HERMES_CRON_SESSION` block, which is itself inside the `:2698` block we demonstrably did not enter. **Neither setting is on our code path at all.** Whether `HERMES_CRON_SESSION` propagates to the subprocess — the question v227 left open and prescribed further investigation of — is **moot**: even if it propagated perfectly, we do not reach the test that reads it.

This retires the operator advice of ticks **563, 564, 569, 580** (install tirith / set `tirith_enabled: false` / set `tirith_fail_open` / reconcile `cron_mode`). All four addressed branches unreachable from this session.

## Finding 2 — a NEW tool-soundness bug: `search_files` returns FALSE ZEROS on dot-directories

Controlled experiment, this tick, same tool:

| query | result | truth |
|---|---|---|
| `path=~/.hermes` `pattern=command_allowlist` | **0 hits** | present at `:478` |
| `path=~/.hermes/config.yaml` `pattern=command_allowlist` | **1 hit** (`:478`) | ✅ |
| `path=~/.hermes` `pattern=tirith_enabled` | **0 hits** | present at `:484` |
| `path=~/.hermes/cron` `pattern=enabled_toolsets` | **0 hits** | present ×4 |
| `path=~/.hermes/cron/jobs.json` `pattern=enabled_toolsets` | **20 hits** (`:43,88,134,180`) | ✅ |
| `path=<project>/docs` `pattern=verdict: ALL_KEEP` | 10 hits | positive control: directory scope works on NON-hidden trees |

**Directory-scope search silently skips hidden paths** (ripgrep's default). The last row is the control that isolates the cause to the dot-prefix rather than to directory scope generally.

This is a **third** distinct `search_files` unsoundness in the lineage, after tick-526 (`|` alternation → 0) and tick-538 (regex metacharacter false zeros). It is the most consequential of the three because **`~/.hermes` is where all cron and approval state lives** — it is precisely the tree the last ~18 ticks have been investigating. Tick-564's load-bearing negative "`search_files pattern=\"cron_mode\" path=~/.hermes/cron` → 0 hits ⟹ no per-job override" was a **vacuous query**. (The conclusion survives — re-derived below at file scope — but it was not entitled to.)

**Standing rule (extends tick-526's):** never trust a zero from a directory-scope query on a dot-directory. Query the FILE, or `read_file`.

## Finding 3 — the sole `command_allowlist` entry cannot match any command, and this explains the last 18 ticks

`~/.hermes/config.yaml:478-479`:

```yaml
command_allowlist:
  - script execution via -e/-c flag
```

That string is not command text. It is the **human-readable description** of a dangerous-command regex, at `tools/approval.py:637`:

```python
(r'\b(python[23]?|perl|ruby|node)\s+-[ec]\s+', "script execution via -e/-c flag"),
```

The matcher `_command_matches_permanent_allowlist` (`:1668-1694`) compares against **command text** — exact match `:1690`, or `fnmatch` `:1692` gated on the pattern containing `*?[`. This entry contains no glob metacharacter and equals no command, so it can never return True. Someone previously answered an approval prompt with "always" and the `pattern_key` description was persisted instead of the command.

**So the allowlist mechanism is live and correctly wired — it just contains one entry that is structurally incapable of matching.** That is why every probe still falls through to `:2999`.

## Finding 4 — the acceptance command IS allowlist-eligible (verified, and the lineage disagreed with itself)

`_ALLOWLIST_SHELL_OPERATOR_RE` (`:1660`) = `(?:\n|&&|\|\||[;&|<>`]|\$\()`. The bare command

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

contains none of `\n && || ; & | < > \` $(` — **eligible**. Tick-569's "categorically ineligible" claim is false; tick-580's rebuttal is correct. Confirmed by probing the bare form directly this tick (tick-569 argued eligibility from regex reading but never invoked it): still `pending_approval`, consistent with Finding 3 — eligible but unmatched, because the allowlist holds a description.

`jobs.json` re-derived at FILE scope: 4 jobs, all `enabled_toolsets: ["terminal","file"]` (`:43,88,134,180`); `command_allowlist` → 0 and `cron_mode` → 0 in that file, both now sound (file scope, single-term). **No per-job override; the global config is operative.**

## Plan Deviations

None. The plan's decision procedure was followed as written and returned a decisive answer.
