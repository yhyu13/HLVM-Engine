# HLVM Test Guidelines — SDD TDD compliance

**This document is the policy that AI agents and humans writing tests in HLVM must follow.**
Dated 2026-09-01. Phase 1 of the four-phase autonomous run.

## The problem we are fixing

The HLVM test framework (`Engine/Source/Common/Test/Test.h`) registered tests and ran them, but it had **no real assertion primitives**. Tests called `HLVM_ENSURE(...)` thinking it was an assertion — it is not; it is a structured log macro that returns void. A test that printed "passed" at the end was green even when every `HLVM_ENSURE` had silently failed. Tests that didn't crash but were wrong reported success.

**That meant: behavior that did not crash but was wrong was invisible to the test suite.**

## What changed

1. New assertion macros in `Test.h`:
   - `HLVM_TEST_EXPECT(cond)` — the core macro
   - `HLVM_TEST_EXPECT_TRUE(x)`, `HLVM_TEST_EXPECT_FALSE(x)`
   - `HLVM_TEST_EXPECT_EQ(a, b)`, `HLVM_TEST_EXPECT_NE(a, b)`
   - `HLVM_TEST_EXPECT_LT(a, b)`, `HLVM_TEST_EXPECT_LE(a, b)`, `HLVM_TEST_EXPECT_GT(a, b)`, `HLVM_TEST_EXPECT_GE(a, b)`
   - `HLVM_TEST_EXPECT_NEAR(a, b, eps)` — float near-equality
2. Each failure logs a structured `error` line with the current test name + file:line + the condition expression.
3. The wrapper counts failures per test and across the run.
4. **The process exits non-zero (`return 1`) if any assertion failed.** CI/scripts can now detect failure.
5. `RECORD_BOOL`/`RECORD_INT` still accept the old style (return bool/int), but now ALSO fail if any `HLVM_TEST_EXPECT*` triggered inside.

## The rule

**Use `HLVM_TEST_EXPECT*` for assertions. Use `HLVM_ENSURE` for production invariants only.**

- `HLVM_ENSURE(x)` — internal invariant in production code. Compiled out only if explicitly disabled. Failure = log only.
- `HLVM_TEST_EXPECT_TRUE(x)` — test assertion. Failure = log + counted + process exits non-zero.

**Do not mix them.** A test that mixes `HLVM_ENSURE` (silent) and `HLVM_TEST_EXPECT_TRUE` (counted) will report false success on the `HLVM_ENSURE` line — that's the bug we are eliminating.

## How to write a new test

```cpp
#include "Test.h"

DECLARE_LOG_CATEGORY(LogMyTest)

RECORD(my_feature_behavioral_test)
{
    // Arrange
    auto thing = MakeThing();

    // Act
    auto result = thing.DoSomething();

    // Assert (RED → GREEN → REFACTOR loop, one assertion at a time)
    HLVM_TEST_EXPECT_TRUE(result.IsValid());
    HLVM_TEST_EXPECT_EQ(result.GetValue(), 42);
    HLVM_TEST_EXPECT_NEAR(result.GetFloat(), 3.14f, 1e-5f);
}
```

## Anti-patterns (do not do these)

- `HLVM_ENSURE(x == y)` inside a test — silent failure; reports green.
- `if (...) HLVM_LOG(info, "passed"); return;` — the `if` is decorative; the test always passes.
- Snapshot tests that capture their own output and compare to itself — can never disagree.
- Tests that exercise private state via friends/`#define private public`.
- Tests that depend on real wall-clock time, randomness without seeding, or external network.

## Properties to prefer

For invariants like "deserialize(serialize(x)) == x", prefer property tests:
```cpp
RECORD(string_format_roundtrip)
{
    for (const auto& input : {"a", "hello world", "{}", "{0}{1}", ""})
    {
        HLVM_TEST_EXPECT_EQ(FString::Format(TXT("{}"), TXT(input)), FString(input));
    }
}
```

## Verification rule (the law)

Every test you write must be **breakable**: after you see it green, change one input or operator and confirm it goes red. A test you cannot make red is not testing anything.

## Migration status

- ✅ `Engine/Source/Common/Test/Test.h` — assertion primitives added, summary exit code added.
- ✅ `Engine/Source/Common/Test/TestString.cpp` — `test_name` migrated; `test_string` benchmark got a baseline assertion.
- ✅ `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` — all 78 `HLVM_ENSURE`/`HLVM_ENSURE_F` migrated to `HLVM_TEST_EXPECT_*`. Copy-paste duplicate block removed.
- ⏸ The remaining 54 test files use `HLVM_ENSURE` extensively. Migration is **opt-in per file**. New tests must use `HLVM_TEST_EXPECT*`.

## How to migrate a file

```bash
# 1. Read the file
# 2. Find all HLVM_ENSURE / HLVM_ENSURE_F calls
grep -n "HLVM_ENSURE" path/to/TestX.cpp
# 3. Replace each with the matching HLVM_TEST_EXPECT_* macro.
#    HLVM_ENSURE(a == b)        → HLVM_TEST_EXPECT_EQ(a, b)
#    HLVM_ENSURE(a != b)        → HLVM_TEST_EXPECT_NE(a, b)
#    HLVM_ENSURE_F(cond, msg)   → HLVM_TEST_EXPECT_TRUE(cond)  (msg is dropped; new macro logs file:line)
# 4. Build:  ./Build.sh --Config=Debug --Target=TestX --Test
# 5. Run:    ./Binary/Debug/TestX  — confirm "TEST SUITE PASSED" at the end.
```

## What "passing tests" means now

A passing test is one where **0 `HLVM_TEST_EXPECT_*` macros failed**, not one where nothing crashed. The exit code is the source of truth; the log is the human-friendly version.