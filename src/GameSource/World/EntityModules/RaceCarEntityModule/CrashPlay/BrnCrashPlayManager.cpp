// =================================================================================================
// BrnCrashPlayManager.cpp -- the Showtime / crash-play state machine.
//
// FIFTEEN out-of-line X360 bodies, ~1,000 instructions, reconstructed 2026-08-29 from the ARTIST
// asm with the DecFIGS DWARF for declaration shape and member names. This is the CONSUMER end of
// the showtime boost economy: until it landed, `mfBounceBoostTimer` had no producer, so
// CrashPlayManager::IsBounceBoosting() was permanently false, RaceCarEntityModule never set
// PlayerDriverControls::mbBoostBounce, nothing ever spent boost, and CrashModeScoring::
// HasCrashModeEnded's idle ladder could not fire. See the banner in BrnCrashPlayDebugComponent.h.
//
// ⛔ THE FEB-2007 SOURCE FOR THIS FILE IS A DIFFERENT PROGRAM AND WAS NOT USED AS A TEMPLATE.
// It is the Burnout-Revenge-era crash play -- crashbreaker, fuel trails, UpdatePostVehicleImpactPing,
// UpdateRotation, six-axis spin rams. NONE of those functions exists in ARTIST, and ARTIST's set
// (bounce boost, traffic stomp, car leaping, new-road/junction messages, the difficulty ramp) is
// absent from Feb-2007. Only the file-scope constant IDIOM (`static const float32_t KF_...`) and
// the brace style are taken from it, which is exactly the rung-3 role AGENTS.md assigns it.
//
// ⭐⭐⭐ THE TUNING CONSTANTS ARE READ OUT OF THE IMAGE, NOT GUESSED -- AND FIVE OF THEM ARE REALLY 0.
// The DecFIGS dwarfdump for this .cpp declares 28 NON-const `float32_t`/`int32_t` file-scope
// tunables at lines 37..64 (KF_BOOST_FOR_VEHICLE_IMPACT_LOW .. KF_UPPER_LIMIT_VEHICLE_SCORE) and a
// separate group of `const` ones. Twenty-three of the 28 sit CONTIGUOUSLY in ARTIST's initialised
// data at 0x82CDB508..0x82CDB560, one 4-byte slot each, in exactly the DWARF's declaration order --
// which is what identifies every single value below. The remaining FIVE (declaration lines 43, 44,
// 45, 51, 52) are the ones missing from that run, and they live in the zero-filled region at
// 0x82FAD2F4..0x82FAD304 -- also contiguous, also in declaration order.
//
// Those five are KF_BOOST_FOR_NEW_ROAD, KF_BOOST_FOR_NEW_JUNCTION, KF_BOOST_FOR_DISTANCE_TRAVELLED,
// KF_COST_FOR_1_BOUNCE_ON_GROUND and KF_COST_FOR_2_BOUNCES_ON_GROUND, and they are 0.0f IN THE
// SHIPPED BUILD. Corroborated two independent ways, because "the image reads zero" is on its own
// worth nothing (an uninitialised global reads zero too):
//   (1) THE SPLIT ITSELF IS THE EVIDENCE. A zero-initialised scalar goes to BSS while a non-zero one
//       stays in .data; the 23 siblings prove the idiom here is `float32_t KF_X = <literal>;`, so
//       the five in BSS have the literal 0.0f. The two runs interleave perfectly -- 0x508..0x51C is
//       lines 37..42, 0x520..0x530 is lines 46..50, 0x534..0x560 is lines 53..64 -- 23 + 5 == 28,
//       with no slot left over and no slot double-booked.
//   (2) NOTHING WRITES THEM. An exhaustive scan of the ARTIST function exports for references to
//       0x82FAD2F4 / 0x82FAD2FC / 0x82FAD300 returns exactly TWO files: 0x822A7D68 (OnEnterRoad)
//       and 0x823020D0 (UpdateMomentum) -- both READERS. There is no producer, no debug-variable
//       registration, no static initialiser.
// ⇒ `mfBoostPercentage += KF_BOOST_FOR_NEW_ROAD` really is a no-op on the console. 0 IS the identity
// of the expression it appears in, which is the one case where reproducing a zero is safe (see the
// placeholder-identity rule). The designers tuned those five income/cost terms off; they are NOT a
// dropped side effect and NOT a hole in this reconstruction. Do not "restore" them to a guess.
//
// Reader used: tools/re/x360rd.py, calibrated first against twelve values Hex-Rays had already
// resolved independently in this TU's own pseudocode (0.0 / 1.0 / 100.0 / 0.079999998 / 0.071428575
// / 0.66666669 / +-1.1920929e-7 / 0.1 / 0.5 / 0.07 / 0.04) -- all twelve matched exactly.
// =================================================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"   // BrnTraffic::GetVehicleSpecies
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"                    // RaceCarContact
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"            // CreateAirRamEvent
#include "GameSource/GameState/BrnGameActions.h"                                    // the three action records
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/Math/BrnMathUtils.h"                                           // BrnMath::Magnitude2D
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/fpu/scalar_operation.h"
#include <cstdlib>   // getenv -- the [crashplay] witness only

