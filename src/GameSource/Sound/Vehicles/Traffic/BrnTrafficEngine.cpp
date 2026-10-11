#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficSoundDiag.h"
#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameSource/Sound/BrnDebugComponent.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

// =============================================================================
// BrnSound::Logic::Traffic::Traffic3DControl / TrafficEngine -- out-of-line bodies.
//
// The AEMS names are the console's CRT-hashed tables: ParameterIndexes::
// AEMS_TrafficEngineClass (AEMS_azimuth, AEMS_velocity, AEMS_surface_Type,
// AEMS_car_Type, AEMS_pitch, AEMS_volume_Engine, AEMS_volume_RoadNoise) and
// SendIndexes::AEMS_TrafficEngineClass (Send01, ReverbSend).
//
// ProcessUpdate also posts the original DrawSphere message and writes the engine
// mixer values to the Sound debug log when their developer switches are enabled.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

u32 KI_SPEW_TRAFFIC = 0; // ARTIST dword_82FFB840
u32 KI_DRAW_ATTACHED = 0; // ARTIST dword_82FFB844

namespace
{
const u32 KAU_TRAFFIC_ENGINE_PARAMETERS[7] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_azimuth" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_velocity" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_surface_Type" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_car_Type" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_pitch" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_volume_Engine" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_volume_RoadNoise" ) ),
};

const u32 KAU_TRAFFIC_ENGINE_SENDS[2] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) ),
};

// The shared CRT-hashed "ReverbSend" every traffic voice's CreateParams reads.
const u32 KU_REVERB_SEND = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) );

// One mph in metres per second.
const f32 KF_MPH_TO_METRES_PER_SECOND = 0.44704f;

// The Q15 unit scale applied to the reverb-send mixer output.
const f32 KF_Q15_TO_UNIT = 3.0518509e-05f;

u32 guEngineVoiceWitnesses = 0;
u32 guEngineReleaseWitnesses = 0;
u32 guEnginePlayingWitnesses = 0;
u32 guEngineFinishedWitnesses = 0;
} // namespace

// ---------------------------------------------------------------------------
// Traffic3DControl
// ---------------------------------------------------------------------------

// The vector deleting destructor tears mEngineDataAtrib and the base members down
// through the Brn3DEffectControl destructor chain.
Traffic3DControl::~Traffic3DControl()
{
}

// MemBase::operator new(0xD0, "Traffic3DControl", flavour), Brn3DEffectControl(), leaf vtable.
CgsSound::Logic::EffectControl* Traffic3DControl::CreateObject( u32 /*luType*/ )
{
    return new Traffic3DControl();
}

