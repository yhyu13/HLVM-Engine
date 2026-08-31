# Pending Impl Review v208

- plan: docs/PENDING_PLAN_v208.md
- commit: docs/PENDING_COMMIT_v208.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-554)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly: zero source files modified, all six delta
cycles covered, the cbuffer row checked in four expressions rather than two. The
declared deviations section says "None" and that is accurate — the plan asked for
the false-zero mechanism in its risks section and the impler delivered it as the
primary output, which is scope *fulfilment*, not creep. No design change, so no
planner sign-off is needed.

## The load-bearing claim, independently re-derived

A wrong diagnosis here is worse than no diagnosis: it would be written into the
audit checklist and steer every future query in the lineage. So I did not accept
the impler's evidence. I tested **two shapes the impler never used**, in a
different file:

| Pattern | Hits | Reads as |
|---|---:|---|
| `Texture\{1,\}` | **60** | BRE interval — metacharacter when escaped |
| `Texture{1,}` | **0** | ERE interval — literal, matches nothing |
| `Dummy\(Direction\|DebugStats\)Texture` | **5** | escaped **grouping + alternation** together |

The third row is the strongest single piece of evidence in this cycle and neither
the plan nor the impler produced it: it exercises `\(`, `\|` and `\)`
simultaneously and returns a correct non-zero. Under the impler's hypothesis this
must work; under any "the tool is broken / strips punctuation / mishandles
alternation" hypothesis it cannot. **Confirmed: POSIX BRE, behaving correctly and
per spec. The tool was never unsound — 28 ticks of usage was.**

That reframing matters and the impler stated it correctly: this is not a tool bug
to route around, it is an operator error to stop making.

## Security scan

- [x] No hardcoded secrets — no source modified
- [x] No shell injection — no shell reachable
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] **Validation**: every negative in the commit is paired with a same-shape
      positive control; I re-derived the two most load-bearing myself.
- [x] **Error handling**: the `grep: Unmatched ( or \(` error string is correctly
      treated as *evidence* rather than a failed query. Per v207's new row 20,
      tool output that looks like a failure must be mapped to a cause — here the
      cause is the diagnosis itself.
- [x] **Tests**: `produces_test_files: no`, and the reviewer ran regardless
      (`skip_impl_review: no`) per HARD INVARIANT #2.

## Where I push back

**The commit's claim in consequence 3 needs one qualification it does not state.**
It says a zero is sound iff the pattern contains no unescaped ERE metacharacter.
True, but the practical reading matters: patterns like `^- \[ \]` (used this tick
to size the PICK queue) contain `[` and `]`, which are metacharacters in **both**
dialects and behave identically, so that query is sound. The rule should be
stated over the *divergent* set — `| + ? ( ) { }` — not "metacharacters"
generally, or a future tick will re-audit sound bracket queries for no reason.
Not a FIX: the commit's table lists exactly the divergent set, so the intent is
unambiguous; the tester should record the sharper phrasing.

**On severity, stated without inflation.** The audit half of this cycle found
nothing broken — the delta is coherent and v203's near-miss restore is intact.
That is a genuine negative result and worth having, since the operator's build was
the one thing gating cards L/M/N. The mechanism half is the cycle's real value:
it retires four catalogued "tool unsoundness" rows, corrects a standing rule that
was actively wrong, and makes the soundness of every recorded zero decidable
instead of doubtful. **It moves no pixel and clears no acceptance gate.**

## Feedback for impler

None. KEEP.
