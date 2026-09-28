#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficSkid.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficSoundDiag.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // PhysicalTrafficState
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "rw/math/vpu/vector3_operation.h"

#include <cmath>

// =============================================================================
// BrnSound::Logic::Traffic::TrafficSkid -- out-of-line bodies.
//
// The AEMS names are the console's CRT-hashed tables: ParameterIndexes::
// AEMS_Skids_Traffic (AEMS_pitch, AEMS_volume, AEMS_azimuth, AEMS_drift, AEMS_size) and
// SendIndexes::AEMS_Skids_Traffic (Send01, ReverbSend) -- the same tables AISkidEffect
// reads.
//
// FLAG (replay serialiser not in this tree): while the sound serialiser plays back,
// the console's ProcessUpdate takes mfDriftFactor from the recorded traffic entity, and
// while it records it writes mfDriftFactor there. This tree's SoundLogicModule holds
// no SoundSerialiser, so the live drift factor is always used.
//
// Not carried: the KB_SKID_PRINT_INFO developer TTY lines in CalculateDriftParameter,
// gated on a switch that is zero in the image.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

namespace
{
const u32 KAU_TRAFFIC_SKID_PARAMETERS[5] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_pitch" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_volume" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_azimuth" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_drift" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_size" ) ),
};

const u32 KAU_TRAFFIC_SKID_SENDS[2] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) ),
};

// The shared CRT-hashed send names every traffic voice's CreateParams reads.
const u32 KU_SEND01      = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) );
const u32 KU_REVERB_SEND = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) );

// The drift model: the lateral and longitudinal wheel speeds each count 1.0 at these
// maxima, the drift parameter tops out at 1023, and the skid fades after 4 s physical.
const f32 KF_SKID_MAX_EXPECTED_LATERAL = 12.0f;
const f32 KF_SKID_MAX_EXPECTED_LONG    = 8.0f;
const f32 KF_SKID_MAX_DRIFT            = 1023.0f;
const f32 KF_SKID_MAX_TIME             = 4.0f;

// The pitch scale for a car (class 0) and for anything bigger.
const f32 KF_SKID_SMALL_PITCH = 1.12060546875f;
const f32 KF_SKID_LARGE_PITCH = 0.8408203125f;

u32 guSkidVoiceWitnesses = 0;
u32 guSkidReleaseWitnesses = 0;
} // namespace

// The console constructor inlines the BrnEffectObject base zero-inits and both leaf
// vtables, constructs mSkidVoice and installs the functor's vtable.
TrafficSkid::TrafficSkid()
    : BrnEffectObject()
    , mSkidVoice()
    , mSkidFunctionPointer()
{
}

// The scalar deleting destructor is fully inlined on the console: the voice wrapper
// destructor, then the BrnEffectObject settle.
TrafficSkid::~TrafficSkid()
{
}

// MemBase::operator new(0xB0, "TrafficSkid", flavour) + the constructor; returns the
// EffectObject view (+0x04).
CgsSound::Logic::EffectObject* TrafficSkid::CreateObject( u32 /*luType*/ )
{
    return new TrafficSkid();
}

// Descriptor {0x30020, "TrafficSkid", BrnEffectObject::sTypeInfo, &CreateObject}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TrafficSkid::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x30020, "TrafficSkid",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &TrafficSkid::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpTrafficSkidReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray( TrafficSkid::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TrafficSkid::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* TrafficSkid::GetTypeName() const
{
    return "TrafficSkid";
}

// One controller: control 0, the TrafficControl (identical code to
// MusicEffect::GetController).
s32 TrafficSkid::GetController( s32 liIndex )
{
    return liIndex == 0 ? 0 : -1;
}

void TrafficSkid::AttachController( CgsSound::Logic::EffectBase* lpController )
{
    if ( lpController->GetEffectID() == 0 )
        mpTrafficControl = static_cast<TrafficControl*>( lpController );
}

// The EffectBase attach, then the skid voice created (not played) on the PLAYER state
// manager's Skids.abi.
bool TrafficSkid::Attach()
{
    mSkidFunctionPointer.Construct( this, &TrafficSkid::OnPostInitVoice );
    CgsSound::Logic::EffectBase::Attach();
    mfDriftFactor = 0.0f;
    mfTimeAsPhysical = 0.0f;

    CgsSound::Logic::StateManager* lpPlayerStateMan = GetLogicModule()->GetEnvironment().GetStateManager( 1 );
    CGS_ASSERT( lpPlayerStateMan != 0, "lpPlayerStateMan" );
    const CgsSound::Playback::Name lSkidsContentName( "Skids.abi" );
    CgsSound::Logic::Content* lpSkidsContent = lpPlayerStateMan->GetContent( lSkidsContentName );
    CGS_ASSERT( lpSkidsContent != 0, "lpSkidsContent" );

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule        = GetLogicModule();
    lParams.mpOnPostInit         = &mSkidFunctionPointer;
    lParams.mFactoryName         = static_cast<u32>( CgsSound::Playback::AemsFactorySkName().GetValue() );
    lParams.mVoiceSpecName       = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_Skids_Traffic" ) );
    lParams.mpContent            = lpSkidsContent;
    lParams.mContentSpecName     = 0;
    lParams.mSlotName            = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_Slot" ) );
    lParams.mSendName            = KU_SEND01;
    lParams.mSubMixVoiceID       = 1;
    lParams.mReverbSendName      = KU_REVERB_SEND;
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex          = 0;
    mSkidVoice.Create( lParams );
    return true;
}

