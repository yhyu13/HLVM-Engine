# Pending Plan v220

- task: Close v219's one remaining open link — decide, from source alone, WHICH of candidates (A)/(B)
  produces the `terminal` refusal, and derive an operator remedy that is reachable under both.
- source: `/home/hangyu5/Documents/Gitrepo-My/hermes-agent` (read-only) + `~/.hermes/config.yaml`
- approach: v219 established that control reaches `approval.py:2999` and named two candidates it could
  not separate, on the ground that separating them requires reading the process environment via the
  very terminal under investigation. **That ground is wrong, and this cycle's whole value is showing
  why.** `tools/approval.py` contains TWO sibling approval entry points that gate on the same three
  variables — `check_dangerous_command` (`:2692-2700`) and `check_code_execution` (`:3117-3121`). They
  order the operands DIFFERENTLY. If the difference is real, it discriminates the candidates without
  any environment read, because one of the two functions is reachable in a state the other is not.
- diff_estimate: +0 / -0 engine source. Markers + health doc only.
- skip_plan_review: no
- test_strategy: role #5 re-derives every load-bearing line with `read_file` at an explicit offset
  (not `search_files`), given that this lineage has now found THREE distinct `search_files` false-zero
  classes (alternation, project-root scoping, dotfile directories). Each zero needs a same-shape,
  same-scope positive control.
- risks:
  - The two functions may be an artifact of reading partial context. Both must be read as contiguous
    line ranges, not via grep hits, before any claim rests on the ordering.
  - The remedy must NOT be "patch hermes-agent". A cron job must not modify the agent executing it.
    Only config-level or operator-side remedies are admissible.
  - v219's Finding 2 (`tirith` is installed) must be carried forward, not silently re-litigated.