namespace BrnWorld
{

// ---- the `const` group (DecFIGS BrnCrashPlayManager.cpp :27..:34) -------------------------------
// These are `const` in the DWARF, so MSVC folded each into the immediate pool rather than giving it
// storage; every value below is the literal the ARTIST asm loads at the one site that uses it.

// :27 -- HandlePlayerToVehicleImpact @0x822D59F0 `lfs f0, flt_82014A5C` == 0.079999998, compared
// against RwMath::Magnitude(lpContact->mNormalStress).
static const f32 KF_MIN_CRASH_MAGNITUDE_REACTION = 0.08f;

// :32 -- Activate seeds mfTimeSinceLastVehicleImpact with flt_82CDB648 == 1.2f, and Feb-2007 spells
// this constant out as (KF_AIR_RAM_OPPORTUNITY_TIME - KF_AIR_RAM_DELAY_TIME) + 0.5f == (0.8 - 0.1)
// + 0.5 == 1.2 exactly. Seeding a debounce timer WITH its own threshold is the idiom this file uses
// three times (see the two below), so the first event of each kind fires immediately.
static const f32 KF_MIN_TIME_BETWEEN_AIR_RAMS = 1.2f;

// :33 / :34 -- both 1.0f (flt_82001C98). HandlePlayerToVehicleImpact gates the stomp on
// `mfTimeSinceLastTrafficStomp >= 1.0` and OnHitOverheadSign gates on
// `mfTimeSinceLastHitOverheadSign >= 1.0`; Activate seeds both fields to exactly that value.
static const f32 KF_MIN_TIME_BETWEEN_TRAFFIC_STOMPS         = 1.0f;
static const f32 KF_MIN_TIME_BETWEEN_HITTING_OVERHEAD_SIGNS = 1.0f;

// :67 -- `const rw::math::vpu::Vector3 K_WORLD_Y_AXIS`, the only Vector3 global the DWARF gives this
// TU, added to the (already normalised) camera Z before the traffic-stomp air ram is normalised.
// ⚠️ FLAG -- THE ONE VALUE IN THIS FILE THAT THE IMAGE DOES NOT SETTLE. ARTIST reads it from
// unk_82FAD920, which is zero in the image, and an exhaustive export scan finds no writer (the only
// reference is UpdateTrafficStomp itself). But unlike the five scalars above, this object CANNOT be
// statically initialised: `RwMath::GetVector3_YAxis()` is a function call, so a namespace-scope
// `const Vector3` initialised from it necessarily gets a DYNAMIC initialiser -- BSS plus a CRT entry
// -- and CRT initialiser thunks are precisely the unnamed helpers the export set is known to have
// holes in. Feb-2007 (`static const RwMath::Vector3 K_WORLD_Y_AXIS = RwMath::GetVector3_YAxis();`)
// and the DecFIGS DWARF agree on the name and the type, so the Y axis is what is reconstructed here.
// DELETE-WHEN somebody reads the ARTIST .CRT initialiser table and settles it: if it turns out no
// initialiser exists, this becomes GetVector3_Zero() and the stomp ram fires along the camera Z
// instead of 45 degrees above it.
static const Vector3 K_WORLD_Y_AXIS = rw::math::vpu::GetVector3_YAxis();

// :70..:75 -- the traffic-stomp block. KI_TRAFFIC_STOMP_AIR_RAM_FRAME_DELAY is the one value the
// DWARF dump prints an initialiser for (40), and ARTIST stores exactly 40 into miFramesUntilAirRam.
static const f32 KF_TRAFFIC_STOMP_GROUND_AIR_RAM_POWER = 0.07f;   // flt_82014A74
static const f32 KF_TRAFFIC_STOMP_FLYING_AIR_RAM_POWER = 0.04f;   // flt_82014A78
static const s32 KI_TRAFFIC_STOMP_AIR_RAM_FRAME_DELAY  = 40;      // DWARF :74, printed
static const f32 KF_TRAFFIC_STOMP_DECAY_RATE           = 0.1f;    // flt_820147E0 -> CreateAirRamEvent::mfDecay

// :78..:84 -- the aftertouch block.
// GROUND vs AIR is decided by which arm of UpdateMomentum's `RwMath::IsZero(mfTimeSinceLastInAir)`
// test loads which literal: mfTimeSinceLastInAir is zeroed while the car is airborne, so the
// IsZero arm (flt_8201FA48 == 0.071428575) is the AIR rate and the other (flt_8200AECC ==
// 0.66666669) is the GROUND rate -- aftertouch bleeds away nine times faster on the ground.
static const f32 KF_AFTERTOUCH_GROUND_DECAY_TIME  = 0.66666669f;
static const f32 KF_AFTERTOUCH_AIR_DECAY_TIME     = 0.071428575f;
static const f32 KF_AFTERTOUCH_NO_BOOST_DECAY_TIME = 0.5f;        // flt_820147FC
// :83 -- granted by OnCarCrash @0x822C3304 for every NEW car the player hits during crash play
// (`lfs f0, flt_820147FC` == 0.5f, added to mfAftertouchPower and Min-clamped to the max). The
// DWARF declares it immediately before KF_AFTERTOUCH_FOR_STATIONARY_BOUNCE_BOOST, and OnCarCrash /
// OnBounce are the only two sites in this TU that ADD a 0.5f to mfAftertouchPower -- which is what
// splits the shared flt_820147FC literal across the two names in declaration order.
static const f32 KF_AFTERTOUCH_FOR_CAR_IMPACT = 0.5f;
// :84 -- granted by OnBounce when the bounce came from a standing start
// (JustBouncedAction::mbFromStationary); same 0.5f literal, different constant.
static const f32 KF_AFTERTOUCH_FOR_STATIONARY_BOUNCE_BOOST = 0.5f;

// ---- the NON-const group (DecFIGS BrnCrashPlayManager.cpp :37..:64) -----------------------------
// Read out of ARTIST's data at 0x82CDB508.., one 4-byte slot per declaration, in order. See the
// file banner for how the five zero-valued ones are attributed. They are non-const in the original
// (they are the designers' live-tunable set) and are kept non-const here for the same reason.
static f32 KF_BOOST_FOR_VEHICLE_IMPACT_LOW    =   20.0f;   // 0x82CDB508
static f32 KF_BOOST_FOR_VEHICLE_IMPACT_HIGH   =   35.0f;   // 0x82CDB50C
static f32 KF_BOOST_FOR_EVERY_10_CARS_HIT_LO  =   50.0f;   // 0x82CDB510
static f32 KF_BOOST_FOR_EVERY_10_CARS_HIT_HI  =   20.0f;   // 0x82CDB514
static f32 KF_BOOST_FOR_OVERHEAD_SIGN         =   25.0f;   // 0x82CDB518
static f32 KF_BOOST_FOR_INITIAL_AIRTIME       =   40.0f;   // 0x82CDB51C
static f32 KF_BOOST_FOR_NEW_ROAD              =    0.0f;   // 0x82FAD2F4 -- tuned OFF, see banner
static f32 KF_BOOST_FOR_NEW_JUNCTION          =    0.0f;   // 0x82FAD2F8 -- tuned OFF
static f32 KF_BOOST_FOR_DISTANCE_TRAVELLED    =    0.0f;   // 0x82FAD2FC -- tuned OFF
static f32 KF_MINIMUM_BOUNCE_BOOST_TIME       =    0.3f;   // 0x82CDB520
static f32 KF_MAXIMUM_BOUNCE_BOOST_TIME       =    1.0f;   // 0x82CDB524
static f32 KF_COST_FOR_BOUNCE_BOOST_EASY      =   10.0f;   // 0x82CDB528
static f32 KF_COST_FOR_BOUNCE_BOOST_HARD      =   20.0f;   // 0x82CDB52C
static f32 KF_COST_FOR_BEING_ON_GROUND        =   20.0f;   // 0x82CDB530 -- percent per SECOND
static f32 KF_COST_FOR_1_BOUNCE_ON_GROUND     =    0.0f;   // 0x82FAD300 -- tuned OFF
static f32 KF_COST_FOR_2_BOUNCES_ON_GROUND    =    0.0f;   // 0x82FAD304 -- tuned OFF
static f32 KF_INITIAL_MIN_BOOST               =   51.0f;   // 0x82CDB534
static f32 KF_LOSE_BOOST_GRACE_PERIOD         =    0.25f;  // 0x82CDB538
static f32 KF_INITIAL_GRACE_PERIOD            =   10.0f;   // 0x82CDB53C
static f32 KF_TIME_ON_GROUND_NO_PENALTY       =    1.0f;   // 0x82CDB540
static f32 KF_TIME_ON_GROUND_PROMPT_NEEDED    =    0.4f;   // 0x82CDB544
static f32 KF_TIME_NO_GROUND_TO_COUNT_AS_AIR  =    0.5f;   // 0x82CDB548
static f32 KF_EASY_TO_HARD_OVER_N_VEHICLES    =  100.0f;   // 0x82CDB54C
static s32 KI_AWARD_BOOST_EVERY_N_VEHICLES    =     10;    // 0x82CDB550 (dword)
static f32 KF_MIN_TRAFFIC_DENSITY             =    0.4f;   // 0x82CDB554
static f32 KF_MAX_TRAFFIC_DENSITY             =    1.0f;   // 0x82CDB558
static f32 KF_LOWER_LIMIT_VEHICLE_SCORE       = 1000.0f;   // 0x82CDB55C
static f32 KF_UPPER_LIMIT_VEHICLE_SCORE       = 5000.0f;   // 0x82CDB560

// The game-event ids this TU posts. Each is the literal in the `li r5,<id>` immediately before the
// AddEvent call, paired with the `li r6,<size>` that follows it -- which is also what pins each
// payload's size. They are NOT in the tree's EGameEventType enum yet (no consumer arm exists), so
// they are spelled here rather than invented into that enum.
static const s32 KI_EVENT_TRIGGER_CRASH_BREAKER   = 44;   // size 32, UpdateTrafficStomp
static const s32 KI_EVENT_VEHICLE_LEAPT           = 47;   // size 4,  UpdateCarLeaping
static const s32 KI_EVENT_ENTER_NEW_ROAD          = 48;   // size 1,  UpdateNewRoad (both posts)
static const s32 KI_EVENT_SHOWTIME_BOUNCE_PROMPT  = 51;   // size 1,  SetBouncePromptNeeded

// The three payloads. Each is exactly the frame the console hands to AddEvent, store for store; the
// sizes are the `li r6` operands. The two single-bool records are posted with size 1, so they carry
// no padding of their own.
struct VehicleLeaptEvent            { s32  miNumVehiclesLeapt; };   // 47, size 4
struct EnterNewRoadEvent            { bool mbIsJunction;       };   // 48, size 1
struct ShowtimeBouncePromptEvent    { bool mbPromptNeeded;     };   // 51, size 1

// 44, size 32. Built at var_D0 in UpdateTrafficStomp @0x822F9088..0x822F90A8:
//   stvx128 (player position)      -> +0x00   16 bytes
//   stw     (ActiveRaceCarIndex)   -> +0x10
//   stfs    1.0f                   -> +0x14
//   stfs    0.0f                   -> +0x18
//   stfs    1.0f                   -> +0x1C
// ⚠️ FLAG: only the first two fields have attested NAMES (the position comes straight from
// ActiveRaceCar::GetPosition, the index from meActiveRaceCarIndex @+0x748). The three trailing
// floats are stored as bare literals with no name evidence anywhere in ARTIST, so they keep
// FieldNN names per this tree's convention rather than being guessed. DELETE-WHEN
// CrashModeScoringRecentCrash::DealWithCrashbreakerRequest @0x82320EB8 is reconstructed -- it is
// the consumer and it will name them.
struct TriggerCrashBreakerEvent
{
    Vector3               mEpicentre;              // +0x00
    ::EActiveRaceCarIndex meActiveRaceCarIndex;    // +0x10
    f32                   mfField14;               // +0x14  literal 1.0f
    f32                   mfField18;               // +0x18  literal 0.0f
    f32                   mfField1C;               // +0x1C  literal 1.0f
};

// =================================================================================================
// Construct  (DWARF BrnCrashPlayManager.h:58) -- NO out-of-line X360 symbol: ARTIST inlines it into
// RaceCarEntityModule::Construct, which is where the by-value member lives.
//
// WHY IT HAD TO LAND WITH THIS TU, measured. Without it the run at
// scratch/flow_run/cp1 fired the CONSOLE'S OWN assert 7,050 times -- once per Update frame:
//     [ASSERT] mpCrashPlayManager != NULL (BrnCrashPlayDebugComponent.cpp:111)
// That assert is genuine console code (ARTIST CrashPlayDebugComponent::Update @0x822A81B0 is
// nothing BUT that test: `lwz r11,0xC(r3) / cmplwi / bne` around the Begin/Fire/End sequence). It
// cannot fire on the console because the manager is constructed; it fired here because nothing
// ever set the debug component's back-pointer. 3,316 of those asserts had to be dismissed by the
// harness during that one run.
//
// WHAT IS ATTESTED, AND WHAT IS NOT:
//   * The six calls the DecFIGS DWARF lists as this function's callees are all here, in its order:
//     Vector3::SetZero, FixedRingBuffer<EntityId,32>::Construct + RingBuffer::Clear,
//     VolumeInstanceId::SetInvalid, FixedRingBuffer<EntityId,8>::Construct + RingBuffer::Clear.
//   * FLAG: mCrashPlayDebugComponent.Construct(this) is NOT in that callee list -- but it is two
//     stores, so it inlines away and would not appear there, Feb-2007's Construct makes exactly
//     this call, and the console's own assert PROVES something must set the pointer. The only
//     DWARF-declared setter is CrashPlayDebugComponent::Construct(CrashPlayManager*).
//   * FLAG: the scalar zeroing below is a value-initialisation, not an attested store list. The
//     class is carved out of module storage that nothing else writes, and the debug component
//     reads these members before the first Activate. Activate re-seeds every one of them.
//   * NOT reproduced: Feb-2007's `mbIsCrashPlayActive = true`. That is pre-merge drift -- on this
//     build Activate is what turns crash play on, and constructing it ON would make every car
//     permanently in crash play from boot.
// =================================================================================================
void CrashPlayManager::Construct()
{
    mCrashPlayDebugComponent.Construct( this );

    mPlayerCarVolumeInstanceID.SetInvalid();
    mLastPlayerPos.SetZero();

    mRecentCrashSet.Construct();
    mRecentCrashSet.Clear();
    mRecentLeaptSet.Construct();
    mRecentLeaptSet.Clear();

    miCarsLeaptThisFrame           = 0;
    miLastStreetEntered            = 0;
    mbSendNewRoadMessage           = false;
    muLastJunctionEnteredID        = 0;
    mbSendNewJunctionMessage       = false;
    mfCrashPlayTime                = 0.0f;
    mfTimeSinceLastInAir           = 0.0f;
    mfTimeSinceLastOnGround        = 0.0f;
    mfTimeSinceLastVehicleImpact   = KF_MIN_TIME_BETWEEN_AIR_RAMS;
    mfTimeSinceLastHitOverheadSign = KF_MIN_TIME_BETWEEN_HITTING_OVERHEAD_SIGNS;
    mbTrafficStomp                 = false;
    miFramesUntilAirRam            = 0;
    mfTimeSinceLastTrafficStomp    = KF_MIN_TIME_BETWEEN_TRAFFIC_STOMPS;

    mfBoostPercentage              = 0.0f;
    mfAftertouchPower              = 0.0f;
    mfDifficultyLevel              = 0.0f;
    mfBounceBoostTimer             = 0.0f;
    mfLoseBoostGracePeriod         = 0.0f;
    miConsecutiveBouncesOnGround   = 0;
    mbIsCrashPlayActive            = false;
    mbIsInShowtime                 = false;
    mbInfiniteAftertouch           = false;
    mbInfiniteBoost                = false;
    mbEarningAirTimeBoost          = false;
    mbAboutToLoseBoost             = false;
    mbBouncePromptNeeded           = false;
    mbBoostChargePending           = false;
}

// =================================================================================================
// Activate  @ 0x822C31A8
//
// ⚠️ BOTH PARAMETERS ARE UNUSED, AND THAT IS THE BINARY'S BEHAVIOUR, NOT A DROPPED SIDE EFFECT.
// The DWARF declares `Activate(ActiveRaceCar*, float32_t)`; across all 54 instructions ARTIST never
// reads r4, r5 or f1 -- there is no `mr` off them and no `stfs f1`. In particular the initial boost
// is NOT taken from lfInitialBoostPercentage: mfBoostPercentage is seeded from the file-scope
// KF_INITIAL_MIN_BOOST (flt_82CDB534 == 51.0f), which is also the floor Update re-applies for the
// whole KF_INITIAL_GRACE_PERIOD. The parameters are kept so the call site keeps the console's shape.
// =================================================================================================
void CrashPlayManager::Activate( ActiveRaceCar* /*lpPlayerActiveRaceCar*/,
                                 f32            /*lfInitialBoostPercentage*/ )
{
    mbIsCrashPlayActive           = true;
    mfCrashPlayTime               = 0.0f;
    mbEarningAirTimeBoost         = true;
    mfDifficultyLevel             = 0.0f;
    mfBounceBoostTimer            = 0.0f;
    miFramesUntilAirRam           = 0;
    miLastStreetEntered           = 0;
    mbSendNewRoadMessage          = false;
    mbSendNewJunctionMessage      = false;
    mbTrafficStomp                = false;
    mbBouncePromptNeeded          = false;
    mbBoostChargePending          = false;
    miConsecutiveBouncesOnGround  = 0;
    mfBoostPercentage             = KF_INITIAL_MIN_BOOST;
    mfTimeSinceLastOnGround       = 0.0f;
    mfTimeSinceLastInAir          = 0.0f;
    mLastPlayerPos.SetZero();
    mfAftertouchPower             = GetMaxAftertouchPower();

    // Each debounce timer starts AT its own threshold, so the first event of each kind is allowed
    // through immediately.
    mfTimeSinceLastHitOverheadSign = KF_MIN_TIME_BETWEEN_HITTING_OVERHEAD_SIGNS;
    mfTimeSinceLastTrafficStomp    = KF_MIN_TIME_BETWEEN_TRAFFIC_STOMPS;
    mfTimeSinceLastVehicleImpact   = KF_MIN_TIME_BETWEEN_AIR_RAMS;

    mRecentCrashSet.Clear();
    mRecentLeaptSet.Clear();

    // The console's tail: gate on `gxMessageFilterFlags & 1`, then the gpDebugPrint virtual.
    if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "SHOWTIME! CrashPlayManager::Activate called.\n";
    }
}