// Only a physical car skids: the time as a physical body accumulates, the voice is
// (re)started, and the drift parameter is read from the car's PhysicalTrafficState --
// fading to zero over the two seconds after KF_SKID_MAX_TIME.
void TrafficSkid::UpdateParams( f32 lfTimeStep )
{
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* lpTrafficSoundEntity = mpTrafficControl->GetTrafficEntity();
    CGS_ASSERT( lpTrafficSoundEntity != 0, "lpTrafficSoundEntity" );

    if ( !lpTrafficSoundEntity->mbIsPhysical )
    {
        mfTimeAsPhysical = 0.0f;
        mfDriftFactor = 0.0f;
        return;
    }

    mfTimeAsPhysical += lfTimeStep;
    if ( mSkidVoice.GetUpdateStage() != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING )
    {
        mSkidVoice.Play( 0 );

        // [FLAG PC witness] BRN_TRAFFICSND_DIAG
        if ( TrafficSoundDiagTake( guSkidVoiceWitnesses, 16 ) )
        {
            *CgsDev::Log::gpDebugPrint
                << "[trafficsnd] voice started type=skid entity="
                << static_cast<s32>( lpTrafficSoundEntity->mu16EntityIndex )
                << " physical=" << mfTimeAsPhysical << "\n";
        }
    }

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    const BrnSound::Module::Io::LogicInputBuffer* lpInputBuffer = lpModule->GetBrnInputStructure();
    CGS_ASSERT( lpInputBuffer != 0, "lpInputBuffer" );
    const BrnSound::Module::Io::RootInputBuffer::PhysicalTrafficStateQueue* lpPhysicalTrafficStates =
        lpInputBuffer->GetPhysicalTrafficStates();

    const s32 liIndex = FindPhysicalTrafficState( lpPhysicalTrafficStates, lpTrafficSoundEntity->mEntityId );
    if ( liIndex == -1 )
        return;

    const f32 lfDrift = CalculateDriftParameter( lpPhysicalTrafficStates->GetEvent( liIndex ) );
    mfDriftFactor = lfDrift;
    if ( mfTimeAsPhysical > KF_SKID_MAX_TIME )
    {
        f32 lfFade = ( KF_SKID_MAX_TIME + 2.0f ) - mfTimeAsPhysical;
        lfFade = ( lfFade >= 0.0f ) ? lfFade : 0.0f;
        mfDriftFactor = lfFade * 0.5f * lfDrift;
    }
}

// While the voice is alive: pitch (the mixer pitch scaled by the car's size), volume,
// azimuth and the drift factor; Send01 at 4.0 and the reverb send from mixer output 4.
void TrafficSkid::ProcessUpdate()
{
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* lpTrafficSoundEntity = mpTrafficControl->GetTrafficEntity();
    CGS_ASSERT( lpTrafficSoundEntity != 0, "lpTrafficSoundEntity" );

    mSkidVoice.Update();
    if ( !mSkidVoice.IsAlive() )
        return;

    const f32 lfVolume      = GetMixerOutputValue( 0, Nicotine::DMixIO::DMX_VOL );
    const f32 lfPitch       = GetMixerOutputValue( 1, Nicotine::DMixIO::DMX_PITCH );
    const f32 lfAzimuth     = GetMixerOutputValue( 2, Nicotine::DMixIO::DMX_AZIM );
    const f32 lfReverbSend  = GetRWACMixerOutputValue( 4, Nicotine::DMixIO::DMX_VOL );
    const f32 lfSizePitch   = ( lpTrafficSoundEntity->muVehicleClass != 0 ) ? KF_SKID_LARGE_PITCH : KF_SKID_SMALL_PITCH;

    mSkidVoice.SetParameter( 0, lfSizePitch * lfPitch, &KAU_TRAFFIC_SKID_PARAMETERS[0] );
    mSkidVoice.SetParameter( 1, lfVolume,              &KAU_TRAFFIC_SKID_PARAMETERS[1] );
    mSkidVoice.SetParameter( 2, lfAzimuth,             &KAU_TRAFFIC_SKID_PARAMETERS[2] );
    mSkidVoice.SetParameter( 3, mfDriftFactor,         &KAU_TRAFFIC_SKID_PARAMETERS[3] );
    mSkidVoice.SetGain( 0, 4.0f,         &KAU_TRAFFIC_SKID_SENDS[0] );
    mSkidVoice.SetGain( 1, lfReverbSend, &KAU_TRAFFIC_SKID_SENDS[1] );
}

