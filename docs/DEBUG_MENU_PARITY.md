# Debug-menu parity campaign

Every control must follow ARTIST's behavior, including original no-ops and broken
leftovers. Restoring removed systems is a separate campaign. Registration alone
does not prove a control works.

The [workflow inventory](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/tree/main/progress/debug_menu)
tracks original registration sites and evidence. Its initial baseline has 1,354
sites across 91 functions and 64 named activation functions. Computed labels,
loop-generated rows and two missing wrapper exports still need enumeration;
these counts are not a completed menu audit.

## Verified repairs

| Control | Restored behavior |
| --- | --- |
| Race Car Entity / Tailgate... / Cone HalfAngle (Rads) | Changes the actual detection cone. |
| Cone Depth | Changes the actual distance test. |
| Min Tailgate Speed | Changes the other car's minimum speed, in mph. |
| Max Relative Speed | Changes the allowed velocity difference, in m/s. |
| Min Duration | Changes when continuous tailgating earns boost credit. |

Previously the menu and cone renderer used writable globals while detection and
boost used independent constants. Both consumers now read the same original data.
Shipped defaults and the strict duration threshold are retained; the relative-speed
comparison also follows PPC's unordered branch behavior.

Production-body regression: 34/34 checks. Shipping compile/link and faithfulness
gates pass. The live case exercises COMPONENT, SET, EXEC and F-key bindings, then
observes detection, boost credit, blocking thresholds and restoration. Run it from
the workflow with `tools/tests/run_case.ps1 -Case debug_menu_tailgate -Slot 15`.
It uses a copied profile and deliberate extreme slider values; it proves those
debug controls, not organic tailgating balance. The optional
`BRN_TAILGATE_DEBUG_TRACE` witness is observational and capped at 160 lines.

All other inventory entries remain pending in this campaign, including entries
previously tested for navigation. Next: connect the race-car reset diagnostic
history to its original producers, then continue through the unavailable pages in
[DEBUG_MENU_REPORT.md](DEBUG_MENU_REPORT.md).
