# Pending Commit v224

- plan: docs/PENDING_PLAN_v224.md
- files: docs/PENDING_COMMIT_v214.md
- source: no bundle — direct edit of one marker file
- target: no branch; nothing committed
- task: Supersede the stale `verify:` line at `docs/PENDING_COMMIT_v214.md:10` (card T). Original audit text above the inserted section is byte-identical to v214's commit; the inserted `## Verify-line correction` section documents why the original verify line produces a false failure on a correct tree and provides a worked-example verify an operator can paste.
- verify: `read_file docs/PENDING_COMMIT_v214.md` → line 13 is `- notes: per v203's standing rule, ...` (the restored original); line 15 is `## Verify-line correction (superseded by tick-574's v224 cycle)` (the inserted section); the rest of the file is byte-identical to pre-patch.
- skip_impl_review: yes
- produces_test_files: no
- notes: **NO engine source touched. NO marker chain integrity violated.** The patch tool's first attempt at this edit (with `old_string` matching against `## Plan Deviations` and table-end line) deleted the load-bearing `notes:` line at the original line 13 due to fuzzy matching; the second patch restored it. Final state has `notes:` at line 13 byte-identical to original, `## Verify-line correction` section inserted at lines 15-34, and original lines 15+ shifted down by 20 lines. **The first-attempt near-miss is exactly the kind of event v203's standing rule guards against** (anchoring on a statement boundary, never on a comment adjacent to an initialiser) — except here the initialiser was a markdown table row, not a C++ brace list. The lesson generalises: anchor `old_string` on the smallest unique substring, and verify the returned diff is byte-equal to the planned edit before declaring the cycle done.

## What the patch did NOT do
- Did NOT modify `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` or any source file.
- Did NOT modify `docs/PENDING_PLAN_v214.md`, `docs/PENDING_PLAN_REVIEW_v214.md`, `docs/PENDING_TESTS_v214.md`, or `docs/PENDING_TEST_AUDIT_v214.md`.
- Did NOT change v214's verdict (KEEP) or its audit trail integrity — the original text above line 15 is byte-identical to v214's commit.
- Did NOT modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file.

## Plan Deviations

The patch was attempted twice. First attempt with `old_string` matching against `## Plan Deviations (impler fills this in if it deviated)` plus the table-end line at original line 27 — the patch tool's fuzzy matcher displaced the `notes:` line at original line 13 with the table-end line (a markdown table-row fuzzy match I did not anticipate). Second attempt with `old_string` matching against the displaced line content restored the `notes:` line byte-exact. **Final state: original lines 1-14 (header + `notes:`) and original lines 15+ (sections) are byte-identical to pre-patch; only lines 15-34 are new.** The plan called for "anchor on `## Plan Deviations`" which is what I did; the deviation is that the tool's fuzzy matching engaged unexpectedly and required a restoration patch.

**Standing rule, generalised from v203**: anchor `old_string` on the smallest unique substring that contains the line(s) you want to insert AFTER. Do not include lines from elsewhere in the file in your `old_string` even if they look like a natural pair — the fuzzy matcher can match a structurally similar but semantically wrong block, and the diff will appear clean.