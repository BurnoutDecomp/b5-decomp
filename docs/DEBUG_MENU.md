# Debug menu

The PC debug UI uses the reconstructed ARTIST menu, variables, callbacks, window
stack and script interface. Its keyboard adapter also supports the game harness.

| Key | Action |
| --- | --- |
| Ctrl+Space | Open the Development menu |
| Space, while a menu is open | Return to the Development menu |
| Up / Down | Select a row |
| Enter | Open a submenu, activate a component, or call the selected function |
| Left / Right | Change a value; either direction toggles a boolean |
| Backspace | Close the current unpinned window |
| Esc | Hide the debug UI |
| `/` | Pin or unpin the active window |
| Tab / Shift+Tab | Next / previous window |
| Shift+arrows | Move the active unpinned window |
| Ctrl+arrows | Dock the active window |
| Backtick | Open or close the command console |
| F1–F12 | Execute a configured binding |

The debug UI captures the keyboard while visible and through the release of its
closing keys. Ordinary gameplay keys cannot edit hidden menus. Physical keyboard
input requires the game window to have focus; unattended harness runs suppress it.

Console examples:

```text
HELP
SET "Core/Debug/Settings/Text Size" 18
ALIAS size "Core/Debug/Settings/Text Size"
size 16
BIND F12 size 18
CALL "Debug/Sim/Step"
CALL "Debug/Sim/Play"
SAVE "debug-settings.txt"
EXEC "debug-settings.txt"
```

Aliases name variables or registered functions. BIND takes the command and its
arguments separately; quote individual paths containing spaces. SAVE includes
active components, saveable variables, aliases, bindings and window positions.
Paths are relative to the game's working directory. ARTIST initializes the
autoexec guard to skip `autoexec.txt`; that default is preserved. Use EXEC to run
a script explicitly.

## Regression harness

From BP-Decomp_Workflow, after building the executable and installing game data:

```powershell
pwsh -NoProfile -File b5-decomp/tests/run_debug_menu.ps1
```

The test owns the box lock and runs the existing `flow_run.ps1` as its child. It
waits for actual car selection, then checks component activation, boolean and
numeric menu edits, SET, SAVE/EXEC round-trip, aliases, F12 bindings, window
pinning and hidden-key isolation. It also exercises Step/Play and captures
screenshots for visual inspection. Any game assertion fails the run.

Use `-Effects -MaxSeconds 250` to check Bloom, Vignette, DOF, Tint and 2d Tint
at the post-processing consumer, capture the same paused scene with effects on
and off, and verify function and variable aliases survive SAVE/EXEC. DOF still
depends on camera blurriness when enabled, as in ARTIST.

Harness keys are manual-reset events named `Local\BurnoutPC_DebugKey_XX`, where
`XX` is the two-digit Windows virtual-key code. Hold the event for key-down and
reset it for key-up. All 256 handles should exist before the game reads them.
`BRN_HARNESS_SLOT` adds the existing `_<slot>` suffix; these events are read only
when `BRN_INPUT_ALLOW_BACKGROUND` is set. `BRN_DEBUG_UI_TRACE=1` enables window and
input-event witnesses in `BrnGame.log`.

The menu exposes the reconstructed subsystem components. Their underlying game
systems retain the project's existing partial reconstruction limits. In
particular, the PC world update does not yet publish the original immediate
streaming status, so the unfinished streaming-pause bridge remains disconnected;
consuming that untouched status freezes the junkyard. This does not change the
existing dev pause scheduling.
