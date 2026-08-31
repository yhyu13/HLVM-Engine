# Overseer escalation

- card: `t_7b79c010`
- edge case: EC-039
- finding: scheduled tick requested terminal access, but every probe returned `pending_approval: tirith:unknown`.
- impact: card status/runs, git status, dispatch, Debug build, mode-20 execution, and newest-stamp validator cannot be independently checked.
- parent action: reconfigure terminal access, run this watchdog interactively, or pause/restructure the cron.
- safety: `AUTO_RESOLVE_DO_NOT: yes` remains authoritative; no auto-resolution or card mutation was attempted.
