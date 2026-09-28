#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h"          // Traffic3DControl
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"    // TrafficClassToSize
#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"               // PassbyStateManager::Passby / PostPassby
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficControl -- out-of-line bodies.
//
// Not carried: the KI_SPEW_TRAFFIC_PASS_CULLS developer TTY lines ("[Passby]
// VelocityThreshold [PASS]", "Distance [PASS]", "Distance [FAIL!]") in UpdateParams,
// gated on a switch that is zero in the image and only registered with the
// debug-variable registry.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

namespace
{
// Splat constants the CRT fills: 0.25 s to the passby apex, and a relative speed
// window of 40 mph (0.44704 * 40.0) to 500.
const f32 KF_TRAFFIC_PASSBY_TIME_TO_APEX = 0.25f;
const f32 KF_TRAFFIC_PASSBY_MIN_VELOCITY_THRESHOLD = 0.44704f * 40.0f;
const f32 KF_TRAFFIC_PASSBY_MAX_VELOCITY_THRESHOLD = 500.0f;
} // namespace

// The scalar deleting destructor settles the BrnEffectControl members (detach state,
// resources-ready byte, attach state) through the base destructor chain.
TrafficControl::~TrafficControl()
{
}

// MemBase::operator new(0x44, "TrafficControl", flavour) with the BrnEffectControl base
// construction inlined; returns the EffectControl view (+0x04).
CgsSound::Logic::EffectControl* TrafficControl::CreateObject( u32 /*luType*/ )
{
    return new TrafficControl();
}

// Descriptor {0x30000, "TrafficControl", BrnEffectControl::sTypeInfo, &CreateObject}.
// FLAG: BrnEffectControl's own descriptor is not homed, so the chain starts at
// EffectControl's (the CollisionControl precedent).
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* TrafficControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x30000, "TrafficControl",
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &TrafficControl::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpTrafficControlReg =
    CgsSound::Logic::EffectControl::AddToClassTypeInfoArray( TrafficControl::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* TrafficControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* TrafficControl::GetTypeName() const
{
    return "TrafficControl";
}

// One controller: control 1, the Traffic3DControl (identical code to
// CollisionControl::GetController).
s32 TrafficControl::GetController( s32 liIndex )
{
    return liIndex == 0 ? 1 : -1;
}

void TrafficControl::AttachController( CgsSound::Logic::EffectBase* lpController )
{
    if ( lpController->GetEffectID() == 1 )
        mpTraffic3dControl = static_cast<Traffic3DControl*>( lpController );
}

// The EffectBase attach (detach state cleared, attach count bumped), then the state's
// traffic entity, and the 3D emitter pointed at the entity's position.
bool TrafficControl::Attach()
{
    CgsSound::Logic::EffectBase::Attach();

    TrafficState* lpState = static_cast<TrafficState*>( GetStateBase() );
    CGS_ASSERT( lpState != 0, "lpState" );
    mbPassbyTriggered = false;
    mpTrafficEntity = lpState->GetTrafficEntity();
    mpTraffic3dControl->AttachEmitterPosition( &mpTrafficEntity->mLocalTransform.Pos() );
    return true;
}

// Mixer input 0 carries the crash flag. Until this attachment has posted its passby:
// the car's velocity (its At axis times its speed) relative to the player microphone
// must lie in [min, max); when that speed times the time to apex exceeds the distance
// to the microphone, one passby of the car's size class goes to the PassbyStateManager.
void TrafficControl::UpdateParams( f32 /*lfTimeStep*/ )
{
    SetMixerInputValue( 0, mpTrafficEntity->mbIsCrashed );
    if ( mbPassbyTriggered )
        return;

    CgsSound::Logic::MicrophoneSystem::Microphone* lpMicrophone =
        GetLogicModule()->GetEnvironment().GetMicrophoneSystem().GetMicrophone(
            CgsSound::Logic::MicrophoneSystem::E_MIC_PLAYER,
            CgsSound::Logic::MicrophoneSystem::E_PLAYER_1 );

    const f32 lfSpeed = mpTrafficEntity->mfSpeed;
    const Vector3 lVelocity = mpTrafficEntity->mLocalTransform.At() * lfSpeed;
    const f32 lfRelativeVelocity = rw::math::vpu::Magnitude( lVelocity - lpMicrophone->GetVelocity() );

    if ( !( lfRelativeVelocity >= KF_TRAFFIC_PASSBY_MIN_VELOCITY_THRESHOLD ) )
        return;
    if ( !( KF_TRAFFIC_PASSBY_MAX_VELOCITY_THRESHOLD > lfRelativeVelocity ) )
        return;

    const f32 lfDistance = rw::math::vpu::Magnitude(
        mpTrafficEntity->mLocalTransform.Pos() - lpMicrophone->GetMicrophoneMatrix().Pos() );
    if ( !( lfRelativeVelocity * KF_TRAFFIC_PASSBY_TIME_TO_APEX > lfDistance ) )
        return;

    CgsSound::Logic::Module* lpLogicModule = GetLogicModule();
    CGS_ASSERT( lpLogicModule != 0, "lpLogicModule" );
    Passby::PassbyStateManager* lpPassbyStateManager =
        static_cast<Passby::PassbyStateManager*>( lpLogicModule->GetEnvironment().GetStateManager( 4 ) );

    const ETrafficSize leSize = TrafficStateManager::TrafficClassToSize( mpTrafficEntity->muVehicleClass );
    const Passby::PassbyStateManager::Passby lPassby(
        mpTraffic3dControl,
        lfRelativeVelocity,
        static_cast<Passby::PassbyStateManager::Passby::EePassbyTypes>(
            AttribSys::Enums::ePassbyTypes::TrafficSmall + leSize ),
        false,
        1.0f );
    mbPassbyTriggered = lpPassbyStateManager->PostPassby( lPassby );
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