// =================================================================================================
// ClampBoostLevel  (DWARF BrnCrashPlayManager.h:225)
//
// No out-of-line X360 symbol: the console inlines it at all six sites, always as the same pair of
// `fsel`s -- `fneg f13,x / fsel f0,f13,ZERO,x` (== Max(x, 0)) then `fsubs f12,100.0,f0 /
// fsel f0,f12,f0,100.0` (== Min(f0, 100)). The DWARF names it as a callee of OnEnterRoad,
// OnEnterJunction, OnHitOverheadSign, OnVehicleHitConfirmed, OnBounce and UpdateMomentum, which is
// exactly the six sites, so the outlining is attested and not inferred.
// =================================================================================================
void CrashPlayManager::ClampBoostLevel()
{
    mfBoostPercentage = rw::math::fpu::Min( rw::math::fpu::Max( mfBoostPercentage, 0.0f ),
                                            KF_MAX_BOOST );
}

// =================================================================================================
// [crashplay] THE WITNESS  (BRN_CRASHPLAY_TRACE=1; flow_run.ps1 -DiagEnv "BRN_CRASHPLAY_TRACE=1")
// Opt-in harness instrument, NOT console code.
//
// It ACCUMULATES and never samples. Every quantity below is a running total taken on the frame the
// thing happens, so nothing can fall between two sampling periods -- the failure mode that has
// already cost this cluster one wave. The periodic line exists only to show the CURRENT meter
// alongside those totals; if the totals move and the meter does not, that is a real finding and
// not an artefact of the print period.
//
// The counters, and what each one PROVES if it is zero:
//   frames        Update ran at all (0 => the PrePhysicsUpdate call site never fires)
//   showtime      the showtime branch ran (0 => mbIsInShowtime is still false: the arming half)
//   press         mbBoostBounce arrived from the pad (0 => THE STIMULUS NEVER HAPPENED -- the
//                 harness, not the game, and no conclusion about the meter is admissible)
//   arm           a press was ACCEPTED (press > 0 but arm == 0 => every press landed inside the
//                 KF_MINIMUM_BOUNCE_BOOST_TIME re-arm window; pulse slower)
//   charge        OnBounce actually spent boost (arm > 0 but charge == 0 => the bounce game action
//                 never reaches OnBounce, i.e. the HandleGameActions arm is still parked)
//   ground        frames the player was charged KF_COST_FOR_BEING_ON_GROUND
//   spent         total percentage points removed by the ground cost
// =================================================================================================
namespace
{
    bool CrashPlayTraceOn()
    {
        static s32 siOn = -1;
        if( siOn < 0 )
        {
            const char* lpcEnv = getenv( "BRN_CRASHPLAY_TRACE" );
            siOn = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
        }
        return siOn == 1;
    }

