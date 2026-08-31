# Pending Plan v204

- task: Sweep v202/v203's layout-vs-each-consumer invariant across the OTHER shared pass classes (the domain v203 left unswept), per v203's standing rule
- source: no bundle — direct source derivation
- approach: v203 swept the invariant across `FReSTIRPass`'s five layouts / six
  layout-consumer pairs, and stopped there. But `FReSTIRPass` is not the only
  shared runtime pass class with two consumers. Enumerate the others, apply
  BOTH invariants this lineage has established to each — (a) v202's
  layout-vs-each-consumer binding agreement, and (b) v183's Phase-D
  guide-extent rule (every texture a dispatch samples must be indexed in that
  texture's own space) — and patch whatever the sweep finds.
- diff_estimate: unknown at plan time (sweep first, patch what it finds)
- skip_plan_review: no
- test_strategy: per-claim file-only re-derivation with same-shape positive
  controls; declaration-shaped queries per v203 row 16; read every returned diff
- risks:
  - The `.hlsl` dual-copy trap (v182): any shader touched exists in two
    directories. Check, do not assume.
  - Patching a SHARED C++ pass changes behaviour for BOTH consumers, including
    the known-good control (`software-development-practices` rule 4). Any fix
    must degrade to a byte-exact no-op for the control, and that must be
    demonstrated, not asserted.
  - v203's near-miss: an `old_string` anchored on a comment adjacent to a
    braced initialiser matches into the initialiser. Anchor on statement
    boundaries and read every returned diff.
