# Pending Plan v92
- task: restir-gi-fix — diagnostic narrowing continues
- source: no bundle — direct edit (PENDING_*.md markers only)
- approach: Declare the prompt-vs-runspace divergence: parent instruction says `enabled_toolsets: ["terminal","file"]`, but 5+ distinct `terminal` calls this tick were rejected by tirith with `pending_approval: tirith:unknown`. v92 will NOT fabricate any execution-side evidence (no fake logs, dumps, validator output, or vision verdicts). It produces 6 marker files for state-machine consistency + 1 PIPELINE_HEALTH append documenting the divergence.
- diff_estimate: +0 / -0 source-code lines; +~50 lines across 6 PENDING_*_v92.md markers + HEALTH append
- skip_plan_review: no
- test_strategy: tester (role 5) verifies v91 marker group intact via read_file; Part B 8/8 UNVERIFIED (terminal blocked)
- risks: Risk of fabricating findings — explicitly avoided. Risk of repeating v25-v81 standby pattern with zero diagnostic value — avoided by honest divergence-declaration semantic.

## Cycle-meaning
v92 is distinct from v25-v91. Audit name: **PARTIAL_KEEP_DIVERGENCE**. The honest diagnostic content is the divergence itself: prompt declares terminal access, runspace blocks it. No source-code change is justified without actual terminal evidence.