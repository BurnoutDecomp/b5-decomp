#include "GameSource/Sound/Vehicles/Engines/BrnAIPhysicsControl.h"
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"                    // Car3DControl -> Cgs3dEffectControl upcast
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"          // RaceCarIsNowActive (message 18)
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehicleengineattribs.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/BrnAISoundDiag.h"   // [DIAG] NOT IN THE X360 BINARY

#include <cmath>
#include <cstring>

// =============================================================================
// BrnSound::Vehicles::Engines::AIPhysicsControl -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//   AIPhysicsControl::AIPhysicsControl        @ 0x826CE758
//   AIPhysicsControl::CreateObject            @ 0x826E4558
//   AIPhysicsControl::Attach                  @ 0x826CE860
//   AIPhysicsControl::UpdateParams            @ 0x826CE8D8
//   AIPhysicsControl::UpdateStartLineReving   @ 0x826CEC90
//   AIPhysicsControl::UpdateCollisionPassbys  @ 0x826B4DF8
//   AIPhysicsControl::UpdateAIPassbys         @ 0x826B4A98
//   AIPhysicsControl::PlayPassBy              @ 0x8269A4C0
//   AIPhysicsControl::Notify                  @ 0x8269A610
//   AIPhysicsControl::GetTypeName             @ 0x82685300
//   sTypeInfo registration                    @ 0x82C62560 (CRT init bank; desc 0x82F2F638)
//
// The file-scope constants (DWARF BrnAIPhysicsControl.cpp:34..52) with the values
// read out of the image (tools/re/x360rd.py) or out of their CRT init thunks
// (tools/re/findinit.py + ppcdis.py) where they live in .bss:
//   KF_MIN_THROTTLE_THRESHOLD        flt_82F2CD1C  = -1.0
//   KF_MAX_THROTTLE_THRESHOLD        flt_82F2CD20  = -0.5
//   KF_DECELERATION_COEFFICIENT /
//   KF_ACCCELERATION_COEFFICIENT     flt_82004E58  =  0.15   (one literal serves both)
//   KF_TIME_BETWEEN_AI_PASSBYS       flt_82F2CD24  =  2.0
//   KF_VOLUME_MODIFIER               flt_82F2CD28  =  2.5
//   KF_PASSBY_MIN_VELOCITY_THRESHOLD  unk_83005FC0 <- 0x82C62478: 40.0 mph * 0.44704
//   KF_NEARMISS_MIN_VELOCITY_THRESHOLD unk_830085C0 <- 0x82C624B0: 20.0 mph * 0.44704
//   KF_DISTANCE_TO_TRIGGER            unk_830080D0 <- 0x82C62510: 10.0
//   KF_NEARMISS_DISTANCE_TO_TRIGGER   unk_8300A6B0 <- 0x82C62538: 5.0
//   KB_DEBUG_AI_PASSYS                byte_82FFB86E (dev-only DebugRender text; not carried)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
static const f32 KF_MIN_THROTTLE_THRESHOLD          = -1.0f;
static const f32 KF_MAX_THROTTLE_THRESHOLD          = -0.5f;
static const f32 KF_DECELERATION_COEFFICIENT        = 0.15f;
static const f32 KF_ACCCELERATION_COEFFICIENT       = 0.15f;
static const f32 KF_TIME_BETWEEN_AI_PASSBYS         = 2.0f;
static const f32 KF_VOLUME_MODIFIER                 = 2.5f;
static const f32 KF_PASSBY_MIN_VELOCITY_THRESHOLD   = 40.0f * 0.44704f;
static const f32 KF_NEARMISS_MIN_VELOCITY_THRESHOLD = 20.0f * 0.44704f;
static const f32 KF_DISTANCE_TO_TRIGGER             = 10.0f;
static const f32 KF_NEARMISS_DISTANCE_TO_TRIGGER    = 5.0f;