    struct CrashPlayWitness
    {
        u32 muFrames;
        u32 muShowtimeFrames;
        u32 muPresses;
        u32 muArms;
        u32 muCharges;
        u32 muGroundFrames;
        f32 mfGroundSpent;
        f32 mfChargeSpent;
    };
    CrashPlayWitness gCrashPlayWitness = { 0u, 0u, 0u, 0u, 0u, 0u, 0.0f, 0.0f };
}

// =================================================================================================
// Update  @ 0x82306530  -- the per-frame spine, called from RaceCarEntityModule::PrePhysicsUpdate.
//
// ⭐ SIGNATURE RECOVERED FROM THE PROLOGUE, NOT THE PSEUDOCODE. Hex-Rays renders this as a nullary
// `int Update()` because the float32_t argument arrives in f1 and EATS the r5 GPR slot, so the
// remaining pointers land in r6/r7/r8 and the decompiler cannot see them as parameters at all. The
// asm is unambiguous: r3 == this, r4 == the Matrix44Affine (by hidden pointer -- the console loads
// its rows with `lvx128 v0,r0,r27` and `lvx128 v0,r27,32`, i.e. +0x00 and +0x20, which are xAxis and
// zAxis), f1 == lfSimTimerTimeStep, r6 == lpOutput, r7 == lpPlayerActiveRaceCar, r8 ==
// lpPlayerControls. That ordering is exactly the DWARF's, and the three null asserts fire on r6/r7/
// r8 in the order the DWARF lists them.
// =================================================================================================
void CrashPlayManager::Update( const Matrix44Affine& lCameraTransform,
                               f32                   lfSimTimerTimeStep,
                               RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                               ActiveRaceCar*         lpPlayerActiveRaceCar,
                               PlayerVehicleControls* lpPlayerControls )
{
    CGS_ASSERT( lpOutput != 0,              "lpOutput" );               // :262
    CGS_ASSERT( lpPlayerActiveRaceCar != 0, "lpPlayerActiveRaceCar" );  // :263
    CGS_ASSERT( lpPlayerControls != 0,      "lpPlayerControls" );       // :264

    // IsCrashing() carries the console's inlined IsAttached() assert (BrnActiveRaceCar.h:1418),
    // which is the assert ARTIST fires between these two byte tests.
    if( !mbIsCrashPlayActive || !lpPlayerActiveRaceCar->IsCrashing() )
    {
        return;
    }

    mfCrashPlayTime                += lfSimTimerTimeStep;
    mfTimeSinceLastVehicleImpact   += lfSimTimerTimeStep;
    mfTimeSinceLastTrafficStomp    += lfSimTimerTimeStep;
    mfTimeSinceLastHitOverheadSign += lfSimTimerTimeStep;

    // ⚠️⚠️ THE CONSOLE'S OWN :278 ASSERT, RAISED ONCE -- the same treatment, for the same reason,
    // that PrePhysicsUpdate's :1726 assert already carries in BrnRaceCarEntityModule.cpp.
    //
    // It is genuine console code and it is CORRECT: mPlayerCarVolumeInstanceID is invalid here
    // because NOTHING IN THIS TREE ASSIGNS IT. Construct() calls SetInvalid() (the DWARF's own
    // callee list) and no writer has landed, so the id stays -1 for the whole session. Before
    // Construct landed, the member held whatever the module's raw storage did -- usually 0, which
    // IsValid() happily accepts -- so the gap was INVISIBLE, not absent.
    //
    // Measured 2026-08-29 (scratch/flow_run/cp2): as a verbatim per-frame CGS_ASSERT this fires
    // 7,102 times in a 130 s run and the PC assert manager BLOCKS on each one, which starves the
    // harness poll loop badly enough that a 1 Hz input schedule delivered ONE press instead of 90.
    // An assert that stops the run from producing evidence is worse than useless.
    //
    // The consumer is UpdateTrafficStomp's air ram (CreateAirRamEvent::mVolumeId), which is
    // already unreachable on this build for an unrelated reason (mbTrafficStomp has no writer
    // until OnCarCrash lands), so nothing downstream is misled by the invalid id.
    // DELETE-WHEN the player car's volume instance is published to the crash-play manager.
    if( !mPlayerCarVolumeInstanceID.IsValid() )
    {
        static bool sbReportedInvalidVolumeId = false;
        if( !sbReportedInvalidVolumeId )
        {
            sbReportedInvalidVolumeId = true;
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] CrashPlayManager::Update: mPlayerCarVolumeInstanceID."
                       "IsValid() (X360 BrnCrashPlayManager.cpp:278) -- raised once, not per frame."
                       " No writer for the member exists in this tree yet.\n";
            }
        }
    }

    // The camera basis, flattened into the ground plane. The console builds both by storing the
    // matrix row to the stack, zeroing its Y word in place, reloading it, and running the two-step
    // Newton-Raphson rsqrt that Normalize() de-optimises back to.
    Vector3 lCameraX = lCameraTransform.xAxis;
    lCameraX.y = 0.0f;
    CGS_ASSERT( rw::math::vpu::MagnitudeSquared( lCameraX ) > 0.0f,
                "RwMath::MagnitudeSquared(lCameraX) > 0.0f" );                                 // :290
    lCameraX = rw::math::vpu::Normalize( lCameraX );

    Vector3 lCameraZ = lCameraTransform.zAxis;
    lCameraZ.y = 0.0f;
    CGS_ASSERT( rw::math::vpu::MagnitudeSquared( lCameraZ ) > 0.0f,
                "RwMath::MagnitudeSquared(lCameraZ) > 0.0f" );                                 // :298
    lCameraZ = rw::math::vpu::Normalize( lCameraZ );

    UpdateMomentum( lfSimTimerTimeStep, lpPlayerActiveRaceCar, lpOutput );

    if( mbIsInShowtime )
    {
        UpdateTrafficStomp( lfSimTimerTimeStep, lpPlayerControls, lpPlayerActiveRaceCar, lpOutput,
                            lCameraX, lCameraZ );
        UpdateBounceBoost( lfSimTimerTimeStep, lpPlayerControls, lpPlayerActiveRaceCar, lpOutput );
    }

    UpdateCarLeaping( lfSimTimerTimeStep, lpOutput );
    UpdateNewRoad( lfSimTimerTimeStep, lpOutput );

    if( mbInfiniteAftertouch )
    {
        mfAftertouchPower = GetMaxAftertouchPower();
    }

    if( mbInfiniteBoost )
    {
        mfBoostPercentage = KF_MAX_BOOST;
    }

    // ⭐ The opening grace period: for the first KF_INITIAL_GRACE_PERIOD seconds of a crash-play
    // session the aftertouch is held at full and the boost meter cannot fall below
    // KF_INITIAL_MIN_BOOST. This is why a fresh showtime session survives its first ~10 s even
    // though UpdateMomentum is already charging KF_COST_FOR_BEING_ON_GROUND against it.
    if( mfCrashPlayTime <= KF_INITIAL_GRACE_PERIOD )
    {
        mfAftertouchPower = GetMaxAftertouchPower();
        mfBoostPercentage = rw::math::fpu::Max( mfBoostPercentage, KF_INITIAL_MIN_BOOST );
    }

    // [crashplay] witness -- see the banner above the function. Not console code.
    ++gCrashPlayWitness.muFrames;
    if( mbIsInShowtime )
    {
        ++gCrashPlayWitness.muShowtimeFrames;
    }
    if( CrashPlayTraceOn() && ( gCrashPlayWitness.muFrames % 30u ) == 0u
        && CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[crashplay] frames=" << static_cast<s32>( gCrashPlayWitness.muFrames )
            << " showtime=" << static_cast<s32>( gCrashPlayWitness.muShowtimeFrames )
            << " press=" << static_cast<s32>( gCrashPlayWitness.muPresses )
            << " arm=" << static_cast<s32>( gCrashPlayWitness.muArms )
            << " charge=" << static_cast<s32>( gCrashPlayWitness.muCharges )
            << " ground=" << static_cast<s32>( gCrashPlayWitness.muGroundFrames )
            << " | boost=" << mfBoostPercentage
            << " bounceTimer=" << mfBounceBoostTimer
            << " aftertouch=" << mfAftertouchPower
            << " tInAir=" << mfTimeSinceLastInAir
            << " tOnGround=" << mfTimeSinceLastOnGround
            << " crashPlayTime=" << mfCrashPlayTime
            << " groundSpent=" << gCrashPlayWitness.mfGroundSpent
            << " chargeSpent=" << gCrashPlayWitness.mfChargeSpent
            << "\n";
    }

    // ARTIST tail-calls the debug component's vtable slot 0 through `lwz r11,0(r30) / lwz r11,0(r11)
    // / bctrl` with r3 still == this. Slot 0 of CgsDev::DebugComponent is Update(), and the
    // component is this class's member at +0x000, so `this` doubles as `&mCrashPlayDebugComponent`.
    mCrashPlayDebugComponent.Update();
}