// Descriptor {0x30010, "Traffic3DControl", Brn3DEffectControl::sTypeInfo, &CreateObject}.
// FLAG: Brn3DEffectControl's own descriptor is not homed, so the chain starts at
// EffectControl's (the Passby3DControl precedent); 0x30010 has no rival to tie-break.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Traffic3DControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x30010, "Traffic3DControl",
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &Traffic3DControl::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpTraffic3DControlReg =
    CgsSound::Logic::EffectControl::AddToClassTypeInfoArray( Traffic3DControl::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Traffic3DControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* Traffic3DControl::GetTypeName() const
{
    return "Traffic3DControl";
}

// ---------------------------------------------------------------------------
// TrafficEngine
// ---------------------------------------------------------------------------

// The console constructor inlines the BrnEffectObject base zero-inits and both leaf
// vtables, clears the three members and constructs the voice wrapper.
TrafficEngine::TrafficEngine()
    : BrnEffectObject()
    , mu16AemsPatchTrafficIndex( 0 )
    , mpTrafficControl( nullptr )
    , mpTraffic3DControl( nullptr )
    , mTrafficEngineVoice()
{
}

TrafficEngine::~TrafficEngine()
{
}

// MemBase::operator new(0x94, "TrafficEngine", flavour) + the constructor; returns the
// EffectObject view (+0x04).
CgsSound::Logic::EffectObject* TrafficEngine::CreateObject( u32 /*luType*/ )
{
    return new TrafficEngine();
}

// Descriptor {0x30000, "TrafficEngine", BrnEffectObject::sTypeInfo, &CreateObject}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TrafficEngine::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x30000, "TrafficEngine",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &TrafficEngine::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpTrafficEngineReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray( TrafficEngine::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TrafficEngine::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* TrafficEngine::GetTypeName() const
{
    return "TrafficEngine";
}

// Two controllers: slot 0 is control 1 (Traffic3DControl), slot 1 is control 0
// (TrafficControl) -- identical code to RoadnoiseEffect::GetController.
s32 TrafficEngine::GetController( s32 liIndex )
{
    if ( liIndex == 0 )
        return 1;
    if ( liIndex == 1 )
        return 0;
    return -1;
}

void TrafficEngine::AttachController( CgsSound::Logic::EffectBase* lpController )
{
    const s32 liEffectId = lpController->GetEffectID();
    if ( liEffectId < 1 )
        mpTrafficControl = static_cast<TrafficControl*>( lpController );
    else if ( liEffectId == 1 )
        mpTraffic3DControl = static_cast<Traffic3DControl*>( lpController );
}

// The EffectBase attach, then -- only for a car whose engine runs -- the engine voice
// on the manager's engine bank, played at once, and the car's size class as the AEMS
// patch index.
bool TrafficEngine::Attach()
{
    CgsSound::Logic::EffectBase::Attach();

    TrafficState* lpState = static_cast<TrafficState*>( GetStateBase() );
    static const u32 su32VoiceSpecName =
        static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_TrafficEngineClass" ) );
    static const u32 su32SlotName =
        static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_Slot" ) );
    static const u32 su32SendName =
        static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) );

    if ( lpState->GetTrafficEntity()->mbIsEngineOn )
    {
        CgsSound::Logic::VoiceWrapper::CreateParams lParams;
        lParams.mpLogicModule        = GetLogicModule();
        lParams.mpOnPostInit         = 0;
        lParams.mFactoryName         = static_cast<u32>( CgsSound::Playback::AemsFactorySkName().GetValue() );
        lParams.mVoiceSpecName       = su32VoiceSpecName;
        lParams.mpContent            = &lpState->GetTrafficStateManager()->GetEngineAemsBank();
        lParams.mContentSpecName     = 0;
        lParams.mSlotName            = su32SlotName;
        lParams.mSendName            = su32SendName;
        lParams.mSubMixVoiceID       = 1;
        lParams.mReverbSendName      = KU_REVERB_SEND;
        lParams.mReverbSubMixVoiceID = 2;
        lParams.miSendIndex          = 0;
        mTrafficEngineVoice.Create( lParams );
        mTrafficEngineVoice.Play( 0 );
        mu16AemsPatchTrafficIndex = static_cast<u16>(
            TrafficStateManager::TrafficClassToSize( mpTrafficControl->GetTrafficEntity()->muVehicleClass ) );

        // [FLAG PC witness] BRN_TRAFFICSND_DIAG
        if ( TrafficSoundDiagTake( guEngineVoiceWitnesses, 16 ) )
        {
            *CgsDev::Log::gpDebugPrint
                << "[trafficsnd] voice started type=engine entity="
                << static_cast<s32>( lpState->GetTrafficEntity()->mu16EntityIndex )
                << " size=" << static_cast<s32>( mu16AemsPatchTrafficIndex ) << "\n";
        }
    }
    return true;
}

// Empty on the console (the slot holds the shared empty body).
void TrafficEngine::UpdateParams( f32 /*lfTimeStep*/ )
{
}

