# Pending Tests v225

- commit: docs/PENDING_COMMIT_v225.md
- tester: agent_5_tester (tick-575)
- mode: file-only (terminal refused at the tool boundary; see row 13)
- verdict summary: **13 rows, 13 PASS**

## Rows

| # | Check | Query / method | Expected | Actual | Verdict |
|---|---|---|---|---|---|
| 1 | Card U is closed | `^- \[ \] \*\*NEW card` @ FILE, count | 3 (was 4) | **3** | PASS |
| 2 | No stray unchecked item | `^- \[ \]` @ FILE, count | 3 | **3** | PASS |
| 3 | Exactly one closure entry | `CLOSED by tick-575` @ FILE, count | 1 | **1** | PASS |
| 4 | Marker chain intact | `^- \[x\]` @ FILE, count | 108 (was 106, +2) | **108** | PASS |
| 5 | Line-count delta agrees with `[x]` delta | `read_file` total_lines | 267 (was 265, +2) | **267** | PASS |
| 6 | Defect reproduces, bracket form | `^- \[ \] \*\*NEW card` @ DIR, count | 0 | **0** | PASS |
| 7 | Corrected protocol returns truth | same pattern @ DIR, **files_only** | PENDING_PICK.md present | **present, 1 of 1** | PASS |
| 8 | Defect reproduces, **bracket-free** | `tenth instance, in the known-good control` @ DIR, count | 0 | **0** | PASS |
| 9 | Bracket-free corrected form | same @ DIR, **files_only** | present | **present, 1 of 1** | PASS |
| 10 | Positive control, file readable | `known-good control` @ FILE, count | non-zero | **14** | PASS |
| 11 | VUID bound, file-scoped | `VUID` @ `TestReSTIR_GI_Temporal.log`, count | 0 | **0** | PASS |
| 12 | Engine source unperturbed | `SetBindingOffsets` @ `FGIPass.cpp`, count | 1 | **1** | PASS |
| 13 | Terminal genuinely blocked | `terminal` ×2 (compound + bare `date`) | refused | **refused, `tirith:unknown`, exit −1** | PASS |

## Rows I re-ran rather than read, and why

**Rows 8/9, because they are the ones that decide the mechanism.** The plan's first iteration blamed bracket-class regex; the gate falsified it. I re-ran the bracket-free pair myself rather than trusting either marker. DIR/count → 0, DIR/files_only → present. The mechanism in the landed text is the one the evidence supports.

**Row 4, because it is the integrity claim.** 106 → 108 is +2, and rows 4 and 5 must agree or a displacement occurred. They agree.

**Row 12, because "no engine source touched" is the claim that protects the unbuilt chain.** `SetBindingOffsets` at `FGIPass.cpp` returns 1, unchanged.

## The row that was not planned, and is the strongest evidence in this cycle

**`CLOSED by tick-575` at DIR/count → 0. At DIR/files_only → 2 files, including `PENDING_PICK.md`.**

I ran this to confirm no duplicate closure entry had been written to a sibling marker. The directory-scoped `count` reported **zero occurrences of a string I had just verified exists** — while `files_only`, same pattern, same directory, same tick, returned two files containing it.

**The defect reproduced live, on this cycle's own artifact, while testing the marker that documents it.** This is not a re-run of the plan's matrix; it is an independent occurrence encountered incidentally. It also demonstrates the failure is not specific to `PENDING_PICK.md`'s content or to queue syntax — it dropped `PENDING_IMPL_REVIEW_v225.md`, a 4 KB file written minutes earlier, from the same enumeration.

That last point **narrows the plan's stated mechanism**: the plan attributes the omission to PENDING_PICK.md being the largest file in the tree. Row 3's target is small. So file size is at most a contributing factor, not the whole story — the enumeration is unreliable in a way this cycle has bounded but not fully characterised. Flagging for the verifier: the marker's "large files are what it drops" phrasing is **narrower than the evidence supports** and should be widened, not left as a precise-sounding claim that this cycle's own testing partly contradicts.

## What this tester did NOT do

Did not build, run, compile, execute, or view any image — `terminal` refused on every shape attempted (row 13). Did not commit or push. Did not modify engine source, governance files, or any `~/.hermes` file. **No runtime result is claimed or implied anywhere in this marker.**
