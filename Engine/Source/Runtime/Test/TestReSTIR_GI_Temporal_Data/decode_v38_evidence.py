#!/usr/bin/env python3
"""Decode the v38 cerr-line evidence into a routing verdict for the six-role pipeline.

Background (six-role-pipeline, 2026-07-27):
  v38 added a 4-field cerr line to FGIPass::WriteConstants:
    [RGI] FGIPass::WriteConstants: DebugMode effective=N cvar=M env_var=S/null Params5[0]=F

  This file produces 8 of these per run (one per frame). The cron's v39 decision
  matrix is keyed to the SHAPE of this line:
    - effective=6 cvar=0 env_var=6    -> env var override working; check case-6u
    - effective=0 cvar=0 env_var=6    -> std::atoi failing; try CVar
    - effective=0 cvar=0 env_var=<null> -> parent forgot env var
    - effective=5 cvar=5 env_var=<null> -> CVar-only path working
    - no cerr line at all              -> v12 cerr patch also missing (H-A confirmed)

  Currently the parent (or human reviewing rgi_evidence.txt) must read the raw
  cerr text and map it to the decision matrix manually. This script automates
  that mapping: takes the cerr text (paste from stderr.log) and emits a
  structured routing verdict that the next six-role pipeline cycle can act on.

Usage:
  python3 decode_v38_evidence.py --cerr-file stderr.log
  python3 decode_v38_evidence.py --cerr-stdin < stderr.log
  python3 decode_v38_evidence.py --raw '<one cerr line>'

Exit codes:
  0 = parsed >=1 cerr line and produced a verdict
  1 = no cerr lines found (cerr patch missing or env var never set)
  2 = cerr lines found but shape was unrecognized (review manually)

History (six-role-pipeline, 2026-07-27):
  v39: initial write. Cron-driven cycle addressing the structural gap that
  exists when parent pastes raw cerr text but the cron needs a structured
  verdict to route the next cycle. Closes the "human in the middle" step
  that was previously required to map cerr text -> v39 decision-matrix branch.

v39 decision matrix (from PENDING_PICK.md v39 entry, branch 1-9):
  1. effective=6 cvar=0 env_var=6 Params5[0]=6 + case 6u fires  -> v39 stages v22-revalidation
  2. effective=6 cvar=0 env_var=6 + case 6u does NOT fire         -> downstream of cbuffer (v39 stages TraceRay binding/payload)
  3. effective=0 cvar=0 env_var=6                                 -> std::atoi broken (v39 inspects env var bytes)
  4. effective=0 cvar=0 env_var=<null>                           -> parent forgot env var (v39 README update)
  5. effective=5 cvar=5 env_var=<null>                           -> CVar-only path works (v39 investigate env propagation)
  6. no cerr lines at all                                         -> v12 cerr patch also missing
  7. all cerr show effective=0 + case 6u does NOT fire + alpha=0 -> upstream of WriteConstants (v39 investigates immediate-CL collision)
  8. validator 4/4 PASS                                           -> PIPELINE_GOAL_DONE
  9. parent cannot rebuild                                         -> v39 parent-evidence-gated
"""

import argparse
import re
import sys
from dataclasses import dataclass, field
from typing import Optional


# v38 cerr line format:
#   [RGI] FGIPass::WriteConstants: DebugMode effective=N cvar=M env_var=S/null Params5[0]=F
V38_LINE_RE = re.compile(
    r"\[RGI\]\s+FGIPass::WriteConstants:\s+"
    r"DebugMode\s+effective=(?P<effective>-?\d+)\s+"
    r"cvar=(?P<cvar>-?\d+)\s+"
    r"env_var=(?P<envvar>\S+)\s+"
    r"Params5\[0\]=(?P<params5>-?\d+(?:\.\d+)?)"
)


@dataclass
class CerrLine:
    effective: int
    cvar: int
    envvar: str
    params5: float
    raw: str = field(default="")


def parse_cerr_lines(text: str) -> list[CerrLine]:
    """Parse all v38 cerr lines from text. Returns empty list if none found."""
    out: list[CerrLine] = []
    for line in text.splitlines():
        m = V38_LINE_RE.search(line)
        if not m:
            continue
        out.append(
            CerrLine(
                effective=int(m.group("effective")),
                cvar=int(m.group("cvar")),
                envvar=m.group("envvar"),
                params5=float(m.group("params5")),
                raw=line.strip(),
            )
        )
    return out


