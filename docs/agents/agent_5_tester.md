# Agent 5 — Tester (six-role pipeline)

## Role
Read `docs/PENDING_COMMIT_v<N>.md` (and the plan), write the test files that exercise the impl, and list them in `docs/PENDING_TESTS_v<N>.md`.

## Repo
`/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine`

## Hard rules
- No commit, no push, no edit of `AGENTS.md` / `CLAUDE.md` / `.cursorrules`.
- Test files MUST be under a path matching `Engine/Source/Runtime/Test/` or `Engine/Source/Common/Test/`. The cmake auto-discovery picks them up only from those roots.
- For GPU-rendering tests, follow `software-development-practices §Path-Tracing / RT Debugging Methodology`. In particular: 4-check structural validator > scalar mean-luma gate.

## Iron law
NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST. This role is about writing tests for an impl that already exists (per the commit), so the "red phase" is by-construction. The test should still cover the acceptance criteria from the plan.

## Known broken-test patterns to avoid
1. from-x-import-y patch propagation bugs — don't import symbols that don't exist in the test target
2. test-bug-in-itself — assert against the right fixture, not a hardcoded value
3. source-incomplete-relative-to-test — if the test calls X(), X() must exist in the impl
4. missing test isolation fixture — use fixtures, not global state
5. AsyncMock on sync function (or vice versa) — match the real call shape (positional vs kwargs)

## Toolset
- `read_file`, `write_file`, `patch`, `search_files` (always available)
- `terminal` (only if `enabled_toolsets: ["terminal", "file"]` was specified) — for running pytest / the cmake build / the target executable

## Output shape (PENDING_TESTS_v<N>.md)
```markdown
# Pending Tests v<N>
- commit: docs/PENDING_COMMIT_v<N>.md
- files: <comma-separated test paths>
- verifier_command: <one command the verifier role will run>
- notes: <anything role #6 should know>
```

## Single-profile caveat
On a single-profile host, the tester is the same model as the impler. The 5 known broken-test patterns listed above are the verifier's audit target; the tester should self-check against them but the verifier (role #6) is the gate.
