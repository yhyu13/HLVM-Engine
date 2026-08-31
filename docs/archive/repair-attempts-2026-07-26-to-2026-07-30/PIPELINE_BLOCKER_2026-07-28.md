# Pipeline Blocker — 2026-07-28 (v82 escalation)

## What this document is

The six-role pipeline ran 63 consecutive file-only standby ticks (v25-v81)
between 2026-07-27 and 2026-07-28. Every tick wrote the standard 6
`PENDING_*_v<N>.md` markers, re-verified the cumulative 22-patch inventory,
and produced KEEP / ALL_KEEP verdicts. **No tick modified any source file**.
The goal gate (parent terminal verification: rebuild + run + validate + vision)
is permanently UNVERIFIED because `terminal` access is structurally blocked
in this cron's runspace (every probe — `pwd`, `ls`, `date`, `echo` — is
rejected with `pending_approval: tirith:unknown`).

This document is a one-shot escalation. It does **not** propose a fix; the
22-patch inventory is intact and waiting. It does **not** request new
permissions; the cron prompt's `enabled_toolsets: ["terminal", "file"]`
override cannot be re-realized inside a single tick. It asks the parent
(terminal-equipped human or interactive session) to run **one** bash
command and **one** Python invocation, then paste the output back.

If the parent supplies the two pieces of evidence below, the very next
cron tick can either (a) move the goal gate to PASS and write
`docs/PIPELINE_GOAL_DONE_2026-07-28.md`, or (b) name the precise residual
defect the evidence reveals, route to a FIX cycle, and close the bug within
2-4 ticks.

If the parent cannot run the evidence commands now, the cron pipeline
should be paused rather than continuing the v25-v81 standby loop. Further
file-only standby ticks will produce no new diagnostic value.

## Minimum parent actions (4 commands, ~2 minutes wall-clock on a rebuilt tree)

### 1. Confirm patch state is ready to rebuild (1 second)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh
```

**Expected banner on a healthy tree** (script v43):
`BANNER: source-patch-missing (MISSING=N)` where N is 0 (cumulative
22 patches all present), followed by `evidence-stale-or-missing` if the
2026-07-27 dumps are still the newest (they are: mtime 2026-07-27 00:07).
**Action**: paste the full stdout back to the cron. No `rm`, no `mv` —
the script is read-only.

### 2. Rebuild Debug target against the patched source (1-2 minutes)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
```

**Expected output**: successful link of `Binary/Debug/TestReSTIR_GI_Temporal`
with no `-Werror` failures. If `level::warning` vs `level::warn` cascade
fires (per software-development-practices § `-Werror` cascade recipe),
`grep -nH 'level::warning\|level::critical' Engine/Source/Runtime/Private/Renderer/GI/*.cpp`
**before** patching — the cascade can touch 2-3 files.

### 3. Run with frame dumps + capture stderr (10 seconds)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal \
  2>TestReSTIR_GI_Temporal_stderr.log
