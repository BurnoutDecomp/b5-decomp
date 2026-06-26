# The Burnout Paradise Takedown Logic Chain

A complete reverse-engineering of the takedown system of the Burnout 5 / Paradise X360 ARTIST
build (`BURNOUT_X360_ARTIST.XEX`) — from the instant two cars touch, through impact
classification, the crash commit, the layered crash-state machine, and every downstream
game-system ripple (boost, scoring, progression, achievements, AI). Addresses are X360.

The core of the chain lives in `BrnPhysics::Vehicle::VehicleManager` (`BrnVehicleManager.cpp`);
the crash-state machine is in the `RaceCarPhysics → VehiclePhysics → SimpleVehiclePhysics`
inheritance chain; the effects fan out across `BrnWorld` (boost), `BrnGameState` (scoring /
achievements), `BrnProgression` (the persistent profile) and `BrnAI`.

---

## 0. The chain at a glance

```
ProcessContactSpies
 └─ HandleRaceCarRaceCarContact 0x82642F78   STAGE 1 — populate the RaceCarResponseInfo working set
      │     (decode EntityIds → indices; gate on mbTakedownsEnabled + mUsedRaceCars; compute
      │      speeds, closing velocity, mfAngleBetweenCars = acos(dot(fwdA,fwdB)); cache interfaces)
      ├─ CheckForGrindingAndRubbing 0x825B5450    (player grind pre-pass → grind event 7/8)
      ├─ CheckForAllTypesOfImpacts 0x82642E58     STAGE 2 — the priority classifier ladder (void)
      │     │   energy gate: |speedA| + |speedB| ≥ flt_82FB8290
      │     │   (#1,#2 may run while a car is already crashing; #3-#8 only if NEITHER is)
      │     ├─ 1. CheckForPlayerSlammingAIIntoAI    0x8263E000  ─▶ InstantTakedown   (domino)
      │     ├─ 2. CheckForHittingAlreadyCrashingCar 0x8263DAC0  ─▶ InstantTakedown   (pile-on)
      │     ├─ 3. CheckForVerticalTakedown          0x8263D728  ─▶ InstantTakedown   (VERTICAL)
      │     ├─ 4. CheckForTBoneTakedown             0x8263D480  ─▶ InstantTakedown   (T_BONE)
      │     ├─ 5. CheckForHeadToHead                0x8263D1A0  ─▶ ApplyShunt + InstantTakedown (HEAD_ON)
      │     ├─ 6. CheckForShuntAndNudge             0x8261A3A0     [FORCE ONLY — not a takedown]
      │     ├─ 7. CheckForSlamAndTradingPaint       0x82619F30     [FORCE ONLY — not a takedown]
      │     └─ 8. CheckForStationaryTargetTakedown  0x8263D948  ─▶ InstantTakedown   (stationary)
      ├─ SetRaceCarCrashing (direct, for v251/v252 set by the classifiers)
      ├─ GenerateContactSituation 0x82646... → ApplySlam / ApplyShunt   (the shunt-physics layer)
      └─ last-attacker / revenge bookkeeping

InstantTakedown 0x82636108   STAGE 3a — decode victim/aggressor, gate, commit
 └─ SetRaceCarCrashing 0x82634C90   STAGE 3b — the universal crash-commit sink
      ├─ RaceCarPhysics::SetCrashing 0x825B8A70   STAGE 4 — the crash-state machine (3 tiers)
      │    └─ vtbl+8 ─▶ VehiclePhysics::SetCrashing 0x825FD088 ─▶ SimpleVehiclePhysics::SetCrashing 0x825D98F0
      ├─ VehicleManagerOutputInterface::AddRaceCarCrashEvent(...)   (the cross-module "TAKEDOWN" event)
      ├─ VariableEventQueue<1536,16>::AddEvent  @ sink+26096        (IO / network / replay)
      └─ allocate maRaceCarCrashData[32] slot {entityId, ETakedownType, priority}  @ +43808

ALTERNATE takedown entry points (bypass the geometric classifiers, share the InstantTakedown tail):
 • DoHornTakedowns 0x8263CC68        ◀ UpdateDrivers          (honk → nearest rival, normal=(0,1,0), stress 50.0)
 • UpdateAggressiveDriving 0x82640690 ◀ UpdateVehiclePhysics  (tailgate/in-front/rubbing timers → free takedowns)

SetRaceCarCrashing is ALSO called directly by: ForceRaceCarCrash, HandleRaceCarTrafficCarPotentialContact,
HandleRaceCarWorldPotentialContact.

STAGE 5 — downstream ripples of a committed takedown:
 • Boost (kill victim's):  BoostStrategy::SetCrashing 0x822A5F00  (byte+188; rising edge → stop-boost vtbl+28)
 • Boost (reward attacker): BoostBurnout2/3/5::OnTakedown 0x822A6288/6638/6F60  (grant boost; BB2/BB3 chain)
 • Scoring fan-out hub:    GameStateModule::ProcessTakedownEvents 0x8238FC50  (drains BrnGameState::TakedownEvent)
      → ScoringSystem::OnPlayerDoesATakedown, DeveloperChallengeManager::OnTakedown(+Chain),
        AchievementManagerBase::OnTakedown 0x8235AAE0, ProgressionManager::OnTakedownTo 0x823666D0
            → Profile::AddTakedown 0x82354C00   (total +102, per-type histogram +104+type)
 • AI rubber-band:         AIModule::OnPlayerTakedown 0x8278A720  (++ race-balancing lane takedown tally)
```