// =================================================================================================
// UpdateMomentum  @ 0x823020D0  -- ⭐⭐⭐ THE ONLY THING THAT SPENDS SHOWTIME BOOST WITHOUT INPUT.
//
// Once the opening grace period is over, a showtime player who is lying on the ground loses
// KF_COST_FOR_BEING_ON_GROUND (20) percent of the meter PER SECOND. That is the drain the whole
// end-of-session ladder waits on, and it is the reason a session that used to run forever now
// terminates: the bar reaches zero in ~2.5 s of continuous ground contact past the grace period,
// unless the player keeps bouncing.
// =================================================================================================
void CrashPlayManager::UpdateMomentum( f32 lfSimTimerTimeStep,
                                       ActiveRaceCar* lpPlayerActiveRaceCar,
                                       RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    const Vector3 lPlayerPosition = lpPlayerActiveRaceCar->GetPosition();

    // The very first frame after Activate has mLastPlayerPos still zeroed, and the console skips
    // the distance award on exactly that frame (`vcmpgtfp` of |mLastPlayerPos| against a splatted
    // FLT_EPSILON, with the sign bit cleared by `vandc`, then the `vrlimi128` that folds the X and
    // Z lanes together). Reconstructed as the vector IsZero the DWARF names.
    if( !rw::math::vpu::IsZero( mLastPlayerPos ) )
    {
        const f32 lfDistanceTravelledLastFrame =
                BrnMath::Magnitude2D( rw::math::vpu::Subtract( lPlayerPosition, mLastPlayerPos ) );
        mfBoostPercentage += KF_BOOST_FOR_DISTANCE_TRAVELLED * lfDistanceTravelledLastFrame;
    }
    mLastPlayerPos = lPlayerPosition;

    if( lpPlayerActiveRaceCar->GetPhysicsState()->mfTimeInAir > 0.0f )
    {
        mfTimeSinceLastOnGround += lfSimTimerTimeStep;

        if( mfTimeSinceLastOnGround > KF_TIME_NO_GROUND_TO_COUNT_AS_AIR )
        {
            mfTimeSinceLastInAir = 0.0f;
            SetBouncePromptNeeded( false, lpOutput );
        }
    }
    else
    {
        mfTimeSinceLastOnGround = 0.0f;
        mfTimeSinceLastInAir   += lfSimTimerTimeStep;

        if( mfTimeSinceLastInAir > KF_TIME_ON_GROUND_NO_PENALTY )
        {
            mfBoostPercentage -= KF_COST_FOR_BEING_ON_GROUND * lfSimTimerTimeStep;
            ++gCrashPlayWitness.muGroundFrames;                                       // [crashplay]
            gCrashPlayWitness.mfGroundSpent += KF_COST_FOR_BEING_ON_GROUND * lfSimTimerTimeStep;
        }

        if( mfTimeSinceLastInAir > KF_TIME_ON_GROUND_PROMPT_NEEDED )
        {
            SetBouncePromptNeeded( mfBoostPercentage > 0.0f, lpOutput );
        }
    }

    // The one-shot airtime award: it runs while the car is still off the ground after Activate and
    // latches off the moment it touches down.
    if( mbEarningAirTimeBoost )
    {
        if( lpPlayerActiveRaceCar->GetPhysicsState()->mfTimeInAir > 0.0f )
        {
            mfBoostPercentage += KF_BOOST_FOR_INITIAL_AIRTIME * lfSimTimerTimeStep;
            ClampBoostLevel();
        }
        else
        {
            mbEarningAirTimeBoost = false;
        }
    }

    // The deferred bounce penalty armed by OnBounce. Both costs are 0.0f in the shipped tuning (see
    // the file banner), so on this build the arm runs and subtracts nothing -- faithfully.
    if( mbAboutToLoseBoost )
    {
        mfLoseBoostGracePeriod -= lfSimTimerTimeStep;

        if( mfLoseBoostGracePeriod <= 0.0f )
        {
            mbAboutToLoseBoost = false;
            mfBoostPercentage -= ( miConsecutiveBouncesOnGround == 1 )
                                 ? KF_COST_FOR_1_BOUNCE_ON_GROUND
                                 : KF_COST_FOR_2_BOUNCES_ON_GROUND;
        }
    }

    ClampBoostLevel();

    // Aftertouch bleed. See the KF_AFTERTOUCH_*_DECAY_TIME note above for which arm is which.
    const f32 lfDecayRate = rw::math::fpu::IsZero( mfTimeSinceLastInAir )
                            ? KF_AFTERTOUCH_AIR_DECAY_TIME
                            : KF_AFTERTOUCH_GROUND_DECAY_TIME;
    mfAftertouchPower = rw::math::fpu::Max( mfAftertouchPower - ( lfSimTimerTimeStep * lfDecayRate ),
                                            0.0f );

    // ...and a second, faster bleed once the meter is empty, so a spent player loses steering too.
    if( rw::math::fpu::IsZero( mfBoostPercentage ) )
    {
        mfAftertouchPower = rw::math::fpu::Max(
                mfAftertouchPower - ( lfSimTimerTimeStep * KF_AFTERTOUCH_NO_BOOST_DECAY_TIME ),
                0.0f );
    }
}