// A staged detach over meDetachState: release the voice, then the BrnEffectObject
// detach, then finished.
bool TrafficSkid::Detach()
{
    switch ( meDetachState )
    {
    case E_DETACH_STATE_NONE:
    case E_DETACH_STATE_BEGIN:
        meDetachState = E_DETACH_STATE_BEGIN;

        // [FLAG PC witness] BRN_TRAFFICSND_DIAG
        if ( TrafficSoundDiagTake( guSkidReleaseWitnesses, 64 ) )
        {
            *CgsDev::Log::gpDebugPrint
                << "[trafficsnd] voice released type=skid stage="
                << static_cast<s32>( mSkidVoice.GetUpdateStage() ) << "\n";
        }

        mSkidVoice.Release();
        // fall through
    case E_DETACH_STATE_UPDATING:
        meDetachState = E_DETACH_STATE_UPDATING;
        if ( !BrnSound::Logic::BrnEffectObject::Detach() )
            return false;
        // fall through
    case E_DETACH_STATE_FINISHED:
        meDetachState = E_DETACH_STATE_FINISHED;
        return true;
    default:
        return false;
    }
}

// The index of the LAST queued physical-traffic state whose entity id matches, or -1.
s32 TrafficSkid::FindPhysicalTrafficState(
    const BrnSound::Module::Io::RootInputBuffer::PhysicalTrafficStateQueue* lpPhysicalTrafficStates,
    EntityId lEntityId )
{
    CGS_ASSERT( lpPhysicalTrafficStates != 0, "lpPhysicalTrafficStates" );
    s32 liFound = -1;
    for ( s32 liIndex = 0; liIndex < lpPhysicalTrafficStates->GetLength(); ++liIndex )
    {
        if ( lEntityId.muValue == lpPhysicalTrafficStates->GetEvent( liIndex ).mEntityID.muValue )
            liFound = liIndex;
    }
    return liFound;
}

// Empty on the console (the functor's target is the shared empty body).
void TrafficSkid::OnPostInitVoice( CgsSound::Logic::VoiceWrapper& /*lrVoice*/ )
{
}

// For a moving car: over the wheels on the ground, the lateral wheel speed (its
// velocity along the car's right axis) and the longitudinal slip (car speed minus the
// wheel's rolling speed, never negative), each normalised; their mean scaled to
// KF_SKID_MAX_DRIFT and clamped to [0, KF_SKID_MAX_DRIFT].
f32 TrafficSkid::CalculateDriftParameter( const BrnPhysics::Vehicle::PhysicalTrafficState& lrPhysicalTrafficState )
{
    f32 lfDrift = 0.0f;
    f32 lfWheelsOnGround = 0.0f;

    if ( lrPhysicalTrafficState.mfSpeed > 0.0f )
    {
        for ( u32 luWheel = 0; luWheel < 4; ++luWheel )
        {
            const BrnPhysics::Vehicle::WheelLite& lrWheel = lrPhysicalTrafficState.maWheels[luWheel];
            if ( !lrWheel.mRoadContact.mbIsOnGround )
                continue;

            f32 lfLongitudinal = ( lrPhysicalTrafficState.mfSpeed
                                   - std::fabs( lrWheel.mfRadiansPerSecond ) * lrWheel.mfRadius )
                                 / KF_SKID_MAX_EXPECTED_LONG;
            const f32 lfLateral = rw::math::vpu::Dot( lrWheel.mVelocity, lrPhysicalTrafficState.mTransform.Right() )
                                  / KF_SKID_MAX_EXPECTED_LATERAL;
            lfLongitudinal = ( lfLongitudinal >= 0.0f ) ? lfLongitudinal : 0.0f;

            lfWheelsOnGround += 1.0f;
            lfDrift += std::fabs( lfLateral ) + lfLongitudinal;
        }

        if ( lfWheelsOnGround > 0.0f )
        {
            f32 lfScaled = ( lfDrift / lfWheelsOnGround ) * KF_SKID_MAX_DRIFT;
            lfScaled = ( -lfScaled >= 0.0f ) ? 0.0f : lfScaled;
            lfDrift = ( ( KF_SKID_MAX_DRIFT - lfScaled ) >= 0.0f ) ? lfScaled : KF_SKID_MAX_DRIFT;
        }
    }
    return lfDrift;
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