// Every frame: the voice wrapper advances, then the seven AEMS parameters (mixer
// azimuth / pitch / engine and road-noise volumes, the car's speed in metres per
// second, surface type 0, the size class) and the two sends (Send01 at 1.0, the
// reverb send from mixer output 5).
void TrafficEngine::ProcessUpdate()
{
    const f32 lfCarType          = static_cast<f32>( mu16AemsPatchTrafficIndex );
    const f32 lfVelocity         = mpTrafficControl->GetTrafficEntity()->mfSpeed * KF_MPH_TO_METRES_PER_SECOND;
    const f32 lfSurfaceType      = 0.0f;
    const f32 lfAzimuth          = GetMixerOutputValue( 3, Nicotine::DMixIO::DMX_AZIM );
    const f32 lfPitch            = GetMixerOutputValue( 2, Nicotine::DMixIO::DMX_PITCH );
    const f32 lfEngineVolume     = GetMixerOutputValue( 0, Nicotine::DMixIO::DMX_VOL );
    const f32 lfRoadNoiseVolume  = GetMixerOutputValue( 1, Nicotine::DMixIO::DMX_VOL );
    const f32 lfReverbSend       = GetMixerOutputValue( 5, Nicotine::DMixIO::DMX_VOL ) * KF_Q15_TO_UNIT;

    const CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE leStageBefore = mTrafficEngineVoice.GetUpdateStage();
    mTrafficEngineVoice.Update();

    // [FLAG PC witness] BRN_TRAFFICSND_DIAG: the engine voice reached PLAYING, or left it
    // on its own while the car is still attached (a looping engine never does).
    const CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE leStageAfter = mTrafficEngineVoice.GetUpdateStage();
    if ( leStageBefore != leStageAfter && leStageAfter == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING
         && TrafficSoundDiagTake( guEnginePlayingWitnesses, 16 ) )
    {
        *CgsDev::Log::gpDebugPrint
            << "[trafficsnd] voice playing type=engine entity="
            << static_cast<s32>( mpTrafficControl->GetTrafficEntity()->mu16EntityIndex ) << "\n";
    }
    if ( leStageBefore == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING
         && leStageAfter == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED
         && TrafficSoundDiagTake( guEngineFinishedWitnesses, 16 ) )
    {
        *CgsDev::Log::gpDebugPrint
            << "[trafficsnd] voice finished type=engine entity="
            << static_cast<s32>( mpTrafficControl->GetTrafficEntity()->mu16EntityIndex ) << "\n";
    }

    mTrafficEngineVoice.SetParameter( 0, lfAzimuth,         &KAU_TRAFFIC_ENGINE_PARAMETERS[0] );
    mTrafficEngineVoice.SetParameter( 1, lfVelocity,        &KAU_TRAFFIC_ENGINE_PARAMETERS[1] );
    mTrafficEngineVoice.SetParameter( 2, lfSurfaceType,     &KAU_TRAFFIC_ENGINE_PARAMETERS[2] );
    mTrafficEngineVoice.SetParameter( 3, lfCarType,         &KAU_TRAFFIC_ENGINE_PARAMETERS[3] );
    mTrafficEngineVoice.SetParameter( 4, lfPitch,           &KAU_TRAFFIC_ENGINE_PARAMETERS[4] );
    mTrafficEngineVoice.SetParameter( 5, lfEngineVolume,    &KAU_TRAFFIC_ENGINE_PARAMETERS[5] );
    mTrafficEngineVoice.SetParameter( 6, lfRoadNoiseVolume, &KAU_TRAFFIC_ENGINE_PARAMETERS[6] );
    mTrafficEngineVoice.SetGain( 1, lfReverbSend, &KAU_TRAFFIC_ENGINE_SENDS[1] );
    mTrafficEngineVoice.SetGain( 0, 1.0f,         &KAU_TRAFFIC_ENGINE_SENDS[0] );

    if ( KI_DRAW_ATTACHED )
    {
        Brn3DEffectControl::DrawSphere lSphere;
        lSphere.mfHeightOffset = 1.0f;
        lSphere.mfRadius = 1.0f;
        lSphere.mfDurationVisible = 0.1f;
        lSphere.mpDebugString = nullptr;
        CgsSound::Io::Message<Brn3DEffectControl::DrawSphere> lMessage( lSphere );
        lMessage.Construct( 1, 3, static_cast<u16>( ( GetId() >> 11 ) & 0x1F ), 1,
                            CgsSound::Io::MessageHeader::E_EFFECT_TYPE_CONTROL );
        GetLogicModule()->PostMessage( lMessage );
    }

    if ( KI_SPEW_TRAFFIC && ( GetId() & 0xF800 ) == 0 )
    {
        const s32 liDCa = mpTraffic3DControl->GetDMixIOPtr()
            ? mpTraffic3DControl->GetDMixIOPtr()->GetDMixInput( 14 ) : 0;
        const s32 liDCr = mpTraffic3DControl->GetDMixIOPtr()
            ? mpTraffic3DControl->GetDMixIOPtr()->GetDMixInput( 13 ) : 0;
        BrnSound::Debug::DebugComponent::mLogCombined
            << "Idx[ " << static_cast<s32>( mpTrafficControl->GetTrafficEntity()->mu16EntityIndex ) << "]\t"
            << "Azi[ " << lfAzimuth << "]\tVel[ " << lfVelocity << "]\tSrf[ " << lfSurfaceType << "]\t"
            << "Car[ " << lfCarType << "]\tPit[ " << lfPitch << "]\t"
            << "VRn[ " << lfRoadNoiseVolume << "]\tVEn[ " << lfEngineVolume << "]\t"
            << "DCr[ " << liDCr << "]\tDCa[ " << liDCa << "]\n";
    }
}

bool TrafficEngine::Detach()
{
    if ( !BrnSound::Logic::BrnEffectObject::Detach() )
        return false;

    // [FLAG PC witness] BRN_TRAFFICSND_DIAG
    if ( TrafficSoundDiagTake( guEngineReleaseWitnesses, 64 ) )
    {
        *CgsDev::Log::gpDebugPrint
            << "[trafficsnd] voice released type=engine stage="
            << static_cast<s32>( mTrafficEngineVoice.GetUpdateStage() ) << "\n";
    }

    mTrafficEngineVoice.Release();
    return true;
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
