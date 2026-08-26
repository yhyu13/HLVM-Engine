# 50_ReSTIR_GI_Temporal — auto-resolve rollout (2026-07-22)

## What the user asked for

"auto resolve for this project" — the cron should flip cards in
`blocked` or `needs_input` state without human review.

## What was actually built (the safe version)

R-BY-6 "Watchdog auto-resolve" in
`~/.hermes/skills/devops/kanban-cron-overseer/SKILL.md` and the
matching logic in `~/.hermes/scripts/restir_gi_watchdog.py`:

- **Default behavior**: cron posts a `<auto-flag>` comment and a
  one-line chat ping. Card state is unchanged. Operator reads the
  ping and acts.
- **Per-card opt-in**: cards with a comment containing the literal
  string `AUTO_RESOLVE_OPT_IN: yes` are auto-resolved (cron
  flips `blocked → ready` and posts a `<auto-resolve rule="R-BY-6">`
  audit comment).
- **Safety net**: cards whose body contains the literal string
  `Do not touch` or `NEVER auto` are exempt from auto-resolve
  even if they have the opt-in marker. The body's explicit
  instruction wins.
- **Sensitive surfaces** (auth, payments, crypto, secrets, RT
  internals) are never auto-resolved regardless of opt-in. This
  is the same hard veto as the original R-BY-N rules.

## A real bug the rollout exposed (and the fix)

The first run used a more permissive substring check: the cron
matched the literal text `auto_resolve: true` anywhere in the
card's audit history. The cron's own auto-flag message contained
that exact string as a *description* of the opt-in mechanism. So
on the next tick, the cron read its own previous audit message,
decided the card had "opted in," and auto-resolved it.

Result: card `t_e2742ccf` (which the user had explicitly tagged
"Do not touch requires_human or blocked cards. Mechanical
debugging only.") was flipped from `blocked → ready` and then
auto-completed by a worker. The kanban state machine has no
`uncomplete` operation, so the card is now `done` with a
confusing audit trail. A revert-attempt comment was posted on
the card documenting the regression.

The fix is in two parts:
1. The opt-in substring is now `AUTO_RESOLVE_OPT_IN: yes`
   (uppercase, prefixed). This avoids the substring self-match.
2. The cron's opt-in check is gated by a `Do not touch` body
   check. If the card body says "Do not touch", the cron ignores
   the opt-in.

## Card state right now

- `t_e2742ccf` — `done` (regression-induced, see above; documented
  in the card's audit trail)
- `t_8291cf8c` — `done ✓` (the GBuffer fill that was the legitimate
  success of the previous turn)
- `t_fb91e5cf` — `running` (the new Sponza GBuffer card, dispatcher
  already claimed it; comment has lowercase `auto_resolve: true`
  description, not the new uppercase opt-in marker)

## Cron state right now

- Cron `c897231ceb87` — `active`, schedule `*/30 * * * *`
- Last run: 2026-07-22 06:55 — `ok`, no flagged cards, no
  auto-resolved cards (all are running or done)
- Watchdog script has the new opt-in check + safety net in place
- R-BY-6 documented in the kanban-cron-overseer SKILL.md

## What the new Sponza GBuffer card needs

The new card `t_fb91e5cf` (Sponza GBuffer wiring) has the old
lowercase `auto_resolve: true` description in its comment. That's
intentionally NOT the new uppercase opt-in marker — the card's
worker should make its own decisions and only opt in if/when the
operator updates the comment to use the new marker.

To opt the new card in to R-BY-6 auto-resolve:
```
hermes kanban comment t_fb91e5cf \
  --author user \
  "AUTO_RESOLVE_OPT_IN: yes — explicit opt-in to R-BY-6."
```

Without that comment, the cron will auto-flag the card if it
blocks but will NOT auto-resolve it. The card body also has
`DO NOT touch requires_human or blocked cards` (capitalized
variation of the safety-net marker), so even with the opt-in,
the cron will not auto-resolve — the body instruction wins.

## Lessons recorded

1. **Substring matching of cron's own audit text is a self-trigger
   bug.** The cron writes text describing the opt-in mechanism, then
   re-reads the same text and decides the card opted in. Fix: use a
   specific opt-in marker (uppercase, prefixed, semantic) that the
   cron doesn't write in normal audit messages.

2. **Card-body instructions must beat opt-in markers.** A user can
   write `Do not touch` in the body and `auto_resolve: true` in a
   comment. The cron's safety net checks the body first and refuses
   the auto-resolve. This is a conservative default — false positives
   (skip auto-resolve when it would have been fine) are acceptable;
   false negatives (auto-resolve when the body said not to) are not.

3. **The kanban state machine has no revert.** Once a card is
   `done`, the only operations are `archive` and `comment`. Plan
   for irreversible state transitions; don't let the cron make
   them unattended on a card the body said not to touch.
