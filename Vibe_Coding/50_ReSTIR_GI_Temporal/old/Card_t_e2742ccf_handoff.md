# Card `t_e2742ccf` — handoff state (2026-07-21)

This document captures the live state of the only card on the kanban
board so a future session can resume without searching the SQLite.

## Card facts

- **id:** `t_e2742ccf`
- **title:** fix: TestReSTIR_GI_Temporal sblob path resolution
- **assignee:** `default`
- **created:** 2026-07-21 06:42
- **created_by:** user
- **current status:** `blocked` (kind: `needs_input`)
- **retries used:** 1 of 2
- **block reason recorded on disk:**

```
review-required: sblob path is fixed and the target test passes/dumps,
but validator is 0/4 because the test's pre-existing render pipeline
produces black frames (missing GBuffer population plus command-list/
TemporalReservoir layout validation errors). Completing acceptance
requires a broader rendering-pipeline task, not the scoped path repair.
```

## What the worker DID accomplish (already on disk, committed nowhere yet)

- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`: replaced
  `'%s'` + `*FString` log pair with `'{}'` + `TO_TCHAR_CSTR` so the
  resolved path actually prints instead of the literal `%s` token
  that fmt's pointer check suppresses. This was **step 1** of the card
  body; done.

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`: changed
  `MakeShaderDataDir()` to anchor at `${GProjectRoot}/Engine/Source/
  Runtime/Test/${GExecutableName}_Data` rather than relying on a
  relative `GExecutableDirectory` (which is itself relative when the
  binary is launched by name, producing a non-resolving path).
  Resolved sblob path now appears in the log. This addressed **step 3**
  ("wrong path" branch).

- Added missing Compute visibility to the GIAccumulate layout. Test
  builds and runs end-to-end without validation errors at the
  layout-binding step.

- `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
  passes, exits 0. Four PNG frames dump to
  `TestReSTIR_GI_Temporal_Data/dumps/`. This satisfies the *first*
  two acceptance criteria and the *build* half of the chain.

## What the worker did NOT do (and why)

The acceptance criterion `validate_restir_gi.py exits 0` fails because
**the dumped frames are entirely black.** Three independent causes
all point to a missing rendering stage upstream of the ReSTIR passes:

1. **GBuffer textures are never populated.** The test cpp declares
   `GBufferWorldPos` / `GBufferNormal` / `GBufferMaterial` but no
   pass writes to them. FGIPass reads from these as input; with
   zero/garbage content, every per-pixel primary hit direction is
   undefined and every GI ray is wasted.
2. **Command-list-not-open errors** during the ReSTIR dispatch
   sequence. Likely the same missing pass — a half-built pipeline.
3. **`TemporalReservoir` reports `GENERAL` vs `SHADER_READ_ONLY`
   layout mismatch.** Probably an nvrhi Vulkan validation layer
   complaint from a missing resource-state transition.

The worker correctly **refused to mask or rewrite** the rendering
pipeline in scope because the card body explicitly asked for a
mechanical path-repair task and that scope was already met.

## What a follow-up card would need (do NOT auto-create yet)

A sibling card — `t_<next>` — would need roughly:

- Acceptance criteria that allow partial-scope success:
  e.g. *"Renders one colored quad that proves GBuffer→GI→display
  chain works end-to-end, even if Sponza is not visible."* That
  decouples the assertion from the full Sponza+ReSTIR rendering.
- A draft GBuffer fill pass that draws one quad with a hardcoded
  world position + normal + material. This unblocks validator
  structural checks without requiring the full `GBufferSponzaVS/
  GBufferSponzaPS` pipeline to be wired through FGIPass.
- Optionally: separate cards for (a) GBuffer fill, (b) Sponza
  material textures via `FSceneResourceManager`, (c) full
  ReSTIR/ReBLUR chain validation.

## Why nothing more was done in this session

Per the rules in the freshly-patched `kanban-cron-overseer` skill
(`references/bypass-rules.md`):

- Hard veto #1: cards with `requires_human=true` — cron won't act.
  Doesn't apply (no such tag).
- Hard veto #2: `blocked` cards without `auto_unblock_on_dependency:
  true` — **cron won't flip.** Applies here. Cron stays silent.

The card sits at `blocked/needs_input` waiting for human reply.

## Suggested next-step options for the user

These are mutually exclusive — pick one:

1. **Extend the card** with the rendering-pipeline scope (would
   require another worker retry; the rendering pipeline is multi-day
   work, not a single tick).
2. **Replace the card** with a narrower sibling card focused on
   "first colored pixel," with reduced acceptance criteria.
3. **Mark the card KEEP-with-caveat**: the path-resize bug IS fixed;
   the rendering-pipeline gap is a separate tracked item; the
   validator's 0/4 result is information, not a failure of the path
   fix specifically.
4. **Leave the card blocked** until a human reviews the three
   remaining issues (GBuffer fill, command-list sequencing,
   TemporalReservoir layout mismatch) and decides the next move.

The cron will see this card as `blocked` on every tick and stay
silent (hard veto). No autonomous action will be taken.

## Action taken on this handoff (2026-07-21)

User chose option 2. Two actions:

- Sibling card `t_8291cf8c` ("TestReSTIR_GI_Temporal: first colored
  pixel (GBuffer fill)") created with assignee=default, priority=8,
  status=ready. Body explicitly scopes to a single-quad GBuffer fill
  + reduced validator, deliverable in one tick. Cites this handoff
  doc and the path-trace debug session as companions.
- Card `t_e2742ccf` got a comment recording the supersession. Status
  left as `blocked/needs_input` — the kanban system has no delete
  operation and the worker explicitly flagged it for human review.
  The user (or next session) can `hermes kanban complete
  t_e2742ccf --summary "superseded by t_8291cf8c"` or
  `hermes kanban archive t_e2742cf8c` once the sibling card verifies
  the rendering chain works.

The watchdog cron `c897231ceb87` will see both cards and skip both
on the next tick: `t_e2742ccf` is `blocked` (hard veto #2), and
`t_8291cf8c` has no bypass flags so the cron won't auto-KEEP it.
