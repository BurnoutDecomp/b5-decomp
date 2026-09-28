#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficSoundDiag.h"
#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficHorn -- out-of-line bodies.
//
// The AEMS names are the console's CRT-hashed tables: ParameterIndexes::
// AEMS_class_horns (AEMS_type, AEMS_horn_pitch, AEMS_horn_volume, AEMS_horn_azimuth,
// AEMS_alarm_volume, AEMS_beep_on_off, AEMS_mode) and SendIndexes::AEMS_class_horns
// (Send01, ReverbSend).
//
// Not carried from ProcessUpdate: the KB_HORN_DRAW_CAR_SIZES / KB_HORN_DRAW_CAR_HOOTING
// DebugRender::DrawText labels, gated on developer switches that are zero in the image.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

namespace
{
const u32 KAU_TRAFFIC_HORN_PARAMETERS[7] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_type" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_horn_pitch" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_horn_volume" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_horn_azimuth" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_alarm_volume" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_beep_on_off" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_mode" ) ),
};

const u32 KAU_TRAFFIC_HORN_SENDS[2] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) ),
};

// The shared CRT-hashed "ReverbSend" every traffic voice's CreateParams reads.
const u32 KU_REVERB_SEND = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) );

// The AEMS patch mode an alarm plays with; a horn plays with mode 0.
const f32 KF_ALARM_PATCH_MODE = 3.0f;

// The Q15 unit scale applied to the reverb-send mixer output.
const f32 KF_Q15_TO_UNIT = 3.0518509e-05f;

u32 guHornVoiceWitnesses = 0;
u32 guHornReleaseWitnesses = 0;
} // namespace

// The console constructor inlines the BrnEffectObject base zero-inits and both leaf
// vtables, constructs mHornVoice and installs the functor's vtable.
TrafficHorn::TrafficHorn()
    : BrnEffectObject()
    , mHornVoice()
    , mHornFunctionPointer()
{
}

// The voice wrapper destructor, then the BrnEffectObject settle.
TrafficHorn::~TrafficHorn()
{
}

// MemBase::operator new(0xB0, "TrafficHorn", flavour) + the constructor; returns the
// EffectObject view (+0x04).
CgsSound::Logic::EffectObject* TrafficHorn::CreateObject( u32 /*luType*/ )
{
    return new TrafficHorn();
}

// Descriptor {0x30010, "TrafficHorn", BrnEffectObject::sTypeInfo, &CreateObject}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TrafficHorn::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x30010, "TrafficHorn",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &TrafficHorn::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpTrafficHornReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray( TrafficHorn::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TrafficHorn::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* TrafficHorn::GetTypeName() const
{
    return "TrafficHorn";
}

// One controller: control 0, the TrafficControl (identical code to
// MusicEffect::GetController).
s32 TrafficHorn::GetController( s32 liIndex )
{
    return liIndex == 0 ? 0 : -1;
}

// Identical code to TrafficSkid::AttachController.
void TrafficHorn::AttachController( CgsSound::Logic::EffectBase* lpController )
{
    if ( lpController->GetEffectID() == 0 )
        mpTrafficControl = static_cast<TrafficControl*>( lpController );
}

// Mixer input 0 carries the horn state of the last processed frame.
void TrafficHorn::UpdateParams( f32 /*lfTimeStep*/ )
{
    SetMixerInputValue( 0, mbPrevHornState ? 0x7FFF : 0 );
}

