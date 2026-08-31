# Pending Plan v83
- task: evidence-confirmation tick — actively wait for parent evidence; record terminal block + dump-staleness state; stage v84 as last-cycle-before-self-pause
- source: no bundle — file-only audit
- approach: per the v82 PARTIAL_KEEP verdict and the cron's current "do not silently stop" directive, v83 confirms the present terminal-block state THIS TURN rather than re-running the v25-v81 identical-pattern standby loop. v83 will:
  1. Re-confirm terminal block this tick (4 distinct `terminal` calls rejected with `pending_approval: tirith:unknown`).
  2. Re-confirm newest dumps still 2026-07-27 00:07 (no parent has run a fresh build/run).
  3. Re-confirm no `PIPELINE_AWAITING_PARENT_*.md`, `PIPELINE_PAUSED_*.md`, or `PIPELINE_GOAL_DONE_*.md` has been written.
  4. Write a NEW `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md` marking the cron explicitly as "waiting on parent terminal evidence" with a hard v84 deadline.
  5. NOT modify any source file; NOT bump ambient/lighting; NOT re-add `WriteGBufferSentinels`; NOT fabricate progress.
- diff_estimate: +0 / -0 lines source code; ~80 lines of new marker/picker docs
- skip_plan_review: yes (this is the v82 PARTIAL_KEEP pattern's documented evolution; same-shape audit)
- test_strategy: 1 fresh Part A spot-check (target: NOT v28 sentinel, NOT v22 binding-layout — pick a non-recycled patch entry; the v41 alpha-encoder at FImageDump.cpp:27 is the natural choice as the last-untouched cross-tick site); 4 state-confirmation probes (terminal/repo/dumps/PIPELINE_*); explicit "Goal gate UNVERIFIED — terminal-block persists, awaiting parent" verdict
- risks: v83 != v25-v81 standby pattern; v83 will surface an explicit deadline so a v84 next tick can self-pause cleanly per v82's recommendation. v83's honest output is "still waiting on parent" — by design, no progress claimed.