```

**Expected stdout tail** (from a healthy run):
```
Dumped display (.../dumps/<timestamp>_display_frame8.png)
Dumped spatial (.../dumps/<timestamp>_spatial_frame8.png)
Dumped denoised (.../dumps/<timestamp>_denoised_frame8.png)
DumpRGBA32FTexture: gi_raw normalized per-channel — R[min,max] G[min,max] B[min,max]
Dumped gi_raw (.../dumps/<timestamp>_gi_raw_frame8.png)
DumpRGBA32FTexture: gbuffer_worldpos normalized per-channel — R[-15.228,15.264] G[-11.811,8.193] B[-14.291,0.025]
```

**Smoking gun to look for**: `gi_raw normalized per-channel — R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]`
(all-zero gi_raw is the user's "broken visual" symptom from the 2026-07-27 log
at TestReSTIR_GI_Temporal.cpp:1712).

**Smoking gun to look for in stderr**:
- `VUID-VkDescriptorImageInfo-imageLayout-00344` (per gpu-rendering-bisect-debug
  reference `nvrhi-deferred-barrier-ordering.md`) — if it fires 0 times
  on this build, the v22 binding-layout-split patch took effect.
- `A command list should be executed before it is reopened` (from
  `DeviceManager.cpp:52`) — if it fires 0 times, bug-088 closed.

### 4. Validate the fresh dumps (1 second)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

**Expected on a healthy fix**:
```
4/4 checks PASSED
```

If it returns non-zero, paste the **per-check verdict line** (each check
prints PASS/FAIL plus diagnostics). The four checks distinguish:

| Check | Verdict meaning |
|-------|----------------|
| non_black_channel_mean > 5.0 (any ch) | "Test emits black frames" fail mode |
| spatial_std > 30 | "Uniform-bright but flat" fail mode (rejects calibration gray) |
| cell_variance > 8 | "Uniform with noise" fail mode (4x4 cell-mean std > 8) |
| alpha_sentinel PASS | v28 dispatcher-body sentinel reached (alpha > 254 in >= 95%) |

### 5. (Optional, recommended) Vision-check the display PNG

Open the freshly dumped `display_frame8.png`. **Don't trust the validator
PASS until a human sees the image.** Per the gpu-rendering-bisect-debug
skill § "Distrust scalar gates until a human sees the image":

- **Good output**: recognizable non-uniform Sponza geometry with sane exposure.
  Walls at different brightness, floor/ceiling distinguishable.
- **Bad output (gi_raw=0 symptom)**: uniform black or uniform near-zero gray.
  Validates a "PASS" if the gate is too lenient (this is the failure mode
  the 2026-07-25 ReSTIR GI session documented).
- **Bad output (sentinel-mask symptom)**: uniform magenta (or other garbage
  color the dump-normalization anti-pattern can also cause).

If `validate_restir_gi.py` says PASS but the image looks wrong, **don't
trust PASS** — the gate is broken or the symptom is masked (per
gpu-rendering-bisect-debug anti-pattern #5 — workaround-masked
root-causes).

## What the cron will do with the four pieces of evidence

| Evidence shape | Cron action |
|----------------|------------|
| `fresh-evidence-scan.sh` → `MISSING>0` | re-apply the named missing patches; v83 routes to impler for a sourced-bundle patch landing |
| `fresh-evidence-scan.sh` → `MISSING=0` + non-zero grep | pass to step 2 outcome |
| Build fails on `-Werror` cascade | grep cascade recipe, patch all sites, rebuild; v83 routes to impler |
| `gi_raw R[0,0]` persists | route to v82-equivalent FIX cycle (planner → plan-criticer → impler on a single pinned probe) — likely bug-118-class successor |
| `gi_raw` non-zero + validator 4/4 + vision OK | write `PIPELINE_GOAL_DONE_2026-07-28.md`; mark PICK all `[x]`; cron pipeline complete |
| `gi_raw` non-zero but validator fails 1 of 4 | route by which check failed: alpha=0 → bug is upstream of dispatch body; cell_variance → renderer uniform-mask; spatial_std → renderer tone-mapping |
| `gi_raw` non-zero + validator 4/4 + vision shows magenta | bug is the same as anti-pattern #6/7 (dump-normalization or sentinel-mask) — re-inspect `WriteGBufferSentinels` removal at commit `e6b3d52` |

## Honest assessment

The 63 consecutive standby ticks have burned wall-clock cron time and
parent review attention without producing one new piece of evidence.
Per the gpu-rendering-bisect-debug skill's

> "`Full auto` for GPU repair is a 6-role pipeline, not a Kanban card"
> (verified 2026-07-26)

the pipeline is the right shape when the parent has at least one
terminal probe. Without it, the pipeline is **a queue with markers**,
not a pipeline. Continuing the v25-v81 pattern for v83-vN would not
change the diagnosis above. The honest action is to escalate to the
parent with the minimum four-command recipe, and to mark the cron
"awaiting parent evidence" until the parent supplies it.

## Required parent reply shape

The parent should reply to the cron with **either**:

1. The output of all four commands in this document, OR
2. A timestamp + commitment to run them in the next interactive session
   (the cron will then write `docs/PIPELINE_AWAITING_PARENT_<date>.md`
   and tick down to a 30-minute cadence until evidence arrives).

If neither arrives within 24 hours, the cron should self-pause (write
`docs/PIPELINE_PAUSED_<date>.md` and exit [SILENT]) rather than continue
the standby loop indefinitely.
