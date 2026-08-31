# Pending Impl Review v207

- plan: docs/PENDING_PLAN_v207.md
- commit: docs/PENDING_COMMIT_v207.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-553)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl is what the revised plan specified: a ternary onto `Desc.OutputTexture`,
one unconditional `setTextureState`, both HLSL copies untouched, the header
contract phrased as a requirement on callers. **`## Plan Deviations` declares
none, and I agree** — I compared the applied hunk against the plan's `FINAL`
paragraph line by line and found no substitution, no widening, and no absorbed
scope. The three "checked rather than assumed" items the impler recorded are all
things the plan explicitly asked to be re-derived.

## Independent verification of the applied patch

Re-derived from the file, not from the commit marker:

1. **All three UAV bindings survive.** `SetTextureUAV` → **3** hits: `(0,
   Desc.OutputTexture)` `:603`, `(1, DebugStatsUAV)` `:631`, `(2, DirectionUAV)`
   `:650`. **Layout↔set pairing holds N-for-N** against `AddTextureUAV(0/1/2)` at
   `:314-316`. This is the check v203's near-miss makes mandatory on any patch
   anchored near a binding block — and this patch deleted a 25-line `if/else`
   directly above `SetTextureUAV(2)`, so it was the realistic failure mode.
2. **The u1 block is byte-intact** — `:605-631` read in place, lazy creation,
   `debugName "FGIPass.DummyDebugStats"`, and its own `setTextureState` all
   present. The patch did not bleed upward.
3. **`DummyDirection` → 1 hit**, the `Shutdown` null-out at `:192`. The creation
   block is gone; nothing assigns it. Consistent with the impler's dead-member
   disclosure, and confirms the deletion was complete rather than partial.

## The one thing the plan did NOT check, which I checked

The new ternary yields `Desc.OutputTexture` on the fallback path and hands it
straight to `setTextureState` — **a null there would be a null-deref on exactly
the consumer the fix targets**, which would have converted a silent OOB store
into a crash. It cannot happen: `:530` early-returns when
`!Desc.SceneTLAS || !Desc.OutputTexture || !Desc.ViewConstants`, 115 lines above
the ternary, in the same function, with no intervening reassignment. **The
mandatory-ness the fix rests on is enforced in code, not merely documented** —
which is a stronger footing than the header comment alone provides, and worth
recording because the comment is what future readers will find first.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection — no shell surface in this diff
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] Validation: fallback operand proven non-null by an early return in scope
- [x] Error handling: unchanged; no new failure path introduced
- [x] Tests: none produced (`produces_test_files: no`), so HARD INVARIANT #2 does
      not require anything further of this gate
- [x] Diff read in full and matched against intent, both files

## On the LSP diagnostics

The impler's dismissal is **correct and correctly reasoned**, and I re-derived it
rather than accepting it. All 9 reported lines map to the pre-edit numbering
(reported `:38`/`:39` = `nvrhi::BufferHandle LightsBuffer` / `uint32_t
LightCount`, now at `:52`/`:53` after 14 inserted comment lines). The errors are
`undeclared identifier 'nvrhi'` and `unknown type 'uint32_t'` — **include
resolution failing in an isolated-header context**, which a comment insertion
cannot cause and cannot fix. Recording the reasoning matters more than the
conclusion here: "the linter complained and I decided it was fine" is exactly the
shape of dismissal that hides a real error, so the mapping is written down.

## Assessment of the finding itself

This is the **twelfth** instance of the extent class and the first in a UAV
**write**. Every prior instance was a wrong-operand read or a dispatch/resource
extent mismatch. **The class is broader than eleven cycles assumed**: it also
covers *optional resources whose fallback is not sized to the dispatch*. No
query shape used in this lineage would surface it — there is no wrong token; the
defect is a **1x1 allocation that is correct for a guarded write and wrong for an
unguarded one**, and it sits 25 lines below a structurally identical block that
is correct. It was found by enumerating the shader's stores while answering a
question about its *reads*.

**Severity, stated honestly.** On the acceptance path this is a **no-op**: the
primary target supplies `OutputDirection`, so the fallback branch never executed
there. The defect was live only in `TestPathTraceGI` — **the known-good control**,
whose entire value is exonerating driver/nvrhi/slangc/binding-layer for
twenty-five unbuilt cycles. An unguarded OOB UAV store is undefined behaviour and
raises no VUID, so the control could have been quietly unsound this whole time
while being cited as clean. That is why this matters despite touching no
rendered pixel in the primary.

## Feedback for impler

None. Proceed to tester.
