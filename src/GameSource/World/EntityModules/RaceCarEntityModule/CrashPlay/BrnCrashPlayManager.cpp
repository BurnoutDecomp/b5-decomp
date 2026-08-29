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

    CGS_ASSERT( mPlayerCarVolumeInstanceID.IsValid(), "mPlayerCarVolumeInstanceID.IsValid()" ); // :278

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