---

## 1. ETakedownType (`BrnGameState::ETakedownType`, home `BrnTakedownType.h`)

| value | name | committed by |
|---|---|---|
| -1 | NONE | (sentinel) |
| 0 | STANDARD | generic slam / aggressive-driving |
| 1 | GRINDING | grind pre-pass |
| 2 | T_BONE | CheckForTBoneTakedown; DoHornTakedowns |
| 3 | VERTICAL | CheckForVerticalTakedown |
| 4 | TRAFFIC_CHECK | (vs traffic vehicle) |
| 5 | HEAD_ON | CheckForHeadToHead |
| 8 | DOUBLE | (two victims one impact) |
| 9 | REVENGE | revenge path (last-attacker bookkeeping) |
| 10/11/12 | INTO_CAR / INTO_VAN / INTO_BUS | shunted into traffic |

The classified type is carried to the scoring layer through the `maRaceCarCrashData[32]` pool
(see §3b) and lands in `Profile::AddTakedown`'s per-type histogram (§5).

> **Key reading correction.** The integer literals `48` / `64` / `80` that appear inside the
> `InstantTakedown(...)` calls are **NOT** the `ETakedownType` — they are the *byte offset of the
> contact-normal / contact-point vector argument*, loaded via `lvx128 vN, r11, 48/64` where
> `r11 = lpInfo->mpContact`. The PowerPC vector-by-value ABI then passes that 16-byte VMX
> register as the `lCollisionNormal` argument. The takedown type is a separate trailing argument
> resolved inside `InstantTakedown` / `SetRaceCarCrashing`.

---

## 2. The `RaceCarResponseInfo` working set

The per-contact struct (`BrnVehicleManager.h:763`, fully reconstructed) that every classifier
reads. Populated by `HandleRaceCarRaceCarContact` from a `RaceCarContact`. Key fields the chain
uses: `mpContact` (+0x00, holds the collision normal @ contact+48 and point @ contact+64); the
four output/deformation interface pointers (+0x04…+0x10); `mRaceCarA/BEntityID` (+0x14/+0x18);
`meActiveRaceCarIndexA/B` (+0x1C/+0x20); `mpRaceCarA/B` (+0x24/+0x28); `mClosingVelocityAtoB`
(+0x30); `mvfSlamMagnitude` (+0x40); the per-car `mbRaceCarA/BIsCrashing` (+0x50/+0x51),
`…IsPlayer` (+0x52/+0x53), `…IsNetworkCar` (+0x54/+0x55); `mfClosingSpeed` (+0x58);
`mfRaceCarA/BSpeed` (+0x5C/+0x60); `mfNormalStressSq` (+0x64); both `Matrix44Affine` transforms
(+0x70/+0xB0); **`mfAngleBetweenCars` (+0xF0)** = `acos(clamp(dot(fwdA,fwdB), -1, 1))`; and the
classifier *outputs* `meImpactType` (+0xF4), aggressor/victim indices (+0xF8/+0xFC), crash flags
(+0x100/+0x101), `mbPlayerWonImpact` (+0x102), `muImpactScore` (+0x104), `meImpactSitutation`
(+0x108).

