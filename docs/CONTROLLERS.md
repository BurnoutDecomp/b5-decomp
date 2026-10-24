# PC controllers

Connect an SDL-supported gamepad over USB or Bluetooth before or after launching.
Xbox, DualShock 4 and DualSense use the same existing menu and driving mappings;
PlayStation controllers do not require an XInput translation program. Button
prompts retain the game's existing artwork. Other devices require an SDL gamepad
mapping; SDL's `SDL_GAMECONTROLLERCONFIG` environment variable accepts custom mappings.

Player 0 uses the earliest attached recognized controller (lowest SDL instance ID).
It keeps that controller until disconnection, then selects the earliest remaining
one. Reconnecting a device does not take control away from the current selection.
Disconnects clear held controls. Standard two-motor rumble uses that same device;
devices without rumble still provide input. Console wheel force feedback is outside
this gamepad adapter's scope.

Keyboard bindings, the game's focus gate and harness channels are unchanged.
Losing focus also stops rumble. The harness's `BRN_INPUT_ALLOW_BACKGROUND=1` keeps
gamepad input and rumble available in background runs; it does not enable background
keyboard input. Rumble stops when the backend shuts down.

The build uses **SDL3 3.4.18**, downloaded from the official release with a pinned
SHA-256. From the parent workflow run `build sdl3`, then `build exe`; `build all`
also fetches SDL. Keep **SDL3.dll** and **SDL3-LICENSE.txt** beside the executable
when distributing the build. No separate SDL installation is required.
CMake targets find `vendor/sdl3/cmake` or an explicit `SDL3_DIR` and stage the
shared runtime and license beside their executable.

## Automated checks

From the parent workflow:

```powershell
python b5-decomp/tests/run_pc_gamepad.py
python b5-decomp/tests/run_fxtailsa_pad_idle.py
python b5-decomp/tests/run_fxrumble3_pad_rumble.py
python b5-decomp/tests/run_cgs_keyboard_pc_contract.py
```

The SDL test runs the production backend, action fill and ManagerX360 with SDL
virtual devices: face buttons, D-pad, start/back, shoulders, stick clicks, stick
endpoints/directions, triggers, connection changes, deterministic selection,
press/release edges, rumble routing, focus muting and shutdown. It checks bounded
empty-device enumeration and launches a separate native probe from a temporary
package with only the Windows system directory on PATH.

## Hardware validation

Physical controller validation was deferred by the maintainer on 2026-10-24
because no controller was available. Virtual devices do **not** verify USB or
Bluetooth transport, physical motor output, or complete menus/gameplay on hardware.
Before claiming hardware coverage, record controller model, transport, build commit
and results for:

- DS4 and DualSense, each over USB and Bluetooth, in menus and gameplay.
- Xbox face buttons, D-pad, both sticks and clicks, triggers, shoulders, pause and back.
- Connect after launch, unplug while holding controls, reconnect, and two-device selection.
- Rumble during play, focus loss, disconnect and normal exit.
- A packaged game launch on a machine without SDL installed.
