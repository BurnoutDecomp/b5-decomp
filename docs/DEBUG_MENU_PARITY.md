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
| Race Car Entity / Show place on track info | Shows the latest reset transform, line test and intersection spheres; can be hidden and restored. |
| Force Online Freeburn Spawn Position | Respawns the local lobby car at its original grid-row debug position. |

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

The reset overlay now shares the module's original embedded debug component with
its producers. It records every accepted result, including empty results and resets
of other cars. Its geometry uses the renderer's queued debug pass and the original
34-vertex/192-index sphere mesh. Native shader/pixel checks pass 84/84; the isolated
live case verifies show/hide/restore, actual render dispatch and visible geometry.
Run `tools/tests/run_case.ps1 -Case debug_menu_reset_history -Slot 15` from the
workflow. `BRN_RESET_HISTORY_TRACE` is a bounded, passive witness.

The forced online spawn control reaches both original mode-arming reads. With it
off, a same-model local lobby car keeps its pose; with it on, the car respawns at
the original spawn-table position. Production checks pass 16/16. Separate isolated
LAN-host runs pass 9/9 for both settings, including actual physics-position checks.
Run `tools/tests/run_case.ps1 -Case debug_menu_racecar_spawn -Slot 16`; set
`BPH_TEST_FORCE_ONLINE_SPAWN=0` in the runner's environment for the default case.

Apply Ram and Magnitude are available with their original behavior: Apply Ram
latches an unused diagnostic flag, and Magnitude stores a value in range 0..100.
ARTIST has no gameplay consumer for either. No new ram effect is added.

Seven registration sites are verified and two are classified as original no-ops.
All other entries remain pending, including those previously tested for navigation.
Other debug primitives and text still need individual parity audits. Next: the
race-car visual/statistics controls, then the unavailable pages in
[DEBUG_MENU_REPORT.md](DEBUG_MENU_REPORT.md).