---

## 3. Stages 1–3: detect → classify → commit

### Stage 1 — `HandleRaceCarRaceCarContact` @0x82642F78 (populate + drive)

Called by `ProcessContactSpies`. Hex-Rays could not allocate its locals (huge function), but the
flow resolves: decode the two `EntityId`s → active-car indices `(id>>10)&0x3FFF` (asserts both
owners are `E_ENTITYTYPE_RACECAR`); **master gate** `mbTakedownsEnabled` (VehicleManager+171464);
both cars must be live in the `mUsedRaceCars` `CgsBitArray` (+44224); normalize + sanity-assert the
contact normal (|n|≈1). It then fills the `RaceCarResponseInfo`: indices, the `maRaceCarCrashState`
(+44192) reads → is-player / is-crashing flags, the per-car velocity magnitudes → speeds + closing
velocity, and `mfAngleBetweenCars` from the two forward vectors via `XMVectorACos`. A **grinding
pre-pass** (`CheckForGrindingAndRubbing`, when a player is involved) can push a grind event
(type 7/8, thresholds +171868/+171900). Then `CheckForAllTypesOfImpacts` runs; if it set the
per-car "should crash" flags it calls `SetRaceCarCrashing` directly; finally `GenerateContactSituation`
→ `ApplySlam` (situations 1/3/5) or `ApplyShunt` (2/4/6), both gated on `mbSlamShuntPhysicsEnabled`
(+171465, one byte past the takedowns-enable gate). **Last-attacker / revenge bookkeeping** is
written here: `maRaceCarLastAttacker[victim]` (+171684) ← `miAttackerToRecord` (+171540), plus
`maRaceCarLastImpactMagnitude` (+171644), the per-car `maRaceCarTakenDownThisFrame` byte (+171676)
and the `mTakenDownRaceCarsBitArray` (+171736); takedown events are throttled by
`muTakedownEventsThisFrame < 32` (+172612).

### Stage 2 — `CheckForAllTypesOfImpacts` @0x82642E58 (the priority ladder, void)

Energy gate (`|speedA| + |speedB| ≥ flt_82FB8290`), then the 8 sub-classifiers first-match-wins.
Sub-classifiers #1 (PlayerSlammingAIIntoAI) and #2 (HittingAlreadyCrashingCar) may run while a car
is already crashing; the parent guards the remaining six behind `!isCrashingA && !isCrashingB`.

**The eight sub-classifiers** (each reads `RaceCarResponseInfo` + a few deep VehicleManager
tuning members, then either commits via `InstantTakedown` or applies force only):

1. **CheckForPlayerSlammingAIIntoAI** 0x8263E000 — the player rams one AI into a *second* AI (a
   "domino"). Highest priority. Requires both cars active AI (`maRaceCarCrashState==1`), neither
   crashing, and (via per-record attacker bookkeeping) that the player is the one doing the
   slamming. Calls `ShouldRaceCarCrashOnCarImpact` per victim, commits whichever passes.