// =================================================================================================
// UpdateBounceBoost  @ 0x822A7CD0  -- ⭐⭐⭐ THE PRODUCER OF mfBounceBoostTimer.
//
// This is the function whose absence broke the whole loop. RaceCarEntityModule::
// ProcessPlayerVehicleInput copies CrashPlayManager::IsBounceBoosting() (== mfBounceBoostTimer >
// 0.0f) into the physics side's PlayerDriverControls::mbBoostBounce; nothing else writes that timer.
//
// ⚠️ THE BUTTON IT READS IS THE *INPUT* PlayerVehicleControls::mbBoostBounce (+0x3B), which is the
// RAW request the pad produces -- CgsInputPadsPC maps action 3 BOOST muStatus bit1 onto it. It is
// NOT the same field ProcessPlayerVehicleInput writes: that one is on the physics-side
// BrnPlayerDriverControls (+0x3F), a different type. There is no cycle here.
// =================================================================================================
void CrashPlayManager::UpdateBounceBoost( f32 lfSimTimerTimeStep,
                                          PlayerVehicleControls* lpPlayerControls,
                                          ActiveRaceCar*         /*lpPlayerActiveRaceCar*/,
                                          RaceCarEntityModuleIO::OutputBuffer_PrePhysics* /*lpOutput*/ )
{
    // An empty meter cancels an in-flight bounce outright.
    if( rw::math::fpu::IsZero( mfBoostPercentage ) )
    {
        mfBounceBoostTimer = 0.0f;
    }
    // Otherwise a fresh press re-arms the timer, but only once the previous bounce has run most of
    // its course -- KF_MINIMUM_BOUNCE_BOOST_TIME is a rate limiter on mashing the button.
    else if( lpPlayerControls->mbBoostBounce
             && mfBounceBoostTimer < KF_MINIMUM_BOUNCE_BOOST_TIME )
    {
        mfBounceBoostTimer   = KF_MAXIMUM_BOUNCE_BOOST_TIME;
        mbBoostChargePending = true;
        ++gCrashPlayWitness.muArms;                                                   // [crashplay]
    }

    if( lpPlayerControls->mbBoostBounce )
    {
        ++gCrashPlayWitness.muPresses;                                                // [crashplay]
    }

    if( mfBounceBoostTimer > 0.0f )
    {
        mfBounceBoostTimer -= lfSimTimerTimeStep;
    }
}

// =================================================================================================
// UpdateTrafficStomp  @ 0x822F9020
//
// ⚠️ lCameraX IS GENUINELY UNUSED HERE. The DWARF declares both camera axes and Update passes both,
// but ARTIST captures only the second vector register (`vmr128 v127, v2`) and never reads the first
// across the whole body. That is the binary's behaviour, not a dropped side effect -- checked the
// way the previous wave's DeformationManager::ReadPotentialContact finding says to check it, by
// looking at what the prologue actually captures rather than at the declaration.
// =================================================================================================
void CrashPlayManager::UpdateTrafficStomp( f32 /*lfSimTimerTimeStep*/,
                                           PlayerVehicleControls* /*lpPlayerControls*/,
                                           ActiveRaceCar* lpPlayerActiveRaceCar,
                                           RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                                           Vector3 /*lCameraX*/,
                                           Vector3 lCameraZ )
{
    if( mbTrafficStomp )
    {
        mbTrafficStomp = false;

        TriggerCrashBreakerEvent lCrashBreakerGameEvent;
        lCrashBreakerGameEvent.mEpicentre           = lpPlayerActiveRaceCar->GetPosition();
        lCrashBreakerGameEvent.meActiveRaceCarIndex = lpPlayerActiveRaceCar->GetActiveRaceCarIndex();
        lCrashBreakerGameEvent.mfField14            = 1.0f;
        lCrashBreakerGameEvent.mfField18            = 0.0f;
        lCrashBreakerGameEvent.mfField1C            = 1.0f;
        lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lCrashBreakerGameEvent ),
                KI_EVENT_TRIGGER_CRASH_BREAKER,
                32 );

        // A stomp refills the meter completely and restores full aftertouch.
        mfBoostPercentage   = KF_MAX_BOOST;
        mfAftertouchPower   = GetMaxAftertouchPower();
        miFramesUntilAirRam = KI_TRAFFIC_STOMP_AIR_RAM_FRAME_DELAY;
    }

    if( miFramesUntilAirRam > 0 )
    {
        --miFramesUntilAirRam;

        if( miFramesUntilAirRam == 0 )
        {
            const f32 lfAirRamPower = lpPlayerActiveRaceCar->IsTouchingWorld()
                                      ? KF_TRAFFIC_STOMP_GROUND_AIR_RAM_POWER
                                      : KF_TRAFFIC_STOMP_FLYING_AIR_RAM_POWER;

            const Vector3 lImpulse = rw::math::vpu::Normalize(
                    rw::math::vpu::Add( lCameraZ, K_WORLD_Y_AXIS ) );

            // The console inlines VehicleEffectsInputInterface::CreateAirRam (DWARF :94) down to the
            // event it builds, then AddEventSafe. Reproduced field for field; the magnitude rides in
            // the direction vector's fourth lane, which is what Vector3Plus is for.
            BrnPhysics::Vehicle::CreateAirRamEvent lAirRamEvent;
            lAirRamEvent.mVolumeId       = mPlayerCarVolumeInstanceID;
            lAirRamEvent.muEffectFlags   = 6;
            lAirRamEvent.mfDecay         = KF_TRAFFIC_STOMP_DECAY_RATE;
            lAirRamEvent.mDirectionAndMagnitude.SetVector3( lImpulse );
            lAirRamEvent.mDirectionAndMagnitude.SetPlus( lfAirRamPower );
            lAirRamEvent.mPosition.SetZero();
            lAirRamEvent.mfStartTime     = 0.0f;

            lpOutput->GetVehicleEffectsInterface()->GetAirRamEventQueue()->AddEventSafe( lAirRamEvent );
        }
    }
}

// =================================================================================================
// UpdateCarLeaping  @ 0x822F91D0   (lfSimTimerTimeStep unused -- f1 is never read)
// =================================================================================================
void CrashPlayManager::UpdateCarLeaping( f32 /*lfSimTimerTimeStep*/,
                                         RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    if( miCarsLeaptThisFrame > 0 )
    {
        VehicleLeaptEvent lLeaptEvent;
        lLeaptEvent.miNumVehiclesLeapt = miCarsLeaptThisFrame;
        lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lLeaptEvent ),
                KI_EVENT_VEHICLE_LEAPT,
                4 );
        miCarsLeaptThisFrame = 0;
    }
}

// =================================================================================================
// UpdateNewRoad  @ 0x822F9228  -- both posts use the SAME event id and record, distinguished only by
// the single payload byte. (lfSimTimerTimeStep unused.)
// =================================================================================================
void CrashPlayManager::UpdateNewRoad( f32 /*lfSimTimerTimeStep*/,
                                      RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    if( mbSendNewRoadMessage )
    {
        EnterNewRoadEvent lEnterRoadEvent;
        lEnterRoadEvent.mbIsJunction = false;
        lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lEnterRoadEvent ),
                KI_EVENT_ENTER_NEW_ROAD,
                1 );
        mbSendNewRoadMessage = false;
    }

    if( mbSendNewJunctionMessage )
    {
        EnterNewRoadEvent lEnterJunctionEvent;
        lEnterJunctionEvent.mbIsJunction = true;
        lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lEnterJunctionEvent ),
                KI_EVENT_ENTER_NEW_ROAD,
                1 );
        mbSendNewJunctionMessage = false;
    }
}

