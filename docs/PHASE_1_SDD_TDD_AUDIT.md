# Phase 1 — Fix non-SDD TDD

## Audit findings (2026-09-01)

57 test files (42 Runtime, 15 Common), 32,431 lines.

**Top issue — the test framework itself is broken.** `Engine/Source/Common/Test/Test.h` has:
- `RECORD`/`RECORD_BOOL`/`RECORD_INT` registration (good — auto-discovery)
- `SECTION` for grouping (good)
- `RunTestAndCalculateAvg` benchmark helper (good)
- **NO assertion primitives** — only `HLVM_ENSURE` (which is a log, not an assert)
- The "test passed" signal is `return true` from `RECORD_BOOL` — but most tests return void, so even on `HLVM_ENSURE` failure, the test reports success because nothing was checked

## Concrete violations (spot-checked)

| File | Issue |
|---|---|
| `TestString.cpp` | Pure benchmark, no assertions at all. "TestString" = log "passed" → green. Useless. |
| `TestName.cpp` (line 125) | Only HLVM_LOG; never asserts ref-count or hash behavior. |
| `TestSceneGraphNode.cpp:91-97` | Triple-pasted assertion block — copy-paste smell, lines 92/95 are identical to 84/85. Dead code. |
| `TestSceneGraphNode.cpp:91-97` | All asserts use `HLVM_ENSURE` which is **not** an assertion — it's a log that returns void. So even on failure, the test "passes". |
| `TestTaskFlow.cpp` (1177 lines) | Mostly manual scaffolding, likely the same — need to spot-check. |
| All test files | The pattern: "run code that could crash, log 'passed', exit 0". Crash = test fail. Pass = silent. This means: behavior that doesn't crash but is wrong = test "passes". |

## The fix — minimal, structural

### Step 1: Add assertion primitives to `Test.h`

```cpp
// New: real assertion macros that count failures
#define HLVM_TEST_EXPECT(cond) \
    do { if (!(cond)) { \
        ++hlvm_private::g_test_failure_count; \
        HLVM_LOG(LogTemp, error, TXT("EXPECT FAILED at {}:{}: {}"), TXT(__FILE__), __LINE__, TXT(#cond)); \
    } } while(0)

#define HLVM_TEST_EXPECT_EQ(a, b)  HLVM_TEST_EXPECT((a) == (b))
#define HLVM_TEST_EXPECT_NE(a, b)  HLVM_TEST_EXPECT((a) != (b))
#define HLVM_TEST_EXPECT_TRUE(x)   HLVM_TEST_EXPECT((x))
#define HLVM_TEST_EXPECT_FALSE(x)  HLVM_TEST_EXPECT(!(x))
#define HLVM_TEST_EXPECT_NEAR(a, b, eps) HLVM_TEST_EXPECT(std::abs((a)-(b)) <= (eps))
```

Then change `RECORD_BOOL` to auto-return `g_test_failure_count == 0`, and print a summary on exit.

### Step 2: Migrate top-N worst tests

Order by visibility (the tests an AI agent will read first to learn the codebase):
1. `TestSceneGraphNode.cpp` — fix the copy-paste, swap `HLVM_ENSURE` → `HLVM_TEST_EXPECT_TRUE`
2. `TestString.cpp` — add at least one real assertion (e.g. `EXPECT_EQ(FString::Format("{0}", 1), "1")`)
3. `TestName.cpp` — assert ref-count and name equality after copy

### Step 3: Add `DOCS/TEST_GUIDELINES.md`

The policy: every new test uses `HLVM_TEST_EXPECT*` not `HLVM_ENSURE`. `HLVM_ENSURE` is for internal invariants in non-test code.

## Scope boundary (this phase)

- Modify: `Engine/Source/Common/Test/Test.h` (add assertion macros)
- Modify: 3 test files (top violations)
- Create: `docs/TEST_GUIDELINES.md`
- Do NOT modify: other test files (preserves coverage; framework migration is opt-in)
- Do NOT modify: build files / `RECORD` registration mechanism
- Do NOT modify: any non-test code

## Success criteria

1. `Test.h` exposes `HLVM_TEST_EXPECT*` macros that fail loudly and count failures.
2. The summary at end of main prints "X failures across Y tests".
3. At least 3 high-visibility tests migrated from `HLVM_ENSURE` (silent) → `HLVM_TEST_EXPECT_TRUE` (counted).
4. Copy-paste duplicates removed in `TestSceneGraphNode.cpp`.
5. `docs/TEST_GUIDELINES.md` documents the policy.
6. Build of at least 1 migrated test still passes (no regression).