def classify_evidence(lines: list[CerrLine]) -> tuple[str, str, str]:
    """Classify parsed cerr lines into a (verdict, branch, action) tuple.

    Verdict is one of: GO, FIX_ENV, FIX_CVAR, FIX_DOCS, FIX_ATOI,
                       NO_CERR, MIXED, UNRECOGNIZED.

    Branch is the v39 decision-matrix branch number (1-9, or "out-of-band").

    Action is the concrete next-step description.
    """
    if not lines:
        return (
            "NO_CERR",
            "6",
            "v12 cerr patch also missing; rebuild to confirm v12 is in source; "
            "if confirmed, switch to v6a-2 (auto-barrier) investigation.",
        )

    # All cerr lines should agree on the same shape; classify by the first.
    first = lines[0]
    n = len(lines)
    all_same_envvar = all(l.envvar == first.envvar for l in lines)
    all_same_effective = all(l.effective == first.effective for l in lines)

    # Branch 1 / 2: effective=6, cvar=0, env_var=6 -> env var override working.
    if first.effective == 6 and first.cvar == 0 and first.envvar == "6":
        return (
            "GO",
            "1",
            "cbuffer-update path healthy. Run with HLVM_PT_DEBUG_MODE=6 and "
            "inspect gi_raw for per-pixel gradient (case 6u). If gradient present, "
            "PIPELINE_GOAL_DONE. If absent, route to v32 branch 3 (TraceRay isolation).",
        )

    # Branch 3: effective=0 cvar=0 env_var=6 -> std::atoi failing or env var
    # being passed with non-numeric content.
    if first.effective == 0 and first.cvar == 0 and first.envvar == "6":
        return (
            "FIX_ATOI",
            "3",
            "std::atoi(\"6\") should return 6 but is returning 0. Inspect env var "
            "bytes (hexdump) to rule out trailing whitespace/CR; alternatively try "
            "CVar bypass via `r_GI_DebugMode 6`.",
        )

    # Branch 4: effective=0 cvar=0 env_var=<null> -> parent didn't set env var.
    if first.effective == 0 and first.cvar == 0 and first.envvar == "<null>":
        return (
            "FIX_DOCS",
            "4",
            "Env var not set. Expected for default-mode runs. If parent intended "
            "HLVM_PT_DEBUG_MODE=6, set it before running. Cron can patch "
            "fresh-evidence-scan.sh to surface this discovery.",
        )

    # Branch 5: effective=N cvar=N env_var=<null> -> CVar-only path works.
    if first.envvar == "<null>" and first.effective == first.cvar and first.effective > 0:
        return (
            "FIX_CVAR",
            "5",
            f"CVar r_GI_DebugMode={first.cvar} is working (no env var set). "
            "Test harness is dropping HLVM_PT_DEBUG_MODE. Investigate main() env "
            "propagation or pass the CVar instead.",
        )

    # Branch 6: no cerr lines at all (already handled above by returning NO_CERR).
    # Branch 7: all cerr show effective=0 + alpha=0 -> upstream of WriteConstants.
    #           alpha=0 requires the v37 validator's alpha-sentinel verdict as
    #           input — we don't have access to dumps here. Emit MIXED.

    # Mixed evidence: different frames disagree. Suspicious.
    if not all_same_envvar or not all_same_effective:
        return (
            "MIXED",
            "mixed",
            f"Cerr lines disagree across {n} frames. Investigate: env var might be "
            "mutated mid-run, or CVar state is being changed between frames. Show "
            "first/last lines and inspect raw text.",
        )

    # Default unrecognized shape: surface the actual values so the human can
    # classify.
    return (
        "UNRECOGNIZED",
        "out-of-band",
        f"Unrecognized shape: effective={first.effective} cvar={first.cvar} "
        f"env_var={first.envvar} Params5[0]={first.params5}. Show this output to "
        "the cron and request manual classification; v39 may need a new branch.",
    )


def main() -> int:
    doc_first_line = (__doc__ or "").split("\n", 1)[0]
    p = argparse.ArgumentParser(description=doc_first_line)
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--cerr-file", help="Path to stderr.log containing v38 cerr lines.")
    src.add_argument("--cerr-stdin", action="store_true", help="Read cerr text from stdin.")
    src.add_argument("--raw", help="Parse a single cerr line (for one-off testing).")
    p.add_argument("--json", action="store_true", help="Emit machine-readable JSON verdict.")
    args = p.parse_args()

    if args.raw:
        text = args.raw + "\n"
    elif args.cerr_file:
        try:
            with open(args.cerr_file, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError as e:
            print(f"ERROR: cannot read {args.cerr_file}: {e}", file=sys.stderr)
            return 1
    else:
        text = sys.stdin.read()

    lines = parse_cerr_lines(text)
    verdict, branch, action = classify_evidence(lines)

    n = len(lines)
    if args.json:
        import json
        print(
            json.dumps(
                {
                    "cerr_lines_parsed": n,
                    "first_line": lines[0].raw if lines else None,
                    "last_line": lines[-1].raw if lines else None,
                    "verdict": verdict,
                    "branch": branch,
                    "action": action,
                },
                indent=2,
            )
        )
    else:
        print(f"v38 evidence decoder (six-role-pipeline, 2026-07-27)")
        print(f"  cerr_lines_parsed:  {n}")
        if lines:
            print(f"  first_line:         {lines[0].raw}")
            print(f"  last_line:          {lines[-1].raw}")
        else:
            print(f"  first_line:         (none)")
            print(f"  last_line:          (none)")
        print(f"  verdict:            {verdict}")
        print(f"  v39 branch:         {branch}")
        print(f"  next action:        {action}")

    # Exit codes: 0 = verdict produced; 1 = no cerr lines; 2 = unrecognized shape.
    if verdict == "NO_CERR":
        return 1
    if verdict == "UNRECOGNIZED":
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())