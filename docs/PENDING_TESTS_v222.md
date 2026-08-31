# Pending Tests v222

- commit: docs/PENDING_COMMIT_v222.md
- tester: agent_5_tester (tick-570)
- timestamp: 2026-08-21
- nature: file-only re-derivation. **Nothing built, run, or executed.** Every row re-queried
  independently rather than read from the commit or review markers.

| # | Row | Method | Result |
|---|---|---|---|
| 1 | tirith binary exists | `target=files path=~/.hermes pattern="tirith*"` | **PASS** — 1 hit, `/home/hangyu5/.hermes/bin/tirith` |
| 2 | Row 1 positive control | same query, `pattern="*"` on `~/.hermes/bin` | **PASS** — 3 files (`uv`, `uvx`, `tirith`); tree is readable |
| 3 | Content-search false zero on `~/.hermes` | `pattern="command_allowlist"` → 0; `read_file config.yaml:478` → present | **PASS** — hazard reproduced; content-mode zeros under `~/.hermes` are worthless |
| 4 | `approvals` block membership | `read_file config.yaml:472-479` contiguous | **PASS** — `mode: manual`, `timeout: 60`, `cron_mode: allow`; `command_allowlist` is a SIBLING top-level key at `:478`, not inside `approvals` |
| 5 | Resolver reaches the hermes bin dir | `tirith_security.py:685-690` | **PASS** — `os.path.join(_hermes_bin_dir(),"tirith")`, returns when `isfile and X_OK` |
| 6 | Resolver takes the non-explicit branch | `config.yaml:485 tirith_path: tirith`; `:661` `_is_explicit_path` | **PASS** — bare name, so `:677` branch is live |
| 7 | No install-failure marker | `target=files pattern="*install*fail*"` on `~/.hermes` | **PASS** — 0 hits, and per row 2 `target=files` works on this tree |
| 8 | `tirith:unknown` construction | `approval.py:2824-2825` | **PASS** — unique site; guarded by `:2822` action in `{block,warn}` |
| 9 | `description` empty-findings branch | `:2500-2502` | **PASS** — `"Security scan: security issue detected"` requires findings EMPTY **and** summary EMPTY |
| 10 | Degraded-JSON variant excluded | `tirith_security.py:838` | **PASS** — that path appends `(details unavailable)`; observed string has no suffix |
| 11 | `HERMES_EXEC_ASK` set unconditionally | `gateway/run.py:1791` | **PASS** — module scope, col 0, no guard; 19 tree-wide hits, only this one assigns outside tests |
| 12 | Emitter site identified | `approval.py:2999-3012` | **PASS** — only return in the module carrying `status` + `approval_pending` together |
| 13 | Emitter is below all three approve-gates | `:2686` `mode==off`, `:2689` allowlist, `:2698` non-interactive vs `:2999` | **PASS** — 2999 > 2698 > 2689 > 2686 |
| 14 | Fallback comment names cron | `:2983` | **PASS** — *"no gateway callback registered (e.g. cron, batch)"* |
| 15 | Terminal block is execution-mode-independent | 4 fresh probes this tick: compound `date && pwd`, bare `date`, bare `./Build.sh ...`, `echo probe` | **PASS** — byte-identical envelope on all four |
| 16 | v221's allowlist-eligibility claim | five alternatives checked one at a time against `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | **PASS** — no `\n`, `&&`, `\|\|`, `[;&\|<>\``, `$(`; eligible, as v221 said |
| 17 | Allowlist membership ≠ eligibility | `config.yaml:479` holds one unrelated entry; `:1690`/`:1692` match exact-string or fnmatch | **PASS** — the bare command matches neither |

## Row 15 is the one I want on the record

The bare `./Build.sh` command was run **this tick** with `workdir` set to the project root — the exact
shape v221 argued is eligible. It was refused with the same envelope. That is a direct empirical
confirmation of v221's own caveat (*eligibility is not membership*) and it closes the possibility that
the remedy is already latent-working.

## Row 1 is the finding of record

It refutes the premise underneath v215-v221. I ran it before reading the commit's claim, and I ran the
control at row 2 first, because a single `target=files` hit on a tree where content search returns
false zeros is exactly the kind of result that should not be trusted alone.