// Unless the voice is already playing: an alarm (either flavour) plays with patch
// mode 3 every frame, a horn plays with mode 0 on the frame it starts. Then the voice
// advances and takes the mixer outputs, the beep flag and the two sends.
void TrafficHorn::ProcessUpdate()
{
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* lpEntity = mpTrafficControl->GetTrafficEntity();
    const bool lbIsHooting = lpEntity->mbIsHooting;

    if ( mHornVoice.GetUpdateStage() != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING )
    {
        bool lbPlay = false;
        if ( lpEntity->muAlarmType == BrnTraffic::BrnTrafficIO::TrafficSoundEntity::E_ALARM_CLASSIC
             || lpEntity->muAlarmType == BrnTraffic::BrnTrafficIO::TrafficSoundEntity::E_ALARM_HORN )
        {
            mfAemsPatchMode = KF_ALARM_PATCH_MODE;
            lbPlay = true;
        }
        else if ( lbIsHooting != mbPrevHornState && lbIsHooting )
        {
            mfAemsPatchMode = 0.0f;
            lbPlay = true;
        }

        if ( lbPlay )
        {
            SetAemsTypeParameter();
            mHornVoice.Play( 0 );

            // [FLAG PC witness] BRN_TRAFFICSND_DIAG
            if ( TrafficSoundDiagTake( guHornVoiceWitnesses, 16 ) )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[trafficsnd] voice started type="
                    << ( mfAemsPatchMode == KF_ALARM_PATCH_MODE ? "alarm" : "horn" )
                    << " entity=" << static_cast<s32>( lpEntity->mu16EntityIndex )
                    << " size=" << static_cast<s32>( meTrafficSize ) << "\n";
            }
        }
    }

    mbPrevHornState = lbIsHooting;
    mHornVoice.Update();

    const f32 lfBeepOnOff   = lbIsHooting ? 1.0f : 0.0f;
    const f32 lfHornPitch   = GetMixerOutputValue( 1, Nicotine::DMixIO::DMX_PITCH );
    const f32 lfHornVolume  = GetMixerOutputValue( 0, Nicotine::DMixIO::DMX_VOL );
    const f32 lfAlarmVolume = GetMixerOutputValue( 5, Nicotine::DMixIO::DMX_VOL );
    const f32 lfHornAzimuth = GetMixerOutputValue( 2, Nicotine::DMixIO::DMX_AZIM );
    GetMixerOutputValue( 3, Nicotine::DMixIO::DMX_FREQ );   // read and not used, as on the console
    const f32 lfReverbSend  = GetMixerOutputValue( 4, Nicotine::DMixIO::DMX_VOL ) * KF_Q15_TO_UNIT;

    mHornVoice.SetParameter( 1, lfHornPitch,   &KAU_TRAFFIC_HORN_PARAMETERS[1] );
    mHornVoice.SetParameter( 2, lfHornVolume,  &KAU_TRAFFIC_HORN_PARAMETERS[2] );
    mHornVoice.SetParameter( 4, lfAlarmVolume, &KAU_TRAFFIC_HORN_PARAMETERS[4] );
    mHornVoice.SetParameter( 3, lfHornAzimuth, &KAU_TRAFFIC_HORN_PARAMETERS[3] );
    mHornVoice.SetParameter( 5, lfBeepOnOff,   &KAU_TRAFFIC_HORN_PARAMETERS[5] );
    mHornVoice.SetGain( 1, lfReverbSend, &KAU_TRAFFIC_HORN_SENDS[1] );
    mHornVoice.SetGain( 0, 1.0f,         &KAU_TRAFFIC_HORN_SENDS[0] );
}

// The EffectBase attach, then the horn voice created (not played) on the manager's horn
// bank, and the car's size class kept for the AEMS type parameter.
bool TrafficHorn::Attach()
{
    mHornFunctionPointer.Construct( this, &TrafficHorn::OnPostInitVoice );
    CgsSound::Logic::EffectBase::Attach();
    mfAemsPatchMode = 0.0f;
    mbPrevHornState = false;

    TrafficState* lpTrafficState = static_cast<TrafficState*>( GetStateBase() );
    CGS_ASSERT( lpTrafficState != 0, "lpTrafficState" );
    CGS_ASSERT( lpTrafficState->GetTrafficStateManager() != 0, "lpTrafficState->GetTrafficStateManager()" );
    CGS_ASSERT( mpTrafficControl != 0, "mpTrafficControl" );
    CGS_ASSERT( mpTrafficControl->GetTrafficEntity() != 0, "mpTrafficControl->GetTrafficEntity()" );

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule        = GetLogicModule();
    lParams.mpOnPostInit         = &mHornFunctionPointer;
    lParams.mFactoryName         = static_cast<u32>( CgsSound::Playback::AemsFactorySkName().GetValue() );
    lParams.mVoiceSpecName       = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_class_horns" ) );
    lParams.mpContent            = &lpTrafficState->GetTrafficStateManager()->GetHornAemsBank();
    lParams.mContentSpecName     = 0;
    lParams.mSlotName            = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_Slot" ) );
    lParams.mSendName            = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) );
    lParams.mSubMixVoiceID       = 1;
    lParams.mReverbSendName      = KU_REVERB_SEND;
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex          = 0;
    mHornVoice.Create( lParams );

    meTrafficSize = TrafficStateManager::TrafficClassToSize( mpTrafficControl->GetTrafficEntity()->muVehicleClass );
    return true;
}

bool TrafficHorn::Detach()
{
    if ( !BrnSound::Logic::BrnEffectObject::Detach() )
        return false;

    // [FLAG PC witness] BRN_TRAFFICSND_DIAG
    if ( TrafficSoundDiagTake( guHornReleaseWitnesses, 64 ) )
    {
        *CgsDev::Log::gpDebugPrint
            << "[trafficsnd] voice released type=horn stage="
            << static_cast<s32>( mHornVoice.GetUpdateStage() ) << "\n";
    }

    mHornVoice.Release();
    return true;
}

// Empty on the console (the functor's target is the shared empty body).
void TrafficHorn::OnPostInitVoice( CgsSound::Logic::VoiceWrapper& /*lrVoice*/ )
{
}

// AEMS_type takes the size class, AEMS_mode the patch mode.
void TrafficHorn::SetAemsTypeParameter()
{
    mHornVoice.SetParameter( 0, static_cast<f32>( meTrafficSize ), &KAU_TRAFFIC_HORN_PARAMETERS[0] );
    mHornVoice.SetParameter( 6, mfAemsPatchMode,                   &KAU_TRAFFIC_HORN_PARAMETERS[6] );
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