// The listener the passby test measures against: Environment::mMicrophoneSystem
// .maMicrophones[E_MIC_PLAYER][E_PLAYER_1] (module + 0x2AF0; position at +0x30 of
// its matrix == module + 0x2B20, velocity == module + 0x2B80).
static const CgsSound::Logic::MicrophoneSystem::Microphone* AIListenerMicrophone( CgsSound::Logic::Module* apModule )
{
    return static_cast<BrnSound::Module::SoundLogicModule*>( apModule )
        ->GetEnvironment().GetMicrophoneSystem().GetMicrophone(
            CgsSound::Logic::MicrophoneSystem::E_MIC_PLAYER,
            CgsSound::Logic::MicrophoneSystem::E_PLAYER_1 );
}

// |a - b| over three lanes, with the console's `vcmpeqfp / vsel` zero guard: a
// zero-length vector yields exactly 0 instead of vrsqrtefp's infinity.
static f32 Length3( f32 afX, f32 afY, f32 afZ )
{
    const f32 lfSquared = afX * afX + afY * afY + afZ * afZ;
    return lfSquared == 0.0f ? 0.0f : std::sqrt( lfSquared );
}

static VecFloat Splat( f32 afValue )
{
    VecFloat lValue = { afValue, afValue, afValue, afValue };
    return lValue;
}
} // namespace

// ---------------------------------------------------------------------------
// AIPhysicsControl::AIPhysicsControl()  @ 0x826CE758
//   PhysicsControl::PhysicsControl(); install off_820AF4B4 / off_820AF480;
//   zero the four Average windows (their samples, cursors and means); Engine::
//   Construct(&mPhysicsEngine). The passby / occlusion tail is not seeded here
//   (Attach @0x826CE860 zeroes it).
// ---------------------------------------------------------------------------
AIPhysicsControl::AIPhysicsControl()
    : PhysicsControl()
    , mAverageRPM()
    , mAverageWheelVel()
    , mAverageVelocity()
    , mAverageSpeedMps()
    , mPhysicsEngine()
    , mfPassbyTriggered( 0.0f )
    , mfNearMissPassby( 0.0f )
    , mu8NotOccludedCount( 0 )
    , mbIsCurrentlyOccluded( false )
    , mbEnableOcclusion( false )
    , mbInsideRadius( false )
    , mfDiagAIWitnessTimer( 0.0f )
{
    mPhysicsEngine.Construct();
}

AIPhysicsControl::~AIPhysicsControl()
{
}

// @ 0x826E4558: MemBase::operator new(1264, "AIPhysicsControl", flavour) + ctor.
CgsSound::Logic::EffectControl* AIPhysicsControl::CreateObject( u32 /*luType*/ )
{
    return new AIPhysicsControl();
}

// Descriptor 0x82F2F638: {0x20000, "AIPhysicsControl", base PhysicsControl
// (0x82F2F578), &CreateObject}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* AIPhysicsControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x20000, "AIPhysicsControl", PhysicsControl::GetStaticTypeInfo(), &AIPhysicsControl::CreateObject );
    return &sTypeInfo;
}

