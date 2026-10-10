# Debug menu repair report

This pass fixes menu presentation, hierarchy, native UI behavior and controls with
existing engine consumers. It does not replace unreconstructed diagnostic renderers
or console-only platform services.

## Fixed

- Restored the selected-row palette from ARTIST: highlight `0xFFFFFFC0`, text
  `0xFF000000` (data at `82F32E38/82F32E3C`). This produces the original pale cyan
  highlight and black selected text.
- Restored missing `GetPath` overrides from the original component vtables.
  Gameplay's mode, scoring, challenge, street and takedown components now nest under
  Gameplay. Network's server, buddy, scoreboard and event-score components nest under
  Network. Prop diagnostics nest under Physics; trigger entities under Triggers;
  PVS/Massive under World; language under Gui.
- Restored the network player-manager identity (`PlayerManager`, `Network`) from
  vtable `820CE35C`, removing the blank root row. Its option table is terminated.
- Restored `Menu::IsUseful() == false` from vtable `820DC354`: hierarchy rows are
  not actions when deciding whether to reuse a window slot. Native submenu
  selection retains the parent window, including parents containing only submenus;
  Back closes the child and returns focus to its parent.
- Unavailable sections retain their entry, stay inactive, and show a dismissible
  modal explanation. An empty activation no longer silently deletes its row.
- Added native pool headroom for the additional reflection/shadow controls.
- Restored nine supported traffic scalar controls through `pc/debug/TrafficControls.h`.
  They bind the live traffic fields used by existing update/render code: traffic off,
  stopped movement, mesh visibility, speed, traffic-light timing, game-mode density,
  final density, budget statistics and avoidance. The original complete
  traffic diagnostic component is still incomplete.
- Registered the recovered Race Car Entity component at the original Prepare
  registration point, using native PC ownership for its complete existing vtable.
  Its supported controls and HUD are available; the concrete gaps below remain
  unavailable. Early INI and component vehicle-LOD registrations share one set of
  rows and metadata. Buddy activation is also available
  after integrating the current reconstruction branch.

The default font size, border metrics, caption rendering and window sizing were
checked against the original code/data and retained. The width adapts to available
rows; missing original systems also remove their longest labels. The two cropped
reference images alone do not establish a different screen-coordinate origin.

## Remaining unavailable sections and controls

| Section | Concrete reconstruction dependency |
|---|---|
| Force Quit to Start Menu | ARTIST's flag at `82FAE28C` is consumed by `BridgeGuiToGame`, which uses Xbox launch data and `XLaunchNewImage`. A native soft-reboot lifecycle is absent. A no-op checkbox is not added. |
| Sound Module | The sound debug component has constructor/identity support, but its activation and diagnostic HUD are incomplete. Normal audio is a separate path. |
| AI | Main AI debug construction/activation and some draw helpers remain absent; the existing world/HUD slices do not supply a complete component lifecycle. Ordinary AI driving is a separate path. |
| Renderer | The original renderer debug component remains a skeletal `GetPath` implementation. Native graphics and reflection controls are available through their existing groups. |
| GameDataModule / DLC | Original debug activation, callbacks and owner wiring are incomplete. |
| Game State | Only a small callback slice, including Toggle Showtime, is homed; the full component lifecycle/menu/HUD is missing. |
| Gui / LanguageManager | The main GUI debug component has singleton and callback slices, but lacks its complete activation/lifecycle; language debug activation is also absent. Correct hierarchy does not reconstruct those controls. |
| Replays | The debug component lifecycle and controls are incomplete alongside partial replay support. |
| Race-car air ram / forced online spawn / last reset trace | Apply Ram has no runtime consumer, so its action and Magnitude input are omitted. Mode arming still uses constant `false` for the forced-spawn switch. Place-on-track line/intersection producers are not connected to the native component owner. Forced spawn and last-reset trace controls are read-only until these dependencies are reconstructed. |
| Traffic pressure system | Its debug field is initialized but has no reconstructed runtime reader. The no-op checkbox is omitted. |
| Traffic drawing / air rams / kill-zone actions | Several diagnostic draw helpers, action paths and the fuzzy-logic diagnostic allocation remain missing. The native scalar adapter does not expose those unsupported actions. |
| Online service diagnostics | Buddy menu activation and its queue are available. Several service consumers still depend on unavailable platform services; activating a page does not establish working online messaging or invites. |
| Shadow-map frustum/TSM diagnostic drawing | `ShadowMap::DebugRender` remains an unreconstructed diagnostic pipeline. Its three debug toggles are read-only to prevent selecting an assert trap. Shadow rendering itself and the new caster controls are separate. |
| Other diagnostic overlays | A registered scalar does not establish that every original 2D/3D overlay consumer is reconstructed. Existing physics/world diagnostic gaps remain outside the available scalar consumers. |

The affected declaration/body boundaries are visible in `BrnAIDebugComponent`,
`BrnDebugComponent` (sound), `BrnRendererModuleDebugComponent`, `BrnGuiDebugComponent`,
`BrnGameStateDebugComponent`, `BrnTrafficDebugComponent`, and `BrnShadowMap` sources.

## Verification

The root-entry live regression failed before the repair on the unnamed network
player-manager row and passes after the hierarchy/identity changes. The real menu
also passes navigation, boolean and numeric edits, state SAVE/EXEC, aliases, F12
bindings, pin/close handling and rejection of ordinary input while hidden.

Additional live coverage exercises the restored traffic and network option controls
and unavailable-section handling. The integrated capture confirms that Race Car
Entity and Buddies activate and vehicle LODs retain exactly 14 shared rows. The
effects regression passes disabling/restoring bloom, vignette, DOF and tint, with
saved aliases restored to their correct targets. The test driver uses longer named-key holds during
new material/font loading; the game's command parser and controls are exercised.

One long integrated Controls run encountered a D3D presentation failure
(`0x88760874`) followed by a failed output reset (`0x8876086C`) after the supported
values and shared-row checks were saved. The interrupted unavailable-section
check was rerun separately with `-Unavailable` and passes, including dismissal and
reopening. This pass does not establish recovery from that device failure.

The palette and hierarchy evidence comes from ARTIST data/vtables, not tuning the
menu to the reference image by eye. Build/test evidence and the vtable extraction
are retained under the workflow's `scratch/PC_SUPPORT_1010/` run directory.
