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

## DualSense adaptive triggers

Native DualSense controllers can use adaptive pedal feedback: engine revs and
gear changes, traction and drifting, braking, boost, road contact, jumps,
landings, scrapes, impacts and Showtime. Vehicle profiles give speed, stunt,
powerful and large vehicles different signatures. Ordinary rumble still works.

Released pedals stay free. Resistance builds with throttle/brake demand, becomes
firmer under boost or hard braking, and eases during wheelspin, drifting and
airtime. Strong contact forces are brief. The default 100 strength uses these
situational curves; it does not hold the motor at maximum.

In **config.ini beside Burnout_PC.exe**, add:

```ini
[AdaptiveTriggers]
Enabled=1
Preset=Immersive
Resistance=100
Texture=100
LeftStrength=100
RightStrength=100
ArcadeCues=1
Diagnostics=0
```

Restart after editing. Presets are `Off`, `Light`, `Standard` and `Immersive`
(default). Resistance and Texture independently accept 0–100 and are also scaled
by the preset. ArcadeCues enables small reward ticks. The existing feedback
enable/pause gates and `[SystemProfile] ControllerVibration=0` also silence
adaptive output. Set Enabled=0 to disable everything. LeftStrength/RightStrength
scale the physical L2/R2 independently, including in reverse. Invalid settings keep
their defaults; numeric ranges are clamped.

Every effect also accepts Strength (0..200 percent; 0 disables it), Frequency
(25..200 percent for vibration), Duration (0..200 percent for transient pulses)
and StartPosition (-1 for its default, or travel zone 0..9). For example,
EngineStrength=60 softens the engine texture, ShiftDuration=150 lengthens the
gear-change cue, and BrakeStartPosition=4 moves the brake bite farther into travel.
The complete ready-to-copy section is in [config.ini.example](../config.ini.example);
see the [tuning reference](ADAPTIVE_TRIGGERS.md#ini-tuning) for effect names and limits.

Use a native USB or Bluetooth connection. A translation tool presenting a virtual
Xbox controller can hide adaptive capabilities. The adapter falls back to normal
input/rumble when native output is unavailable or rejected. Device selection is
the same player-0 selection described above. DualSense Edge coverage must be
recorded separately.

Effects clear on focus loss, pause, blocking asserts, device replacement and
shutdown; a 500 ms stale-snapshot limit also releases them when game updates stop.
The explicit harness background override applies to trigger output as well.
Diagnostics logs bounded `[triggers]` lines in BrnGame.log; with no controller it
can observe the game's signals without claiming physical output.

Distributions also include **DualSenseTriggerEffects-LICENSE.txt**. See the
[effect catalogue and testing guide](ADAPTIVE_TRIGGERS.md) for signal sources,
priorities, software checks and the pending physical USB/Bluetooth checklist.

## Map controller buttons to keyboard keys

Add a `[ControllerKeyboard]` section to **config.ini beside Burnout_PC.exe**,
then restart. For example:

```ini
[ControllerKeyboard]
R4=End
Back+Start=F11
L4=Ctrl+Shift+K
```

`R4=End` dismisses the assert screen from the right primary paddle, including
while the normal game input loop is halted. `Back+Start=F11` toggles fullscreen;
the last example holds a keyboard chord while L4 is held. Targets reach the game's
keyboard bindings, debug keyboard and window shortcuts. Events belong to the game
window and mappings release when it loses focus, the controller disconnects, or
the game shuts down. The physical keyboard remains available.

Button names are case-insensitive. Use SDL button names or these aliases:

- `R4`, `L4`, `R5`, `L5`: right/left primary and secondary paddles.
- `A/B/X/Y` or `Cross/Circle/Square/Triangle`: the four face positions.
- `LB/RB` (`L1/R1`), `LS/RS`, `Back` (`Select`), `Start` (`Menu`).
- `DPadUp/Down/Left/Right`, `Guide`, `Touchpad`, and `Misc1` through `Misc6`.

Join controller buttons with `+` for a combination. Active mappings consume their
source buttons, replacing their original game actions. A complete longer chord
takes priority over an overlapping shorter binding; unrelated bindings can run
together. A shorter binding still activates if it is pressed before the remaining
buttons of its chord.

Targets accept letters, digits, `F1`–`F24`, `End`, `Home`, `Insert`, `Delete`,
`Enter`, `Escape`, `Space`, `Tab`, `Backspace`, `PageUp/PageDown`, arrow names,
`Shift`, `Ctrl`, `Alt`, and hexadecimal Windows virtual-key codes such as `0x23`
for End. Join target keys with `+` for a keyboard chord. At most 64 mappings are
loaded; invalid entries are skipped and their count is logged. With no mappings,
the original controller bindings apply.

Extra buttons must be exposed by the device's SDL driver/mapping. Physical Steam
controller paddle behavior remains subject to the hardware validation below.
The Bluetooth Steam Controller HIDAPI driver is enabled by default; an explicit
`SDL_JOYSTICK_HIDAPI_STEAM` environment override is respected.

## Automated checks

From the parent workflow:

```powershell
python b5-decomp/tests/run_pc_gamepad.py
python b5-decomp/tests/run_adaptive_triggers.py
python b5-decomp/tests/run_vehicle_feedback.py
python b5-decomp/tests/run_fxtailsa_pad_idle.py
python b5-decomp/tests/run_fxrumble3_pad_rumble.py
python b5-decomp/tests/run_cgs_keyboard_pc_contract.py
```

The SDL test runs the production backend, action fill and ManagerX360 with SDL
virtual devices: face buttons, D-pad, start/back, shoulders, stick clicks, stick
endpoints/directions, triggers, connection changes, deterministic selection,
press/release edges, rumble routing, focus muting and shutdown. Keyboard tests use
SDL virtual paddles and the production assert-key helper, including polling while
normal gameplay is paused, overlapping chords, consumed buttons, key messages,
focus/disconnect/shutdown release and config parsing. It checks bounded
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