// CRT init bank @0x82C62560: `addi r3, r11, 82F2F638 ; b EffectControl::AddToClassTypeInfoArray`.
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const
    gpAIPhysicsControlReg = CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(
        AIPhysicsControl::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* AIPhysicsControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// @ 0x82685300: returns off_82F2F63C == the descriptor's name.
const char* AIPhysicsControl::GetTypeName() const
{
    return "AIPhysicsControl";
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::Attach()  @ 0x826CE860  (sub-object vtable +0x14)
//   if (!PhysicsControl::Attach()) return 0;
//   mfPassbyTriggered = 0; mfNearMissPassby = 0; mu8NotOccludedCount = 0;
//   mbIsCurrentlyOccluded = mbEnableOcclusion = mbInsideRadius = 0; return 1;
// ---------------------------------------------------------------------------
bool AIPhysicsControl::Attach()
{
    if ( !PhysicsControl::Attach() )
        return false;
    mfPassbyTriggered     = 0.0f;
    mfNearMissPassby      = 0.0f;
    mu8NotOccludedCount   = 0;
    mbIsCurrentlyOccluded = false;
    mbEnableOcclusion     = false;
    mbInsideRadius        = false;
    mfDiagAIWitnessTimer  = 0.0f;
    return true;
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::UpdateParams(f32)  @ 0x826CE8D8  (sub-object vtable +0x18)
//
//   raw = mpVehiclePhysicsData;                                    ; lwz 0x34(this+4)
//   mAverageWheelVel.Record(min(raw->maWheels[2].mfRadiansPerSecond,     ; 0x124 / 0x194
//                              raw->maWheels[3].mfRadiansPerSecond));    ; fsel = the smaller
//   mAverageSpeedMps.Record(fabs(dot3(raw->mTransform.At(), raw->mLinearVelocity)));  ; 0x210 . 0x330
//   mPhysicsEngine.clutchFactor = 1.0 (vrlimi lane 0 of +0xB0); allow up/down gear changes;
//   Engine::Update(wheelAngVel = mAverageWheelVel.mean, gas = mThrottle.cur, brake = 0,
//                  handbrake = 0, steering = 0, rearWheelRadius = raw->maWheels[2].mfRadius,
//                  allowReverse = 0, forwardSpeed = mAverageSpeedMps.mean, dt);
//   mAverageVelocity.Record(raw->mLinearVelocity);
//   mAverageRPM.Record(mPhysicsEngine RPM lane);                    ; +0xB0 lane 1
//   raw->mfRPM = mAverageRPM.mean; raw->mi8Gear = mPhysicsEngine.gear;
//   raw->mLinearVelocity = mAverageVelocity.mean;
//   PhysicsControl::UpdateParams(dt);
//   <throttle ramp on mThrottle / mAccelerationMagnitude, see below>
//   <occlusion DMix slot 9 from mbIsCurrentlyOccluded / mu8NotOccludedCount>
//   mDeltaThrottle.Flush(mThrottle.cur - mThrottle.prev); mfRotation = 0;
//   mIsAccelerating.Update(prev); mIsAccelerating.Update(mThrottle.cur > 0.5);
//   UpdateAIPassbys(dt);
// ---------------------------------------------------------------------------
void AIPhysicsControl::UpdateParams( f32 afTimeStep )
{
    BrnSound::Vehicles::VehicleState* lpVehicleState =
        static_cast<BrnSound::Vehicles::VehicleState*>( GetStateBase() );
    BrnPhysics::Vehicle::RaceCarState* lpRaw = lpVehicleState->GetVehicleData();
    PhysicsData& lrData = mProcessedPhysicsData;

    // 0x826CE904..0x826CE914: f12 = w2 - w3 ; fsel f1, f12, w3, w2 -> the smaller.
    const f32 lfWheel2 = lpRaw->maWheels[2].mfRadiansPerSecond;
    const f32 lfWheel3 = lpRaw->maWheels[3].mfRadiansPerSecond;
    mAverageWheelVel.Record( ( lfWheel2 - lfWheel3 ) >= 0.0f ? lfWheel3 : lfWheel2 );

    const Vector3& lrForward  = lpRaw->mTransform.At();
    const Vector3& lrVelocity = lpRaw->mLinearVelocity;
    const f32 lfForwardSpeed = lrForward.x * lrVelocity.x + lrForward.y * lrVelocity.y + lrForward.z * lrVelocity.z;
    mAverageSpeedMps.Record( std::fabs( lfForwardSpeed ) );

    mPhysicsEngine.SetClutchFactor( 1.0f );
    mPhysicsEngine.SetAllowGearChanges( true );
    mPhysicsEngine.Update( Splat( mAverageWheelVel.GetAverage() ),
                           Splat( lrData.mThrottle.GetCurrent() ),
                           Splat( 0.0f ),
                           false,
                           Splat( 0.0f ),
                           Splat( lpRaw->maWheels[2].mfRadius ),
                           false,
                           Splat( mAverageSpeedMps.GetAverage() ),
                           Splat( afTimeStep ) );

    mAverageVelocity.Record( lpRaw->mLinearVelocity );
    mAverageRPM.Record( mPhysicsEngine.GetRPM() );

    lpRaw->mfRPM          = mAverageRPM.GetAverage();
    lpRaw->mi8Gear        = static_cast<s8>( mPhysicsEngine.GetCurrentGear() );
    lpRaw->mLinearVelocity = mAverageVelocity.GetAverage();

    PhysicsControl::UpdateParams( afTimeStep );

    // 0x826CEA64..0x826CEBB8 -- the AI throttle ramp. The base pass pushed the raw
    // gas as current; the AI first swaps it back (Update(GetPrevious()) -- the
    // previous frame's ramped throttle returns as current, the raw value becomes
    // the previous) and then ramps that value by 0.15 per frame: toward 1.0 while
    // the car is accelerating (|accel| > KF_MAX, i.e. not braking hard), toward
    // 0.0 while it decelerates (accel < KF_MIN), and holds between the two.
    lrData.mThrottle.Update( lrData.mThrottle.GetPrevious() );
    const f32 lfThrottle = lrData.mThrottle.GetCurrent();
    const bool lbThrottleIsZero = !( lfThrottle > 1.1920929e-7f || lfThrottle < -1.1920929e-7f );
    const f32 lfAcceleration = lrData.mAccelerationMagnitude.GetCurrent();
    if ( lbThrottleIsZero || lfAcceleration >= KF_MIN_THROTTLE_THRESHOLD )
    {
        if ( std::fabs( lfThrottle - 1.0f ) >= 1.52587890625e-5f && lfAcceleration > KF_MAX_THROTTLE_THRESHOLD )
        {
            f32 lfNext = 1.0f;
            if ( std::fabs( 1.0f - lfThrottle ) > KF_ACCCELERATION_COEFFICIENT )
            {
                const f32 lfDelta = 1.0f - lfThrottle;
                const f32 lfSign = ( lfDelta == 0.0f ) ? 0.0f : ( lfDelta >= 0.0f ? 1.0f : -1.0f );   // fsel
                lfNext = lfSign * KF_ACCCELERATION_COEFFICIENT + lfThrottle;
            }
            lrData.mThrottle.Update( lfNext );
        }
    }
    else if ( std::fabs( -lfThrottle ) <= KF_DECELERATION_COEFFICIENT )
    {
        lrData.mThrottle.Update( 0.0f );
    }
    else
    {
        const f32 lfDelta = -lfThrottle;
        const f32 lfSign = ( lfDelta == 0.0f ) ? 0.0f : ( lfDelta >= 0.0f ? 1.0f : -1.0f );           // fsel
        lrData.mThrottle.Update( lfSign * KF_DECELERATION_COEFFICIENT + lfThrottle );
    }

    // 0x826CEBBC..0x826CEBFC -- DMix control input 9: while occluded, hold 0x7FFF
    // for four frames of "not occluded" before dropping it.
    if ( mbIsCurrentlyOccluded )
    {
        if ( mu8NotOccludedCount >= 4 )
        {
            mbIsCurrentlyOccluded = false;
            SetMixerInputValue( 9, 0 );
        }
        else
        {
            SetMixerInputValue( 9, 0x7FFF );
            ++mu8NotOccludedCount;
        }
    }
    else
    {
        SetMixerInputValue( 9, 0 );
    }

    // 0x826CEC00..0x826CEC30: every mDeltaThrottle slot takes cur - prev, cursor 0.
    lrData.mDeltaThrottle.Flush( lrData.mThrottle.GetCurrent() - lrData.mThrottle.GetPrevious() );
    // 0x826CEC34: stfs 0.0 -> PhysicsData + 0x1D0 (mfRotation, the camera-gain latch).
    lrData.mfRotation = 0.0f;
    // 0x826CEC38..0x826CEC74: the swap-then-push on mIsAccelerating.
    lrData.mIsAccelerating.Update( lrData.mIsAccelerating.GetPrevious() );
    lrData.mIsAccelerating.Update( lrData.mThrottle.GetCurrent() > 0.5f );

    UpdateAIPassbys( afTimeStep );

    AIEngineWitness( afTimeStep );
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::UpdateCollisionPassbys(f32)  @ 0x826B4DF8  (primary vtable +8)
//   a bare tail-call into PhysicsControl::UpdateCollisionPassbys @0x826B2628.
// ---------------------------------------------------------------------------
void AIPhysicsControl::UpdateCollisionPassbys( f32 afTimeStep )
{
    PhysicsControl::UpdateCollisionPassbys( afTimeStep );
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::UpdateStartLineReving(f32)  @ 0x826CEC90  (primary vtable +16)
//
// The player machine (PhysicsControl::UpdateStartLineReving @0x826CC0C0) with two
// AI differences:
//   * STARTLINE also flushes mAverageRPM to the authored sample's RPM
//     (0x826CED8C..0x826CEDA4: mean + nine slots = rpm, cursor = 0), so the engine
//     simulation's window carries the performance too;
//   * the hand-back ramp and RESUMING aim at UnityPhysicsRpm(mPhysicsEngine RPM
//     lane) -- `lvx128 v0, this+0x4C0 ; vspltw v0, v0, 1` -- not the raw mfRPM.
// ---------------------------------------------------------------------------
void AIPhysicsControl::UpdateStartLineReving( f32 afTimeStep )
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    CGS_ASSERT( lpModule != 0, "lpLogicModule" );
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    CGS_ASSERT( lpInput != 0, "mpBrnLogicInputBuffer" );
    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface* lpGameModeInterface =
        lpInput->GetGameModeInterface();
    CGS_ASSERT( lpGameModeInterface != 0, "lpGameModeInterface" );
    const s32 liModeState = lpGameModeInterface->miCurrentGameModeState;

    PhysicsData& lrData = mProcessedPhysicsData;

    if ( meIntroRevingState < E_NIS_REVING_STATE_STARTLINE )              // 0x826CEE80
    {
        meIntroRevingState = E_NIS_REVING_STATE_OFF;
        if ( liModeState <= 1 )
        {
            mEngineDataSet = PickStartLineRevDataSet();
            meIntroRevingState = E_NIS_REVING_STATE_STARTLINE;
        }
        return;
    }

    if ( meIntroRevingState == E_NIS_REVING_STATE_STARTLINE )             // 0x826CED40
    {
        meIntroRevingState = E_NIS_REVING_STATE_STARTLINE;
        if ( mEngineDataSet.mpDataPoints == 0
             || mEngineDataSet.mnCurrentPoint >= mEngineDataSet.mnNumPoints )
        {
            mEngineDataSet = PickStartLineRevDataSet();
        }

        const EngineRevEntry lSample = UpdateEngRevDataSet( mEngineDataSet, afTimeStep );
        mAverageRPM.Flush( lSample.mfRpm );

        lrData.mThrottle.Update( lrData.mThrottle.GetPrevious() );
        lrData.mNormalizedRpm.Update( lrData.mNormalizedRpm.GetPrevious() );
        lrData.mThrottle.Update( lSample.mfThrottle );
        lrData.mDeltaThrottle.Flush( lrData.mThrottle.GetCurrent() - lrData.mThrottle.GetPrevious() );
        lrData.mNormalizedRpm.Update( lSample.mfRpm );
        lrData.mGear.Flush( 1 );

        if ( liModeState < 2 )
            return;

        const EngineRevEntry lHandover = UpdateEngRevDataSet( mEngineDataSet, afTimeStep );
        const f32 lfUnityRpm = UnityPhysicsRpm( mPhysicsEngine.GetRPM() );
        f32 lfLength = 500.0f * 0.001f;                                    // flt_82F2CC24 * 0.001
        if ( !( lfLength > 0.0f ) )
            lfLength = 0.01f;
        mEngineStartLineRPM.mfLength       = lfLength;
        mEngineStartLineRPM.mfFinish       = lfUnityRpm * 9000.0f + 1000.0f;
        mEngineStartLineRPM.mbComplete     = false;
        mEngineStartLineRPM.mfStart        = lHandover.mfRpm;
        mEngineStartLineRPM.mfCurrentValue = lHandover.mfRpm;
        mEngineStartLineRPM.mfElapsedTime  = 0.0f;
        mEngineStartLineRPM.meCurveTypes   = static_cast<CgsSound::Utils::Curve::ECurveType>( 2 );
        meIntroRevingState = E_NIS_REVING_STATE_RESUMING;
        return;
    }

    if ( static_cast<s32>( meIntroRevingState ) >= 3 )
        return;

    // RESUMING                                                            // 0x826CECF0
    if ( mEngineStartLineRPM.IsFinished() )
    {
        meIntroRevingState = E_NIS_REVING_STATE_OFF;
        return;
    }
    const f32 lfLiveNormalizedRpm = UnityPhysicsRpm( mPhysicsEngine.GetRPM() ) * 9000.0f + 1000.0f;
    mEngineStartLineRPM.mfFinish = lfLiveNormalizedRpm;
    mEngineStartLineRPM.Update( afTimeStep );
    if ( mEngineStartLineRPM.IsFinished() )
        mEngineStartLineRPM.mfCurrentValue = lfLiveNormalizedRpm;
    lrData.mNormalizedRpm.Update( lrData.mNormalizedRpm.GetPrevious() );
    lrData.mNormalizedRpm.Update( mEngineStartLineRPM.GetValueFloat() );
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::UpdateAIPassbys(f32)  @ 0x826B4A98  (DWARF cpp:268)
//
//   mfPassbyTriggered -= dt; mfNearMissPassby -= dt;
//   relVel = mVelocity3d.cur - listener.mVelocity;    ; this+0xE0 - module+0x2B80
//   relPos = mTransform.cur.pos - listener.pos;       ; this+0x1B0 - module+0x2B20
//   speed = |relVel| ; distance = |relPos|             ; vrsqrtefp + Newton, zero-guarded
//   <KB_DEBUG_AI_PASSYS text "V%0.1f" / "D%0.1f" -- dev render, not carried>
//   if (speed >= KF_PASSBY_MIN_VELOCITY_THRESHOLD && KF_DISTANCE_TO_TRIGGER > distance
//       && mfPassbyTriggered < 0)                     PlayPassBy(speed, false);
//   if (KF_NEARMISS_DISTANCE_TO_TRIGGER > distance) {
//       if (speed >= KF_NEARMISS_MIN_VELOCITY_THRESHOLD && !mbInsideRadius) PlayPassBy(speed, true);
//       mbInsideRadius = 1; }
//   else mbInsideRadius = 0;
// ---------------------------------------------------------------------------
void AIPhysicsControl::UpdateAIPassbys( f32 afTimeStep )
{
    mfPassbyTriggered -= afTimeStep;
    mfNearMissPassby  -= afTimeStep;

    const CgsSound::Logic::MicrophoneSystem::Microphone* lpListener = AIListenerMicrophone( GetLogicModule() );
    const Vector3& lrListenerVelocity = lpListener->GetVelocity();
    const Vector3& lrListenerPosition = lpListener->GetMicrophoneMatrix().Pos();
    const Vector3& lrVelocity = mProcessedPhysicsData.mVelocity3d.GetCurrent();
    const Vector3& lrPosition = mProcessedPhysicsData.mTransform.GetCurrent().Pos();

    const f32 lfRelativeSpeed = Length3( lrVelocity.x - lrListenerVelocity.x,
                                         lrVelocity.y - lrListenerVelocity.y,
                                         lrVelocity.z - lrListenerVelocity.z );
    const f32 lfDistance = Length3( lrPosition.x - lrListenerPosition.x,
                                    lrPosition.y - lrListenerPosition.y,
                                    lrPosition.z - lrListenerPosition.z );

    if ( lfRelativeSpeed >= KF_PASSBY_MIN_VELOCITY_THRESHOLD )
    {
        if ( KF_DISTANCE_TO_TRIGGER > lfDistance && mfPassbyTriggered < 0.0f )
            PlayPassBy( lfRelativeSpeed, false );
    }

    if ( KF_NEARMISS_DISTANCE_TO_TRIGGER > lfDistance )
    {
        if ( lfRelativeSpeed >= KF_NEARMISS_MIN_VELOCITY_THRESHOLD && !mbInsideRadius )
            PlayPassBy( lfRelativeSpeed, true );
        mbInsideRadius = true;
    }
    else
    {
        mbInsideRadius = false;
    }
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::PlayPassBy(f32, bool)  @ 0x8269A4C0  (DWARF cpp:362)
//
//   assert(mpLogicModule, "lpLogicModule");                                  ; cpp:366
//   passbys = module->mEnvironment.mapStateManagers[4];                     ; module+0x2964
//   Passby p = { staticPos = 0, mp3dControl = mp3dCarControl (this+0x220),
//                relVel = speed, type = nearMiss ? TrafficSmall(3) : TrafficLarge(5),
//                volume = KF_VOLUME_MODIFIER (2.5), suppressBoostBys = 1 };
//   if (passbys->PostPassby(p))                                             ; (count+1) < 8
//       nearMiss ? mfNearMissPassby = 2.0 : mfPassbyTriggered = 2.0;        ; KF_TIME_BETWEEN_AI_PASSBYS
// ---------------------------------------------------------------------------
void AIPhysicsControl::PlayPassBy( f32 afRelativeVelocityMagnitude, bool abNearMiss )
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    CGS_ASSERT( lpModule != 0, "lpLogicModule" );
    BrnSound::Logic::Passby::PassbyStateManager* lpPassbyStateManager =
        static_cast<BrnSound::Logic::Passby::PassbyStateManager*>(
            lpModule->GetEnvironment().GetStateManager( 4 ) );

    const BrnSound::Logic::Passby::PassbyStateManager::Passby lPassby(
        static_cast<const CgsSound::Logic::Cgs3dEffectControl*>( mp3dCarControl ),
        afRelativeVelocityMagnitude,
        abNearMiss ? AttribSys::Enums::ePassbyTypes::TrafficSmall
                   : AttribSys::Enums::ePassbyTypes::TrafficLarge,
        true,
        KF_VOLUME_MODIFIER );
    const bool lbPosted = lpPassbyStateManager != 0 && lpPassbyStateManager->PostPassby( lPassby );
    if ( lbPosted )
    {
        if ( abNearMiss )
            mfNearMissPassby = KF_TIME_BETWEEN_AI_PASSBYS;
        else
            mfPassbyTriggered = KF_TIME_BETWEEN_AI_PASSBYS;
    }

    // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG (capped at 64 lines).
    if ( AISoundDiagLive() )
    {
        static u32 suCount = 0;
        if ( suCount++ < 64u )
        {
            *CgsDev::Log::gpDebugPrint
                << "[ai-sound-passby] car=" << static_cast<s32>( GetAttachInfo().muVehicleIndex )
                << ( abNearMiss ? " near-miss" : " passby" )
                << " relSpeed=" << afRelativeVelocityMagnitude
                << " accepted=" << static_cast<s32>( lbPosted ? 1 : 0 )
                << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// AIPhysicsControl::Notify(const MessageHeader*)  @ 0x8269A610  (sub-object vtable +0x24)
//
//   id = msg->mi16EventId (+8);
//   18 (E_SOUNDMESSAGE_RACE_CAR_IS_ACTIVE): key = *(u64*)(msg + 0x18)     ; ld r4, 0x18(r4)
//       burnoutcarasset car(key, 0); physicsvehiclehandling h(car.PhysicsVehicleHandling
//       refspec (+0x158) collection); physicsvehicleengineattribs e(h.PhysicsVehicleEngine
//       Attribs refspec (+0x30) collection); EngineAttribs a; a.Construct();
//       a.InitializeFromAttribs(&e); mPhysicsEngine.Prepare(&a); mPhysicsEngine.Reset(0);
//   39 (E_SOUNDMESSAGE_ENABLE_OCCLUSION): mbEnableOcclusion = *(u8*)(msg + 0x10);
// ---------------------------------------------------------------------------
void AIPhysicsControl::Notify( const CgsSound::Io::MessageHeader* apkMessage )
{
    const s32 liEventId = apkMessage->GetEventId();
    if ( liEventId == BrnSound::E_SOUNDMESSAGE_RACE_CAR_IS_ACTIVE )
    {
        const CgsSound::Io::Message<BrnSound::RaceCarIsNowActive>* lpMessage =
            static_cast<const CgsSound::Io::Message<BrnSound::RaceCarIsNowActive>*>( apkMessage );
        u64 luCarAssetKey = 0;
        std::memcpy( &luCarAssetKey, &lpMessage->mData.maData[8], sizeof( luCarAssetKey ) );

        Attrib::Gen::burnoutcarasset lCarAsset( luCarAssetKey, 0 );
        Attrib::RefSpec* lpHandlingSpec = lCarAsset.GetPhysicsVehicleHandlingRefSpec();
        Attrib::Gen::physicsvehiclehandling lHandling(
            lpHandlingSpec ? const_cast<Attrib::Collection*>( lpHandlingSpec->GetCollection() ) : 0, 0 );
        Attrib::Gen::physicsvehicleengineattribs lEngineAttribs(
            const_cast<Attrib::Collection*>( lHandling.PhysicsVehicleEngineAttribs().GetCollection() ), 0 );

        BrnPhysics::Vehicle::VehicleAttribs::EngineAttribs lAttribs;
        lAttribs.Construct();
        lAttribs.InitializeFromAttribs( lEngineAttribs.GetLayoutPointer() );
        mPhysicsEngine.Prepare( &lAttribs );
        mPhysicsEngine.Reset( Splat( 0.0f ) );

        // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG
        if ( AISoundDiagLive() )
        {
            *CgsDev::Log::gpDebugPrint
                << "[ai-sound-engine] car=" << static_cast<s32>( GetAttachInfo().muVehicleIndex )
                << " engine prepared from car asset key=" << CgsDev::E_PRINTMODE_HEXONCE << luCarAssetKey
                << "\n";
        }
    }
    else if ( liEventId == BrnSound::E_SOUNDMESSAGE_ENABLE_OCCLUSION )
    {
        const CgsSound::Io::Message<EnableOcclusionData>* lpMessage =
            static_cast<const CgsSound::Io::Message<EnableOcclusionData>*>( apkMessage );
        mbEnableOcclusion = lpMessage->mData.mbEnable;
    }
}

// ---------------------------------------------------------------------------
// [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG.
// One line per second of game time per attached AI car: the simulated engine feed.
// ---------------------------------------------------------------------------
void AIPhysicsControl::AIEngineWitness( f32 afTimeStep )
{
    if ( !AISoundDiagLive() )
        return;
    mfDiagAIWitnessTimer += afTimeStep;
    if ( mfDiagAIWitnessTimer < 1.0f )
        return;
    mfDiagAIWitnessTimer = 0.0f;

    const CgsSound::Logic::MicrophoneSystem::Microphone* lpListener = AIListenerMicrophone( GetLogicModule() );
    const Vector3& lrListenerPosition = lpListener->GetMicrophoneMatrix().Pos();
    const Vector3& lrPosition = mProcessedPhysicsData.mTransform.GetCurrent().Pos();
    const f32 lfDistance = Length3( lrPosition.x - lrListenerPosition.x,
                                    lrPosition.y - lrListenerPosition.y,
                                    lrPosition.z - lrListenerPosition.z );
    *CgsDev::Log::gpDebugPrint
        << "[ai-sound-engine] car=" << static_cast<s32>( GetAttachInfo().muVehicleIndex )
        << " rpm=" << mAverageRPM.GetAverage()
        << " gear=" << static_cast<s32>( mPhysicsEngine.GetCurrentGear() )
        << " throttle=" << mProcessedPhysicsData.mThrottle.GetCurrent()
        << " unityRpm=" << mProcessedPhysicsData.mUnityRpm.GetCurrent()
        << " wheelRadS=" << mAverageWheelVel.GetAverage()
        << " speedMps=" << mAverageSpeedMps.GetAverage()
        << " distance=" << lfDistance
        << " occluded=" << static_cast<s32>( mbIsCurrentlyOccluded ? 1 : 0 )
        << "\n";
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