// =================================================================================================
// SetBouncePromptNeeded  @ 0x822F92A8  -- edge-triggered: the event only goes out on a change.
// =================================================================================================
void CrashPlayManager::SetBouncePromptNeeded( bool lbPromptNeeded,
                                              RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    if( lbPromptNeeded != mbBouncePromptNeeded )
    {
        ShowtimeBouncePromptEvent lPromptEvent;
        lPromptEvent.mbPromptNeeded = lbPromptNeeded;
        lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lPromptEvent ),
                KI_EVENT_SHOWTIME_BOUNCE_PROMPT,
                1 );
        mbBouncePromptNeeded = lbPromptNeeded;
    }
}

// =================================================================================================
// OnCarCrash  @ 0x822C3280  -- the per-car-hit reward, and the recently-hit DEBOUNCE SET.
//
// (Landed 2026-08-29. It used to be a trap stub in WorldLinkStubs.cpp; that stub is now deleted.
// The ledger files this function under GameShared/GameClasses/Containers/CgsRingBuffer.h because
// its only callees are the inlined ring-buffer methods -- the same misfiling BoostStrategy::
// UpdateChainExploits carries. Its real home is this .cpp: the DecFIGS dwarfdump declares the body
// here, with `int32_t liCrashSetIndex` at BrnCrashPlayManager.cpp:689.)
//
// ⛔⛔ THE STUB'S OWN COMMENT WAS WRONG ON THE POINT IT WAS WRITTEN FOR. It said this function
// "WOULD: push the hit vehicle into mRecentCrashSet, award the per-car boost, and set
// mbTrafficStomp -- which is why the traffic-stomp air ram is currently unreachable". It sets
// NEITHER mbTrafficStomp NOR mfBoostPercentage. The whole body is four stores wide and the asm is
// unambiguous: the only fields it touches are mRecentCrashSet, mfAftertouchPower (+0x138) and
// mfTimeSinceLastVehicleImpact (+0x120).
//
// ⭐ And mbTrafficStomp has NO true-writer ANYWHERE IN ARTIST. Exhaustive scan of all 30,084
// exported functions for a byte store at CrashPlayManager +0x128, and separately for the
// module-relative form (module + 0x18218, in every PPC encoding: `lis/ori 0x18218`, `addis rX,rY,2`
// + `stb ..., -0x7DE8(rX)`, and the literal 98840): the ONLY writers are Activate @0x822C31A8
// (stores 0 -- the init) and UpdateTrafficStomp @0x822F9074 (stores 0 -- the consume). Nothing sets
// it. So the traffic-stomp air ram is dead code in the shipped X360 build too, and landing this
// body does not -- and cannot -- wake it. Whatever gates showtime's stomp on the console, it is not
// this member and not this function. DO NOT spend another wave looking for the writer here.
//
// SHAPE (asm 0x822C3280..0x822C333C, 48 instructions):
//   if (!mbIsCrashPlayActive) return;                  `lbz r11, 0x14C(r31)` + beq
//   if (!lbPlayerHitCar)      return;                  `clrlwi r11, r5, 24` + beq
//   for (i = 0; i < mRecentCrashSet.GetLength(); ++i)  `lwz r11, 0x40(r31)` == miLength
//       if (mRecentCrashSet[i] == lHitVehicleID) return;   -- already counted, no double reward
//   mRecentCrashSet.Push(&lHitVehicleID);
//   mfAftertouchPower = Min(mfAftertouchPower + KF_AFTERTOUCH_FOR_CAR_IMPACT,
//                           GetMaxAftertouchPower());
//   if (mfTimeSinceLastVehicleImpact >= KF_MIN_TIME_BETWEEN_AIR_RAMS)
//       mfTimeSinceLastVehicleImpact = 0.0f;
//
// The `fsel f0, f12, f13, f0` at 0x822C3324 with f12 == (mfAftertouchPower + 0.5f) - 1.0f IS
// rw::math::fpu::Min<float> -- the DWARF's callee list for this function names Min<float>
// explicitly, which is what turns a branchless select back into the call.
//
// The two literals: flt_820147FC == 0.5f is KF_AFTERTOUCH_FOR_CAR_IMPACT (the DWARF declares that
// constant immediately before KF_AFTERTOUCH_FOR_STATIONARY_BOUNCE_BOOST, which is the other 0.5f
// site in this file), and flt_82CDB648 == 1.2f is KF_MIN_TIME_BETWEEN_AIR_RAMS, already named at
// the top of this file from Activate's seed of the same field.
//
// The reset is deliberately one-sided: mfTimeSinceLastVehicleImpact only restarts once the previous
// air-ram window has fully expired, so a burst of contacts in the same collision does not keep
// re-arming it. Activate seeds the field TO the threshold so the first hit of a session fires.
//
// DROPPED: the DecFIGS build's `CgsDev::StrStream::StrStream` block (a streamed assert message).
// ARTIST emits no assert here at all -- no BeginAssert/FireAssert/EndAssert triple anywhere in the
// 48 instructions -- so it is compiled out in the target build. Rung 1 wins.
// =================================================================================================
void CrashPlayManager::OnCarCrash( CgsSceneManager::EntityId lHitVehicleID, bool lbPlayerHitCar )
{
    if( !mbIsCrashPlayActive || !lbPlayerHitCar )
    {
        return;
    }

    // Already in the recently-hit window -> this contact earns nothing.
    for( s32 liCrashSetIndex = 0;
         liCrashSetIndex < mRecentCrashSet.GetLength();
         ++liCrashSetIndex )
    {
        if( mRecentCrashSet[static_cast<u32>( liCrashSetIndex )] == lHitVehicleID )
        {
            return;
        }
    }

    mRecentCrashSet.Push( &lHitVehicleID );

    mfAftertouchPower = rw::math::fpu::Min( mfAftertouchPower + KF_AFTERTOUCH_FOR_CAR_IMPACT,
                                            GetMaxAftertouchPower() );

    if( mfTimeSinceLastVehicleImpact >= KF_MIN_TIME_BETWEEN_AIR_RAMS )
    {
        mfTimeSinceLastVehicleImpact = 0.0f;
    }
}

// =================================================================================================
// HandlePlayerToVehicleImpact  @ 0x822D5928  -- the traffic-stomp DETECTOR.
//
// The final test is the DWARF's `Y*Y >= X*X + Z*Z` on the contact normal, which is "the player came
// down ON the car" rather than side-swiped it. ARTIST spells it with three `vspltw`s and a
// `vmaddfp`, and the DWARF's callee list for this function is exactly
// operator*<VectorAxisY,VectorAxisY>, operator*<VectorAxisZ,VectorAxisZ>,
// operator*<VectorAxisX,VectorAxisX>, operator+ and operator>= -- which is what fixes the operand
// roles in the `vmaddfp`.
// =================================================================================================
void CrashPlayManager::HandlePlayerToVehicleImpact(
        ActiveRaceCar*            lpPlayerActiveRaceCar,
        CgsSceneManager::EntityId lHitVehicleID,
        const BrnPhysics::ContactSpy::RaceCarContact* lpContact )
{
    if( !mbIsCrashPlayActive || !lpPlayerActiveRaceCar->IsCrashing() )
    {
        return;
    }

    const f32 lfCrashMagnitude = rw::math::vpu::Magnitude( lpContact->mNormalStress );
    if( lfCrashMagnitude < KF_MIN_CRASH_MAGNITUDE_REACTION )
    {
        return;
    }

    OnCarCrash( lHitVehicleID, true );

    if( mfTimeSinceLastTrafficStomp >= KF_MIN_TIME_BETWEEN_TRAFFIC_STOMPS
        && lHitVehicleID.GetOwner() == 2
        && BrnTraffic::GetVehicleSpecies( lHitVehicleID.GetEntityIndex() ) != 1 )
    {
        const Vector3& lrNormal = lpContact->mNormal;

        if( ( lrNormal.y * lrNormal.y )
            >= ( lrNormal.x * lrNormal.x ) + ( lrNormal.z * lrNormal.z ) )
        {
            mfTimeSinceLastTrafficStomp = 0.0f;
        }
    }
}

