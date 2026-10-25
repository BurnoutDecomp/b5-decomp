# Adaptive trigger feedback

This optional PC feature presents the existing car through a native DualSense.
R2 normally represents the engine and traction; L2 represents braking. Reverse
swaps those roles to match VehiclePhysics::UpdateEngine. No physics attributes,
input values, boost rules or recovered record layouts are changed.

For setup and settings, see [PC controllers](CONTROLLERS.md#dualsense-adaptive-triggers).
The initial tuning is a software implementation; physical feel, full pedal
travel, USB/Bluetooth output and transport-specific behavior require the hardware
checks below. An accepted SDL packet is not evidence that a trigger moved.

## Effect catalogue and sources

The main read-only observer is [VehicleFeedback.h](../src/pc/input/VehicleFeedback.h).
It reads the locked local-player interface and the frame's semantic game events
in GameStateModule::PreWorldUpdate. It publishes copied values through a separate
mailbox; only the input path resolves and sends device effects.

| Situation | Trigger feel | Authoritative signal used |
| --- | --- | --- |
| Ordinary acceleration | Light progressive pedal | Actual forward/reverse role and gas demand |
| Engine revving | Fine RPM-dependent texture | RaceCarState::mfRPM |
| Hard launch | Brief load shudder | Low-speed, grounded gas-demand transition |
| Gear change | Short unload, then kick | RaceCarState::mi8Gear transition under drive |
| Rev limiter | Distinct chatter | Engine::Update's actual pre-clamp flywheel > MaxRPM limit observation |
| Wheelspin | Irregular flutter | Actual powered-wheel mask from ApplyEngineForcesOntoWheels, plus valid grounded wheel/road speed difference |
| Grip returning | Short recovery, then smooth pedal | Filtered slip crossing its lower hysteresis threshold |
| Boost activation | Brief surge | BoostOutputInfo::mbIsBoosting transition |
| Sustained boost | Faster energetic texture | Actual boosting state |
| Boost ending | Short wind-down | Boosting-to-not-boosting transition |
| Burnout chain | Separated double kick | Semantic chain notification/count; live count increase during boosting |
| Light braking | Initial travel and bite point | Actual braking demand and grounded state |
| Heavy braking | Firmer progressive resistance | Actual braking demand; presentation profile |
| Braking skid | Coarse chatter | Brake/handbrake demand and grounded wheel skid factor |
| Drift initiation | Brief breakaway cue | Published drift state transition |
| Sustained drift | Uneven controlled texture | Boost drift flag / drift time and absolute drift scale |
| Rough surfaces | Small irregular taps | Ground-contact collision tag -> existing surface/rumblesurface definitions and speed band |
| Ramp takeoff | Both pedals unload | Transition from some valid grounded wheels to none |
| Airborne | Road load disappears; light engine texture remains | No valid grounded wheels; published engine RPM |
| Landing | Contact pulse and short settling tail | Ground reacquisition after airtime, previous vertical velocity |
| Wall scrape | Sustained dragging texture | Local ContactSpy friction stress and mbContactingWall |
| Barriers / traffic checks | Contact knock | Local normal stress; actual traffic-check notification |
| Collision / shunt / takedown contact | Stronger short pulse | Local ContactSpy stress or VehicleImpactEvent naming the local aggressor/victim |
| Player wreck | Brief shudder, then neutral | New actual crash state; driving load disabled while crashing |
| Reverse | Engine on L2, brake on R2 | Actual reverse gear plus the engine's gas/brake role swap |
| Showtime | Bounce/slam/boosted-bounce pulses | Actual Showtime state, contacts and JustBouncedEvent payload |

Additional ArcadeCues use scored near misses, completed stunts and actual boost
awards. BoostStrategy::AddBoost records an award after its existing earning/crash
gates and clamp, only if that operation adds boost. The local-player strategy is
bound in UpdateOutputBoostInfo. No HUD-meter polling is used to infer awards.
Continuous earning is coalesced to at most one reward cue per 300 ms, below more
important physical feedback.

The limiter and boost-award [sidecars](../src/pc/input/EngineFeedback.h) store only
bounded value records and integer identity cookies, never dereference saved game
pointers, and expire observations after 500 ms. They are disabled when feedback
observation is unnecessary. World/car changes reset the main resolver. Diagnostics
can enable observation without a native controller for a software runtime witness.
The powered-wheel mask follows the actual front/rear power split and the original
handbrake rear-only branch; a grounded-wheel flag is not a drive-selection flag.
Losing contact on all powered wheels clears filtered wheelspin without a grip
recovery cue, even when the other axle stays grounded. Launch and recovery pulses
are discarded on powered-wheel contact loss; drift-start pulses are discarded
on takeoff, so they cannot reload the airborne pedals after the unloading ends.
Photo mode uses the director's actual GetInPictureParadise() signal copied in the
post-world step, independently of the recovered rumble manager's pending transfer.

## INI tuning

Edit **config.ini beside Burnout_PC.exe**, section **[AdaptiveTriggers]**, then
restart the game. The complete commented section in [config.ini.example](../config.ini.example)
contains every applicable control with its default. Missing keys preserve the
original Immersive tuning; names and presets are case-insensitive.

| Key | Range and behavior |
| --- | --- |
| Enabled | 0 disables all adaptive output; 1 enables it (default) |
| Preset | Off, Light, Standard, Immersive (default) |
| Resistance / Texture | 0..100 percent; smooth pedal resistance and texture/pulse strength independently |
| LeftStrength / RightStrength | 0..100 percent on physical L2/R2, regardless of forward/reverse pedal role |
| ArcadeCues / Diagnostics | 0 or 1; reward cues default on, bounded diagnostic logs default off |
| `<Effect>Strength` | 0..200 percent of that effect's normal strength; default 100; 0 disables it |
| `<Effect>Frequency` | 25..200 percent of the effect's normal vibration frequency; default 100, final output capped at 120 Hz |
| `<Effect>Duration` | 0..200 percent of a transient's normal duration; default 100; 0 skips it |
| `<Effect>StartPosition` | -1 preserves its normal engagement zone (default); 0..9 selects the first active travel zone |

Effect names are Pedal, Engine, Launch, Shift, Limiter, Wheelspin, GripRecovery,
BoostStart, Boost, BoostEnd, Chain, Brake, BrakeSkid, DriftStart, Drift, Surface,
Takeoff, Airborne, Landing, Scrape, Impact, Wreck, Reverse, Showtime, Bounce,
BoostedBounce and Reward. These prefixes have no spaces or hyphens.

Duration applies to Launch, Shift, GripRecovery, BoostStart, BoostEnd, Chain,
DriftStart, Takeoff, Landing, Impact, Wreck, Bounce, BoostedBounce and Reward.
Continuous effects follow current vehicle state. Frequency only affects vibration;
Pedal, Brake and Showtime provide smooth resistance. Takeoff releases both
triggers, so its Strength only enables/disables that unloading transition and its
Frequency/StartPosition have no effect. A disabled higher-priority effect allows
current lower-priority feedback to continue; disabling Shift also skips its initial
unloading phase. Grounded/airborne behavior and lifecycle clearing still apply.

For example:

```ini
[AdaptiveTriggers]
Enabled=1
EngineStrength=60
EngineFrequency=80
ShiftDuration=150
BrakeStartPosition=4
ScrapeStrength=0
```

Master strength, preset, per-effect strength and physical-trigger strength combine.
The hardware has only eight nonzero force steps, so small numerical changes may
feel identical. Limits are clamped; malformed text and unknown keys are reported
and keep defaults. Settings never alter control values, vehicle physics, output
rate limits or lifecycle clearing. ControllerVibration=0 remains a master mute.

## Vehicle character

Boost type selects Speed, Stunt or Power presentation tuning. Engine frequencies
and launch/brake strengths vary between profiles. Large, slower vehicles select
Heavy tuning when the published half-height exceeds 0.85 and maximum speed is
below 145 mph. Those are presentation thresholds, not inferred mass or changes to
vehicle statistics. Physical calibration may adjust this tuning.

## Composition, timing and native output

[AdaptiveTriggers.h](../src/pc/input/AdaptiveTriggers.h) resolves one mode per
trigger. Wrecks, contacts, landings and bounces take priority; braking/traction and
scraping follow; boost and chain cues follow those; engine/surface/pedal feedback
forms the baseline. Optional reward ticks can briefly replace the baseline.
After a transient expires, the current driving state supplies the next effect.
AI/cinematic driving disables pedal-load effects while local impact/wreck
observations can still be presented. Pause and explicit disable clear all effects.

Native output is limited to one changed packet per 20 ms (50 Hz). Neutral lifecycle
resets bypass that delay. Identical packets are not resent. The device internally
runs its vibration waveform, so steady effects do not require per-frame writes.
Time-based envelopes do not depend on frame count; wheelspin uses smoothing and
hysteresis. Stale snapshots release both triggers after 500 ms.

[DualSenseEffects.h](../src/pc/input/DualSenseEffects.h) produces SDL's 47-byte PS5
effect payload. Right-trigger bytes start at
10, left-trigger bytes at 21. Modes are Off (0x05), Feedback (0x21) and Vibration
(0x26), with ten independent zones clamped to strength 0–8. Zero-amplitude or
zero-frequency vibration encodes Off. The smallest nonzero hardware step limits
how finely a light effect can be scaled. Feedback and vibration are coordinated
modes, not arbitrary simultaneous force channels.
When ordinary rumble is active, trigger reports and neutral trigger clears also
carry its current motor values and emulation mode. Firmware selection and legacy
amplitude scaling follow SDL's pinned PS5 driver, including its Edge/unknown-firmware
policy. Trigger-only deduplication keeps motor changes from generating extra
adaptive writes. Speaker and LED controls stay disabled in these reports.
Texture=0 silences vibration and transient pulses while preserving smooth pedal
resistance. Ground-contact unloading still follows takeoff/airtime. Resistance=0
silences smooth resistance independently of enabled textures.

SDL owns USB/Bluetooth framing and CRCs. Native Sony PS5 identity and a successful
neutral output probe are required before sending adaptive effects. An output
failure attempts a neutral reset once, disables that connection's adaptive path
and keeps input/ordinary rumble available. Replacing/reconnecting the device
reprobes support and starts neutral. The same selected handle receives input,
ordinary rumble and adaptive output; no second HID owner is introduced.

Protocol references:

- [SDL 3.4.18 PS5 packet example](https://github.com/libsdl-org/SDL/blob/release-3.4.18/test/testcontroller.c)
- [SDL_SendGamepadEffect](https://wiki.libsdl.org/SDL3/SDL_SendGamepadEffect)
- [SDL 3.4.18 PS5 rumble composition](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/joystick/hidapi/SDL_hidapi_ps5.c)
- [John Nielk1 Klein's trigger effect factories](https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db), with the adapted encoding's [MIT notice](licenses/DualSenseTriggerEffects.txt)

## Software validation

Run from the parent workflow:

```powershell
python b5-decomp/tests/run_adaptive_triggers.py
python b5-decomp/tests/run_vehicle_feedback.py
python b5-decomp/tests/run_pc_gamepad.py
python b5-decomp/tests/run_fxrumble3_pad_rumble.py
python b5-decomp/tests/run_fxtailsa_pad_idle.py
python b5-decomp/tests/run_cgs_keyboard_pc_contract.py
```

The resolver suite covers driving/contact/boost/drift/reverse/Showtime transitions,
priorities, expiry, settings, profiles, malformed values, independent zone packing
and limiter/award identity filtering. The projection suite uses the production
published records and variable event queue to check member selection, reverse,
invalid contact rejection, local impact filtering and unchanged queue contents.
The SDL virtual-device suite drives the production backend and records output
packets, ordinary rumble, rate limits, failure fallback and lifecycle behavior.
These checks do not prove physical motor behavior.

Diagnostics=1 emits bounded `[triggers]` lines in BrnGame.log: selected device,
transport, output support, car identity, RPM, speed, ground contact, slip, boost,
Showtime and resolved cues. Support=0 means no device, 1 unsupported, 2 accepted
native effect output and 3 rejected. Physical validation is separate in every case.
With no diagnostics/native device, the world observer is skipped and the sidecars
perform only their cheap disabled check.

## Physical USB and Bluetooth validation

Detailed device/transport results have not been recorded. Record build commit, controller model/firmware where available,
transport and results separately for USB and Bluetooth. Test normal input and
ordinary rumble alongside the effects. Test Edge separately if available.

1. Use native connection, enable Diagnostics=1 and select Immersive. Confirm the
   selected device accepts effect output. Establish Standard/Light/Off comparisons.
2. Test partial and sustained full throttle, partial/full braking, launches,
   upshifts, reverse and real limiter intervention. Confirm full pedal travel and
   complete accelerator/brake input remain reachable.
3. Test boost start/sustain/end, a chain continuation, wheelspin/recovery,
   brake-induced drift and sustained drift. Compare at least three car profiles.
4. Test rough surfaces, takeoff/airtime/landing, wall contact, barrier/traffic
   contact, shunts and a wreck. Verify road chatter disappears in the air and a
   wreck releases after its short shudder.
5. Test Showtime ordinary and boosted bounces, contact sequences and optional
   near-miss/stunt/boost-award ticks. Disable ArcadeCues and repeat reward cases.
6. Test overlapping states: boost during drift/wheelspin, landing while braking,
   scraping into a larger impact and wreck-to-respawn. Check priority and recovery.
7. Test pause, menu/car selection, photo mode, loading, Alt-Tab, an assert screen,
   unplug/reconnect, another selected controller, settings Off/zero strengths,
   ControllerVibration=0 and normal game exit. No stale force should persist or
   replay on return. Recheck ordinary rumble after adaptive output is disabled.
8. Compare a representative drive with Off and Immersive, including output cadence
   and performance. Successful API calls, virtual devices and boot success alone
   do not satisfy this physical checklist.

Report both the diagnostic evidence and what each trigger physically felt like.
Keep physical device/transport results separate from software checks and from
maintainer approval to merge the feature.
