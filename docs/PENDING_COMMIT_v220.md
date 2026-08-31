# Pending Commit v220

- plan: docs/PENDING_PLAN_v220.md
- plan_review: docs/PENDING_PLAN_REVIEW_v220.md (KEEP, two binding additions)
- files: **none — zero source files modified.** No engine source, no agent source, no config.
- source: `/home/hangyu5/Documents/Gitrepo-My/hermes-agent` (read-only), `~/.hermes/config.yaml` (read-only)
- target: (uncommitted working tree — this pipeline does not commit)
- task: Separate v219's candidates (A)/(B) from source alone; derive a remedy above the defective branch
- verify: `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no

## Plan Deviations

None. Both plan-gate additions were executed; addition 2 changed the cycle's conclusion, see §FINDING 3.

## FINDING 1 — the candidates ARE separable from source, and v219's stated reason for not separating them is refuted

v219 wrote that separating (A) from (B) "requires reading the process environment via the terminal
under investigation." It does not. `tools/approval.py` gates two sibling entry points on the same three
quantities and **orders them oppositely**, read as contiguous ranges (`read_file`, not grep):

| | `check_dangerous_command` (terminal) | `check_code_execution` (execute_code) |
|---|---|---|
| operands computed | `:2692-2694` `is_cli`, `is_gateway`, `is_ask` | `:3117-3118` `is_gateway`, `is_ask` |
| cron check position | `:2700`, **nested inside** `if not is_cli and not is_gateway and not is_ask:` (`:2698`) | `:3121`, **at function scope, before any `is_gateway`/`is_ask` branch** |
| consequence | cron policy is consulted **only if** all three are false | cron policy is consulted **unconditionally** |

**In `check_code_execution` the cron branch dominates; in `check_dangerous_command` it is subordinate
to three other conditions.** So `approvals.cron_mode` is honoured for `execute_code` and conditionally
honoured for `terminal` — a divergence between two functions whose comments state the same intent
(`:2699` "Cron sessions: respect cron_mode config"; `:3120` "Cron: no user is present to approve").

**This does not require knowing which of `is_gateway`/`is_ask` is true**, which is the point: the
observed refusal proves `is_gateway or is_ask` (`:2887`), and under *either* the `:2698` conjunction is
false and `:2700` is skipped. **Both candidates collapse into one statement with one remedy shape.**
v219 was right that it could not name which operand; it was wrong that naming it mattered.

**Plan-gate addition 1 — is the sibling live?** `check_code_execution` is a real entry point with its
own container fast-path (`:3107-3110`) and its own block result (`:3123-3136`); it is not dead code.
But this session never called `execute_code`, so the differential is a **code-reading argument about
intent**, not an observation of two behaviours. Stated plainly as the plan gate required. It is
nonetheless decisive for the *terminal* path, because the terminal path's control flow is read directly.

## FINDING 2 — enumeration of every early return above `:2698` (plan-gate addition 2)

Read as one contiguous range `:2649-2690`. Seven return sites precede the defective branch:

| # | Site | Condition | Operator-reachable without patching agent source? |
|---|---|---|---|
| 1 | `:2651-2652` | `_should_skip_container_guards(env_type, ...)` | No — deployment shape, not config |
| 2 | `:2658-2661` | `detect_hardline_command` | No, and it is a **block**, not a pass |
| 3 | `:2668-2672` | `_check_sudo_stdin_guard` | No, also a block |
| 4 | `:2677-2681` | `_match_user_deny_rule` (`approvals.deny`) | Config-reachable but a **block** — wrong direction |
| 5 | `:2686-2687` | `_YOLO_MODE_FROZEN or session yolo or approval_mode == "off"` | **YES — `approvals.mode: off`** |
| 6 | `:2689-2690` | `_command_matches_permanent_allowlist(command)` | **YES — `command_allowlist`** |
| 7 | `:2700+` | the defective cron branch | keyed correctly, unreachable |

Sites 5 and 6 are the only two that both **return approved** and are **reachable from `config.yaml`**.

**Wiring verified for both, per the plan gate's explicit warning that an unread key is exactly the
v215/v216 failure:**

- **Site 5.** `_get_approval_mode()` (`:1928-1931`) reads `approvals.mode` via `_get_approval_config()`
  (`:1918-1925`) → `load_config()["approvals"]`. Current value `~/.hermes/config.yaml:473 mode: manual`.
  `_normalize_approval_mode` (`:1887-1898`) accepts exactly `manual|smart|off` and **its docstring warns
  that YAML 1.1 parses bare `off` as boolean False, handled at `:1899` `isinstance(mode, bool)`.** So
  `mode: off` works quoted or unquoted. This branch is read on the terminal path at `:2686`, above the
  defect. **Wired.**
- **Site 6.** `_command_matches_permanent_allowlist` (`:1668-1694`) reads the in-memory
  `_permanent_approved` set, populated by `load_permanent()` (`:1654-1657`), whose only config-side
  caller is `load_permanent_allowlist()` (`:1702-1717`) reading `config["command_allowlist"]`.
  **Two disqualifiers, both verified:** (i) `:1678` rejects any command containing `\n && || ; & | < > \` $(`
  via `_ALLOWLIST_SHELL_OPERATOR_RE` (`:1660`) — and the acceptance command is a compound `cd ... && ./Build.sh ...`,
  so it can never match; (ii) `search_files path=cron pattern="load_permanent_allowlist"` → **0 hits**,
  against 13 hits in `tools/approval.py` and many in `tests/` as same-token positive controls — the
  loader is called from gateway/TUI session setup, **not from the cron scheduler path**. So even a
  non-compound entry may never be loaded in this process. **NOT wired for cron. Rejected.**

`~/.hermes/config.yaml:479` already contains one `command_allowlist` entry — evidence the key is used,
and evidence it has not helped, consistent with (ii).

## FINDING 3 — the remedy, and why it differs from all three prior ones

**`approvals.mode: off` in `~/.hermes/config.yaml`** (currently `manual` at `:473`) is the single
config change that returns approved at `:2686`, **above** the branch that is broken, and is therefore
correct under candidate (A) and (B) alike without either being identified.

Why the three prior remedies failed, each for a different reason now stated precisely:
- **v215 (install `tirith`)** — aimed at a non-problem; v219 Finding 2 showed the binary is present at
  `~/.hermes/bin/tirith`. `tirith` supplies fields, not the branch.
- **v216 (nothing to change)** — true of the *keys it examined*; `cron_mode: allow` (`:475`) is correct
  and inert.
- **v219 (implicitly, fix the cron var)** — correct in mechanism, but its action targets the process
  environment, which no config file controls and no cron job may set for itself.

**Blast radius, stated honestly and not minimised:** `mode: off` disables approval prompts for **all**
sessions on this host, not just this cron job — every interactive and gateway session too. The three
unconditional floors survive it (`:2658` hardline, `:2668` sudo-stdin, `:2677` user deny rules, all of
which fire *above* `:2686` and are documented as un-bypassable by yolo/mode=off). It is nonetheless a
host-wide reduction in guardrails and **the operator, not this job, must weigh it.** The narrower
alternative — a per-job knob that makes `:2700` reachable — does not exist in config today; it would
require an agent-source change, which a cron job must not make to the agent executing it.

## What this cycle did NOT establish

Nothing was built, run, compiled, linted, validated or viewed. The v183-v220 chain remains unbuilt;
acceptance gates 1-7 are unchanged at 0/7. This cycle changed the *remedy*, not any gate's status.
No engine source, agent source, or config file was modified — the remedy is reported for the operator.