// =================================================================================================
// OnBounce  @ 0x822A7EF8  -- ⭐⭐ THE SPEND. Every bounce that was charged by UpdateBounceBoost
// costs a difficulty-lerped slice of the meter, which is the whole showtime economy in one line.
// =================================================================================================
void CrashPlayManager::OnBounce( const BrnGameState::GameStateModuleIO::JustBouncedAction* lpBounceAction )
{
    if( IsBounceBoosting() && mbBoostChargePending )
    {
        mbBoostChargePending = false;

        const f32 lfBoostCost = KF_COST_FOR_BOUNCE_BOOST_EASY
                              + ( ( KF_COST_FOR_BOUNCE_BOOST_HARD - KF_COST_FOR_BOUNCE_BOOST_EASY )
                                  * mfDifficultyLevel );
        mfBoostPercentage -= lfBoostCost;
        ClampBoostLevel();
        ++gCrashPlayWitness.muCharges;                                                // [crashplay]
        gCrashPlayWitness.mfChargeSpent += lfBoostCost;
    }

    if( lpBounceAction->mbOnCar )
    {
        // Bouncing off another car does not count as being on the ground at all.
        miConsecutiveBouncesOnGround = 0;
        mbAboutToLoseBoost           = false;
    }
    else
    {
        ++miConsecutiveBouncesOnGround;
        CGS_ASSERT( miConsecutiveBouncesOnGround > 0, "miConsecutiveBouncesOnGround > 0" );   // X360 :814

        if( !mbAboutToLoseBoost )
        {
            mfLoseBoostGracePeriod = KF_LOSE_BOOST_GRACE_PERIOD;
            mbAboutToLoseBoost     = true;
        }
    }

    if( lpBounceAction->mbFromStationary )
    {
        mfAftertouchPower = rw::math::fpu::Min(
                mfAftertouchPower + KF_AFTERTOUCH_FOR_STATIONARY_BOUNCE_BOOST,
                GetMaxAftertouchPower() );
    }
}

// =================================================================================================
// OnEnterRoad  @ 0x822A7D68   /   OnEnterJunction  @ 0x822A7E30
// Both awards are 0.0f in the shipped tuning (see the file banner); the message flags are the live
// half of these two handlers.
// =================================================================================================
void CrashPlayManager::OnEnterRoad(
        const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction* lpRRAction )
{
    if( !mbIsCrashPlayActive )
    {
        return;
    }

    CGS_ASSERT( lpRRAction->mRoadId != 0, "lpRRAction->mRoadId != 0" );   // X360 :744

    if( lpRRAction->mRoadId != miLastStreetEntered )
    {
        mfBoostPercentage += KF_BOOST_FOR_NEW_ROAD;
        ClampBoostLevel();
        mbSendNewRoadMessage = true;
        miLastStreetEntered  = lpRRAction->mRoadId;
    }
}

void CrashPlayManager::OnEnterJunction(
        const BrnGameState::GameStateModuleIO::SendJunctionPlayerIsAtAction* lpJAction )
{
    if( !mbIsCrashPlayActive )
    {
        return;
    }

    CGS_ASSERT( lpJAction->muJunctionID != 0, "lpJAction->muJunctionID != 0" );   // X360 :773

    if( lpJAction->muJunctionID != muLastJunctionEnteredID )
    {
        mfBoostPercentage += KF_BOOST_FOR_NEW_JUNCTION;
        ClampBoostLevel();
        mbSendNewJunctionMessage = true;
        muLastJunctionEnteredID  = lpJAction->muJunctionID;
    }
}

// =================================================================================================
// OnHitOverheadSign  @ 0x822A8028  -- a 25% top-up, rate-limited to one per second.
// =================================================================================================
void CrashPlayManager::OnHitOverheadSign()
{
    if( mfTimeSinceLastHitOverheadSign < KF_MIN_TIME_BETWEEN_HITTING_OVERHEAD_SIGNS )
    {
        return;
    }

    mfAftertouchPower               = GetMaxAftertouchPower();
    mfBoostPercentage              += KF_BOOST_FOR_OVERHEAD_SIGN;
    mfTimeSinceLastHitOverheadSign  = 0.0f;
    ClampBoostLevel();
}

// =================================================================================================
// OnVehicleHitConfirmed  @ 0x822C3348  -- the scoring feed: per-car boost, plus the difficulty ramp
// and its every-N-cars bonus.
//
// ⚠️ The two PPC trap instructions ARTIST emits around the `divw` (`twllei r10,0` /
// `twlgei r10,-1`) are the compiler's own divide-by-zero and INT_MIN/-1 guards for the `%`, not
// source-level behaviour, and are de-optimised away here per the AGENTS.md de-optimisation rule.
// =================================================================================================
void CrashPlayManager::OnVehicleHitConfirmed( s32 liVehicleBaseScore,
                                              s32 liVehicleChainBonus,
                                              s32 liTotalVehiclesHit )
{
    // Only an UNCHAINED hit pays the per-vehicle award.
    if( liVehicleChainBonus == 0 )
    {
        const f32 lfClampedScore = rw::math::fpu::Clamp( static_cast<f32>( liVehicleBaseScore ),
                                                         KF_LOWER_LIMIT_VEHICLE_SCORE,
                                                         KF_UPPER_LIMIT_VEHICLE_SCORE );
        const f32 lfScaledScore  = ( lfClampedScore - KF_LOWER_LIMIT_VEHICLE_SCORE )
                                 / ( KF_UPPER_LIMIT_VEHICLE_SCORE - KF_LOWER_LIMIT_VEHICLE_SCORE );
        const f32 lfBoost = KF_BOOST_FOR_VEHICLE_IMPACT_LOW
                          + ( lfScaledScore * ( KF_BOOST_FOR_VEHICLE_IMPACT_HIGH
                                                - KF_BOOST_FOR_VEHICLE_IMPACT_LOW ) );
        mfBoostPercentage += lfBoost;
        ClampBoostLevel();
    }

    if( liTotalVehiclesHit > 0 )
    {
        // The session gets harder as the pile grows: 0 at the first car, 1.0 once
        // KF_EASY_TO_HARD_OVER_N_VEHICLES have been hit. It scales both the bounce COST (OnBounce)
        // and the traffic density (GetShowtimeTrafficDensityScale).
        mfDifficultyLevel = rw::math::fpu::Clamp(
                static_cast<f32>( liTotalVehiclesHit ) / KF_EASY_TO_HARD_OVER_N_VEHICLES,
                0.0f, 1.0f );

        if( ( liTotalVehiclesHit % KI_AWARD_BOOST_EVERY_N_VEHICLES ) == 0 )
        {
            mfBoostPercentage += KF_BOOST_FOR_EVERY_10_CARS_HIT_LO
                               + ( ( KF_BOOST_FOR_EVERY_10_CARS_HIT_HI
                                     - KF_BOOST_FOR_EVERY_10_CARS_HIT_LO ) * mfDifficultyLevel );
            ClampBoostLevel();
        }
    }
}

// =================================================================================================
// GetShowtimeTrafficDensityScale  @ 0x822A8088  -- more traffic while the session is still easy.
// =================================================================================================
f32 CrashPlayManager::GetShowtimeTrafficDensityScale() const
{
    CGS_ASSERT( mfDifficultyLevel >= 0.0f && mfDifficultyLevel <= 1.0f, "mfDifficultyLevel>=0 && mfDifficultyLevel<=1.0f" );   // X360 :973

    const f32 lfScale = KF_MIN_TRAFFIC_DENSITY
                      + ( ( KF_MAX_TRAFFIC_DENSITY - KF_MIN_TRAFFIC_DENSITY )
                          * ( 1.0f - mfDifficultyLevel ) );

    CGS_ASSERT( lfScale >= 0.0f && lfScale <= 1.0f, "lfScale >= 0.0f && lfScale <= 1.0f" );   // X360 :977

    return lfScale;
}

}