2. **CheckForHittingAlreadyCrashingCar** 0x8263DAC0 — one car is *already* crashing and the other
   rams it (a "pile-on" / finish-off). This is why it precedes the not-crashing gate. Includes a
   player-revenge sub-gate (`mePlayerActiveRaceCarIndex` vs the record's current-attacker field).
3. **CheckForVerticalTakedown** 0x8263D728 — one car ends up vertically above/below the other
   (ramp-launch onto, or slammed down on, the victim). Early-outs if *both* cars had a recent
   impact (`HasRaceCarHadRecentImpact`); calls the sibling `CheckForVerticalTakedownSituation` and
   an up-axis height comparison. Commits VERTICAL.
4. **CheckForTBoneTakedown** 0x8263D480 — perpendicular hit: `|mfAngleBetweenCars − π/2| <
   mfTBoneAngleBandDegrees·(π/180)` (band @ +171564), plus a side-plane half-width test
   (+171568). Commits T_BONE.
5. **CheckForHeadToHead** 0x8263D1A0 — head-on: `|mfAngleBetweenCars| ≥ (180° −
   mfHeadToHeadAngleToleranceDeg)·(π/180)` (tolerance @ +171628) AND at least one car's speed ≥
   `mfHeadToHeadMinClosingSpeed·flt_82F31928` (+171636). Decides the loser by scaled speed,
   multiplies the closing magnitude by 5.0, sets impact type 4, applies a shove (`ApplyShunt`),
   and commits HEAD_ON via `InstantTakedown`. *(The only classifier that both shoves AND crashes.)*
6. **CheckForShuntAndNudge** 0x8261A3A0 — **FORCE ONLY.** Recency-gated (`HasRaceCarHadRecentImpact`
   on both cars). Alignment must be below 1.9 (too head-on → belongs to #5). Classifies the
   contact as nudge (closing ≤ `mfNudgeMaxClosingSpeed`·scale, +171580, type 2) or shunt (≤
   `mfShuntMaxClosingSpeed`·scale, +171584, type 4), stamps the impact-type field, returns 1 — but
   **never crashes** anyone.
7. **CheckForSlamAndTradingPaint** 0x82619F30 — **FORCE ONLY**, the lightest tier (side-by-side
   "trading paint"). Recency-gated. Energy must fall in the band `[mfTradingPaintMinSpeed,
   mfTradingPaintMaxSpeed]` (+171616/+171620). Sets impact severity 1/3/5, stores a slam vector,
   returns 1. No crash.
8. **CheckForStationaryTargetTakedown** 0x8263D948 — taking down a near-stopped car. Master gate
   `+172315`; proximity (squared distance < 0.04); speed-asymmetry (`|speedA−speedB| ≥
   flt_82FB8298`, slower ≤ `flt_82FB829C`, faster ≥ `flt_82FB7F18`). Commits straight to
   `InstantTakedown` (no shove — the victim is stationary).

**Predicates.**
- `ShouldRaceCarCrashOnCarImpact` — per-victim "does this impact qualify to crash the car"
  (consumed by #1/#2, takes the victim/other RaceCarPhysics records + collision-normal rodata
  vectors). Inlined / no standalone export.
- `HasRaceCarHadRecentImpact` 0x825B4EB8 — the **recency throttle**. The two force-only tiers
  (#6/#7) bail if *either* car has had an impact within its recent window; this prevents a single
  sustained scrape from spawning a stream of shunts every frame. Takedowns (#3-#5,#8) do *not* gate
  on it — a takedown is a one-shot terminal event (the victim is now crashing, so it can't repeat).

> **Shunt vs takedown.** A *takedown* (#1-#5, #8) is a **commit**: it calls
> `InstantTakedown → SetRaceCarCrashing`, which wrecks the victim and stamps last-attacker /
> taken-down bookkeeping. A *shunt / nudge / slam* (#6/#7) is a **force + timer**: it shoves the
> car, classifies the severity, sets a recent-impact marker, and returns 1 so the cascade treats
> the contact as handled — but both cars survive and can collide again once the recency window
> expires.

### Stage 3a — `InstantTakedown` @0x82636108 (commit entry)

Decodes the victim/aggressor `EntityId`s to active-car indices, gates on `mbTakedownsEnabled`,
skips re-crashing a car already in the fatal crash-state (`maRaceCarCrashState != 2`), calls
`SetRaceCarCrashing`, zeroes the aggressor's recovery timer if the player was the victim, and
stamps the last-attacker + taken-down bookkeeping. *(Surprising but asm-verified: the crash-state
check + last-attacker write use the VICTIM slot, the recovery-timer zero + taken-down byte use the
AGGRESSOR slot.)* Bodied in `BrnVehicleManager.cpp`.

### Stage 3b — `SetRaceCarCrashing` @0x82634C90 (the universal crash-commit)

The sink every takedown path funnels into (also called directly by `ForceRaceCarCrash` and the
traffic/world contact handlers). Hex-Rays failed its locals; the body resolves to:

1. **Suppression gates** — decode the victim index; bail if any of: `RaceCarStatusRecord` flag
   bytes +124/+125 (per-car invuln / already-handled, by cause sub-code), `mbSuppressPlayerCrash`
   (+172306) AND victim is the local player (`mePlayerActiveRaceCarIndex` +172204), or
   `mbSuppressIfAlreadyCrashState1` (+172307) AND already in crash-state 1.
2. **Entity-id validation** — against per-car id tables (+43584, +148128); debug asserts.
3. **The crash commit** — `RaceCarPhysics::SetCrashing(&maRaceCarVehicles[victim], bool)` where the
   bool is `(player not remote) & (distance-to-player < radius) & (crashState==1) & a7`. So a
   *distant* car crashes **logically** (state + event) but **skips the physics latch** — the crash
   replay only runs for cars near the player camera. Stamps the vehicle record (entity id @ +5200,
   crash matrix @ +5184, `mbCrashCommitted` @ in-record +3097), fires
   `AddRaceCarCrashEvent(sink, …)`, and pushes a 64-byte record onto the IO
   `VariableEventQueue<1536,16>` (sink+26096).
4. **`maRaceCarCrashData[32]` allocation** — a `CgsBitArray` free-list (+44232) finds a slot in the
   32-entry pool at **+43808** (12-byte stride `{u32 mEntityId, u32 meType=ETakedownType, f32
   mfPriority}`); when full it overwrites the lowest-priority entry. **This is where the
   ETakedownType chosen upstream is recorded** for the scoring/UI layer to read by entity id.

---

## 4. Stage 4: the crash-state machine (3 inheritance tiers)

`RaceCarPhysics : VehiclePhysics : SimpleVehiclePhysics`. `SetRaceCarCrashing` calls
`RaceCarPhysics::SetCrashing`, which dispatches down the chain — each tier contributes its slice of
the crash onset:

- **`RaceCarPhysics::SetCrashing` 0x825B8A70** — calls the base (vtbl+8) first, then (if crashing)
  snapshots the live velocity/orientation vectors (record +80/+96) into the crash-replay slots
  (+5136/+5152) and zeroes the crash-blend / recovery float (+5168). This drives the spinning
  crash animation. *(Same 5216-byte record VehicleManager indexes as `maRaceCarVehicles[8]`.)*
- **`VehiclePhysics::SetCrashing` 0x825FD088** — resets selected lanes of the orientation/velocity
  vectors (+3824/+4096…+4160), clears settle flags (+4946/+4958), sets a handle to NONE (+4340 =
  −1), chains to the base, then **seeds the crash-tumble impulse** (`vmaddfp` into the +1824 region,
  scaled by `unk_83018040`).
- **`SimpleVehiclePhysics::SetCrashing` 0x825D98F0** — the leaf flag-setter: sets the two canonical
  **is-crashing booleans** (+1808 / +1811 = 1) that the rest of the engine polls, re-normalizes 4
  motion/suspension vectors, and **clears the per-wheel "active" bytes** (+517/+741/+965/+1189,
  stride 224) so the car goes limp.
- **`BoostStrategy::SetCrashing` 0x822A5F00** — a *separate* world-side flag (not in the physics
  chain): byte +188 = crashing; on a rising edge it fires vtbl+28 (the stop-boost reaction) —
  this is how a crash kills the crashing car's active boost.

---

## 5. Stage 5: downstream ripples of a committed takedown

The crash event + a deferred `BrnGameState::TakedownEvent` fan out across the game systems:

- **Boost reward (attacker).** `BoostBurnoutN::OnTakedown` (vtable-dispatched from the boost update,
  fired when *you* land a takedown): **BB2** 0x822A6288 grants boost (vtbl+196, award +40) or
  accumulates into a pending chain pot (+308) when deferring (+197); **BB3** 0x822A6638 increments a
  capped (max 3) takedown-chain counter (a1[77]), notifies the chain (vtbl+200), then grants
  (vtbl+196, award a1[10]); **BB5** 0x822A6F60 is the minimal grant. (vtbl+196 = the boost-grant
  entry; per-mode award field +40 / a1[10].)
- **Scoring fan-out hub.** `GameStateModule::ProcessTakedownEvents` 0x8238FC50 drains the queued
  `TakedownEvent`s and calls `ScoringSystem::OnPlayerDoesATakedown`,
  `DeveloperChallengeManager::OnTakedown` (+`OnTakedownChain`), `AchievementManagerBase::OnTakedown`,
  `ProgressionManager::OnTakedownTo`, and pushes a `VariableEventQueue<13312,16>` UI/audio event
  (using `GetPlayerActiveRaceCarIndex` for player-vs-AI attribution).
- **Persistent profile.** `ProgressionManager::OnTakedownTo` 0x823666D0 → `Profile::AddTakedown`
  0x82354C00, which bumps the **total-takedowns counter (+102)** and the **per-`ETakedownType`
  histogram (+104+type)** — the saved stat (STANDARD=0…INTO_BUS=12 index the histogram). It also
  fires training tips gated by game-mode bits.
- **Achievements.** `AchievementManagerBase::OnTakedown` 0x8235AAE0 gates the "X takedowns" /
  mode-specific takedown achievements off those profile counters.
- **AI rubber-band.** `AIModule::OnPlayerTakedown` 0x8278A720 (from `AIModule::HandleGameActions`)
  resolves the AI car, and if race-balancing is enabled increments that car's lane takedown tally
  in the `RaceBalancingRoute<7>` table — feeding the catch-up / rubber-band rival AI.

---

## 6. Alternate (non-impact) takedown entry points

Both bypass the geometric classifiers and share the `InstantTakedown → SetRaceCarCrashing` tail.

- **`DoHornTakedowns` 0x8263CC68** (◀ `UpdateDrivers`) — when the player honks (gate `+172311` +
  the per-car horn-active flag), find the **nearest** eligible rival (loop the `mUsedRaceCars`
  bitset, skip the player's own index, keep the minimum distance), and commit `InstantTakedown`
  with the collision normal forced to world-up `(0,1,0)` and a fixed `lfNormalStressSq = 50.0`.
- **`UpdateAggressiveDriving` 0x82640690** (◀ `UpdateVehiclePhysics`) — per-frame timers over all 8
  car slots detect sustained aggression against the player: **tailgating**, **in-front**, and
  **rubbing** counters (+171936) and float accumulators (+171744). Past their thresholds it sets
  driver-feedback bytes (output interfaces) and fires **free** takedowns (`lfNormalStressSq ≈ 0.01`)
  gated purely on timers, not impact energy.

---

## 7. Recovered `VehicleManager` deep layout (used by the chain)

Offsets are asm-proven; names DWARF-attested where noted, else inferred-by-role (flagged in the
header). Per-car arrays are indexed by active-race-car slot (0..7).

| Offset | Member | Notes |
|---|---|---|
| +0 (224-stride) | `maRaceCarStatus[8]` (`RaceCarStatusRecord`) | `mbTakenDown` @ in-record +124; suppress flags +124/+125 |
| +1856 (5216-stride) | `maRaceCarVehicles[8]` (`RaceCarPhysics`) | the per-car physics record (crash latch @ +5136/+5152, recovery @ +5120/+5168) |
| +43584 (4) | `maRaceCarEntityId[8]` | id validation table |
| +43744 (8) | `maAggressiveDrivingVictimEntityId[8]` | |
| +43808 (12-stride) | **`maRaceCarCrashData[32]`** | `{u32 mEntityId, u32 meType (ETakedownType), f32 mfPriority}` — the type record |
| +44192 (4) | `maRaceCarCrashState[8]` | 1 = active, 2 = fatal/crashing |
| +44224 | `mUsedRaceCars` (`CgsBitArray<8>`) | live-car bitset |
| +44232 | `mRaceCarCrashDataAllocBits` (`CgsBitArray<32>`) | free-list for the crash-data pool |
| +171464 | `mbTakedownsEnabled` | master gate |
| +171465 | `mbSlamShuntPhysicsEnabled` | the slam/shunt-physics gate |
| +171540 | `miAttackerToRecord` | source for last-attacker writes |
| +171564 / +171568 | `mfTBoneAngleBandDegrees` / `mfTBoneSidePlaneHalfWidth` | T-bone test |
| +171580 / +171584 | `mfNudgeMaxClosingSpeed` / `mfShuntMaxClosingSpeed` | shunt/nudge bands |
| +171616 / +171620 | `mfTradingPaintMinSpeed` / `mfTradingPaintMaxSpeed` | paint band |
| +171628 / +171636 | `mfHeadToHeadAngleToleranceDeg` / `mfHeadToHeadMinClosingSpeed` | head-on test |
| +171644 (4) | `maRaceCarLastImpactMagnitude[8]` | |
| +171676 (1) | `maRaceCarTakenDownThisFrame[8]` | |
| +171684 (4) | `maRaceCarLastAttacker[8]` | revenge source |
| +171736 | `mTakenDownRaceCarsBitArray` (`CgsBitArray<8>`) | |
| +171744 / +171936 | `maAggressiveDrivingTimers` / `maAggressiveDrivingCounters` | tailgate/in-front/rubbing |
| +171868 / +171900 | `mfGrindingThresholdA` / `B` | grind pre-pass |
| +172204 | `mePlayerActiveRaceCarIndex` | DWARF-attested |
| +172306 / +172307 | `mbSuppressPlayerCrash` / `mbSuppressIfAlreadyCrashState1` | crash suppression |
| +172311 | `mbHornTakedownEnabled` | horn gate |
| +172315 | `mbStationaryTakedownsEnabled` | stationary gate |
| +172612 | `muTakedownEventsThisFrame` | per-frame event cap (32) |

Shared rodata constants (values not in the per-function exports — recover from `.rdata`):
`flt_82FB8290` (energy gate), `flt_82F31928` (global speed-unit scale, multiplies every tuning
speed threshold), `flt_82FB8298/829C` + `flt_82FB7F18` (stationary thresholds), `unk_82FB8310`
(paint alignment), `byte_82F2A1A4/A5` (aggressive-driving frame gates), `unk_83018040`
(crash-tumble scale).

---

## 8. Reconstruction status

| Function | Status |
|---|---|
| `CheckForAllTypesOfImpacts` | **bodied** (`BrnVehicleManager.cpp`) |
| `InstantTakedown` | **bodied** |
| the 8 sub-classifiers + `HasRaceCarHadRecentImpact` | mapped (this doc); bodyable next — geometric/state tests against the §7 layout |
| `SetRaceCarCrashing`, `HandleRaceCarRaceCarContact` | mapped; large functions (Hex-Rays local-alloc failure) — body against asm/DWARF, not the pseudocode |
| crash-state machine (`*::SetCrashing`) | mapped; belongs to the RaceCarPhysics/VehiclePhysics home pass |
| downstream ripples (boost / scoring / AI `OnTakedown`) | mapped; separate subsystems |
