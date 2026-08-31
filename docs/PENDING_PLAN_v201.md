# Pending Plan v201

- task: Apply v198's set-difference procedure to the PRIMARY target (never done)
- source: no bundle — direct source analysis
- approach: v198 found the tenth instance of the extent class in the *control*
  (`TestCornellBoxGI.cpp`) using a set-difference between creation sites, after
  three grep shapes all reported that file clean. The procedure was never run
  against the primary target `TestReSTIR_GI_Temporal.cpp`. Every cycle v183-v197
  fixed that file by *substitution* — a query-shape method that v198 proved
  cannot express lifetime defects. So the primary target has been swept nine
  times by a method known to be incomplete, and never once by the method that
  found the defect the sweeps missed. Close that gap.
- diff_estimate: +0 / -0 expected (audit; patch only if an instance is found)
- skip_plan_review: no
- test_strategy: file-only enumeration with controlled positives for every zero
- risks:
  - The `FB.width` cumulative sweep has never been run either — v191..v195 each
    declined to bundle specifically so their own enumerations stayed verifiable,
    which means each verified only its own site. Nobody ran the union.
  - v198's lesson: absences do not appear in any grep. The check must be a set
    difference over *creation sites vs recreation sites*, not a token sweep.
  - Risk of `§Anti-patterns §6` drift if this merely re-states v200. It does not:
    v200 audited compile risk (arity, cbuffer packing); this audits a *runtime*
    defect class using a procedure never applied to this file.
