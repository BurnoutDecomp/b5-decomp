#include "GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

// =============================================================================
// BrnSound::Vehicles::Wheels::AISkidEffect -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//   AISkidEffect::AISkidEffect      @ 0x826D0780 (export-set hole; ppcdis)
//   AISkidEffect::CreateObject      @ 0x826D0840
//   AISkidEffect::GetController     @ 0x82685D38 (ICF-folded with MusicEffect's)
//   AISkidEffect::AttachController  @ 0x82685D48
//   AISkidEffect::Attach            @ 0x826F4D20
//   AISkidEffect::UpdateParams      @ 0x826B8FD0
//   AISkidEffect::ProcessUpdate     @ 0x826E5F10 (export-set hole; ppcdis)
//   AISkidEffect::Detach            @ 0x826F4E98
//   AISkidEffect::ReceiveImpact     @ 0x826B93A0
//   AISkidEffect::Notify            @ 0x826D0920
//   AISkidEffect::GetTypeName       @ 0x82685D28
//   sTypeInfo registration          @ CRT init bank (desc 0x82F2F774)
//
// File-scope constants (DWARF BrnAISkidEffect.cpp): KF_SMALL_SKID_INTENSITY
// flt_82F2CDE0 = 1024, KF_SMALL_SKID_DURATION flt_82F2CDE4 = 1000,
// KF_LARGE_SKID_INTENSITY flt_82F2CDE8 = 1024, KF_LARGE_SKID_DURATION flt_82F2CDEC
// = 1500. The AEMS parameter names ParameterIndexes::AEMS_Skids_Traffic (bss
// table 0x8300A694, CRT thunk 0x82C647D8): AEMS_pitch, AEMS_volume, AEMS_azimuth,
// AEMS_drift, AEMS_size; the sends SendIndexes::AEMS_Skids_Traffic (0x8300865C,
// thunk 0x82C64064): Send01, ReverbSend.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

namespace
{
const f32 KF_SMALL_SKID_INTENSITY = 1024.0f;
const f32 KF_SMALL_SKID_DURATION  = 1000.0f;
const f32 KF_LARGE_SKID_INTENSITY = 1024.0f;
const f32 KF_LARGE_SKID_DURATION  = 1500.0f;

const u32 KAU_AI_SKID_PARAMETERS[5] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_pitch" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_volume" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_azimuth" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_drift" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_size" ) ),
};
const u32 KAU_AI_SKID_SENDS[2] =
{
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "Send01" ) ),
    static_cast<u32>( CgsSound::Playback::Name::MakeHash( "ReverbSend" ) ),
};
} // namespace

// @ 0x826D0780: the BrnEffectObject base construction, PathLine<2>::ClearStages
// on mDriftInterp, mfDriftFactor = 0, VoiceWrapper::VoiceWrapper on mSkidsVoice.
AISkidEffect::AISkidEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mpPhysicsControl( 0 )
    , mDriftInterp()
    , mfDriftFactor( 0.0f )
    , mSkidsVoice()
{
    mDriftInterp.ClearStages();
}

AISkidEffect::~AISkidEffect()
{
}

// @ 0x826D0840: MemBase::operator new(200, "AISkidEffect", flavour) + ctor.
CgsSound::Logic::EffectObject* AISkidEffect::CreateObject( u32 /*luType*/ )
{
    return new AISkidEffect();
}

// Descriptor 0x82F2F774: {0x20040, "AISkidEffect", base BrnEffectObject (0x82F2E7FC),
// &CreateObject}.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* AISkidEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x20040, "AISkidEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &AISkidEffect::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpAISkidEffectReg = CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
        AISkidEffect::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* AISkidEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* AISkidEffect::GetTypeName() const
{
    return "AISkidEffect";
}

// @ 0x82685D38: `subfic r11, r4, 0 ; subfe r3, r11, r11` == (slot == 0) ? 0 : -1
// -- one controller, effect id 0 (the AI physics control).
s32 AISkidEffect::GetController( s32 aiSlot )
{
    return aiSlot == 0 ? 0 : -1;
}

// @ 0x82685D48: `if ((ctl->miObjectId & 0x7F0) == 0) mpPhysicsControl = ctl`.
void AISkidEffect::AttachController( CgsSound::Logic::EffectBase* apController )
{
    if ( apController->GetEffectID() == 0 )
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>( apController );
}

// ---------------------------------------------------------------------------
// AISkidEffect::Attach()  @ 0x826F4D20
//   EffectBase::Attach (the +0xE attach count / +0x24 detach stage, inlined);
//   playerMan = module->mEnvironment.mapStateManagers[1]     ; module+0x2958, assert "lpPlayerStateMan" (cpp:276)
//   skids = playerMan->GetContent(Name("Skids.abi"))         ; dword_82FFBF74, assert "lpSkidsContent" (cpp:279)
//   CreateParams{module, onPostInit 0, AemsFactory, "AEMS_Skids_Traffic", skids, spec 0,
//                "AEMS_Slot", "Send01", submix 1, "ReverbSend", reverb submix 2, sendIndex 0}
//   mSkidsVoice.Create(params); mSkidsVoice.Play(0); return 1;
// ---------------------------------------------------------------------------
bool AISkidEffect::Attach()
{
    if ( !CgsSound::Logic::EffectBase::Attach() )
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    CgsSound::Logic::StateManager* lpPlayerStateMan = lpModule->GetEnvironment().GetStateManager( 1 );
    CGS_ASSERT( lpPlayerStateMan != 0, "lpPlayerStateMan" );
    CgsSound::Logic::Content* lpSkidsContent = 0;
    if ( lpPlayerStateMan )
    {
        const CgsSound::Playback::Name lContentName( "Skids.abi" );
        lpSkidsContent = lpPlayerStateMan->GetContent( lContentName );
    }
    CGS_ASSERT( lpSkidsContent != 0, "lpSkidsContent" );

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule       = GetLogicModule();
    lParams.mpOnPostInit        = 0;
    lParams.mFactoryName        = static_cast<u32>( CgsSound::Playback::AemsFactorySkName().GetValue() );
    lParams.mVoiceSpecName      = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_Skids_Traffic" ) );
    lParams.mpContent           = lpSkidsContent;
    lParams.mContentSpecName    = 0;
    lParams.mSlotName           = static_cast<u32>( CgsSound::Playback::Name::MakeHash( "AEMS_Slot" ) );
    lParams.mSendName           = KAU_AI_SKID_SENDS[0];
    lParams.mSubMixVoiceID      = 1;
    lParams.mReverbSendName     = KAU_AI_SKID_SENDS[1];
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex         = 0;
    mSkidsVoice.Create( lParams );
    mSkidsVoice.Play( 0 );
    return true;
}

// ---------------------------------------------------------------------------
// AISkidEffect::UpdateParams(f32)  @ 0x826B8FD0
//   <four lazily-hashed names AEMS_yaw/drift/reverse/speed (dword_8300C71C..728) are
//    computed here and never read by this body -- not carried>
//   raw = mpPhysicsControl->mpVehiclePhysicsData (assert, BrnPhysicsControl.h:475)
//   drift = raw->mfAbsDriftScale * 1024.0
//   if (IsCrashing.cur == 1 && IsCrashing.prev != cur)                   ; just started crashing
//       mDriftInterp.Initialize(current, KF_SMALL_SKID_INTENSITY, 100.0, LINEAR)
//       mDriftInterp.AddLinkedStage(0.0, KF_SMALL_SKID_DURATION, LINEAR)
//   mDriftInterp.Update(dt)
//   mfDriftFactor = max(mDriftInterp.current, drift)                     ; fsel @0x826B9184
//   SetMixerInputValue(0, clamp(mfDriftFactor * 32.0, 0, 32767))         ; 0x826B9190..0x826B91B4
//   <byte_82FFB813 dev text / DebugRender::DrawText -- not carried>
// ---------------------------------------------------------------------------
void AISkidEffect::UpdateParams( f32 afTimeStep )
{
    const BrnSound::Vehicles::VehicleData* lpRaw = mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT( lpRaw != 0, "mpVehiclePhysicsData" );
    const f32 lfDrift = lpRaw->mfAbsDriftScale * 1024.0f;

    const CgsSound::Utils::DataPoint<bool>& lrCrashing = mpPhysicsControl->GetPhysicsData().IsCrashing;
    if ( lrCrashing.GetCurrent() && lrCrashing.GetPrevious() != lrCrashing.GetCurrent() )
    {
        mDriftInterp.Initialize( mDriftInterp.mfCurrentValue, KF_SMALL_SKID_INTENSITY, 100.0f,
                                 CgsSound::Utils::Curve::E_LINEAR );
        mDriftInterp.AddLinkedStage( 0.0f, KF_SMALL_SKID_DURATION, CgsSound::Utils::Curve::E_LINEAR );
    }
    mDriftInterp.Update( afTimeStep );

    const f32 lfCurrent = mDriftInterp.mfCurrentValue;
    mfDriftFactor = ( lfCurrent - lfDrift ) >= 0.0f ? lfCurrent : lfDrift;      // fsel

    f32 lfMixer = mfDriftFactor * 32.0f;
    lfMixer = ( -lfMixer >= 0.0f ) ? 0.0f : lfMixer;                             // max(v, 0)
    lfMixer = ( ( 32767.0f - lfMixer ) >= 0.0f ) ? lfMixer : 32767.0f;           // min(v, 32767)
    SetMixerInputValue( 0, static_cast<s32>( lfMixer ) );
}

// ---------------------------------------------------------------------------
// AISkidEffect::ProcessUpdate()  @ 0x826E5F10  (export-set hole, read with ppcdis)
//   mSkidsVoice.Update();
//   if (stage != 7 && stage != 0) {                                     ; wrapper +0x48
//     pitch   = DMix(1, DMX_PITCH); volume = DMix(0, DMX_VOL); azimuth = DMix(2, DMX_AZIM)
//     reverb  = GetRWACMixerOutputValue(4, DMX_VOL)
//     if (voice live) SetParameter(0 AEMS_pitch, pitch) ; (1 AEMS_volume, volume) ;
//                     (2 AEMS_azimuth, azimuth) ; (3 AEMS_drift, mfDriftFactor)
//     if (voice live) SetGain(0 Send01, 4.0) ; SetGain(1 ReverbSend, reverb) }
// ---------------------------------------------------------------------------
void AISkidEffect::ProcessUpdate()
{
    mSkidsVoice.Update();
    const s32 liStage = mSkidsVoice.GetState();
    if ( liStage == 7 || liStage == 0 )
        return;

    const f32 lfVolume  = GetMixerOutputValue( 0, Nicotine::DMixIO::DMX_VOL );
    const f32 lfPitch   = GetMixerOutputValue( 1, Nicotine::DMixIO::DMX_PITCH );
    const f32 lfAzimuth = GetMixerOutputValue( 2, Nicotine::DMixIO::DMX_AZIM );
    const f32 lfReverb  = GetRWACMixerOutputValue( 4, Nicotine::DMixIO::DMX_VOL );

    if ( mSkidsVoice.HasLiveVoice() )
    {
        mSkidsVoice.SetParameter( 0, lfPitch,       &KAU_AI_SKID_PARAMETERS[0] );
        mSkidsVoice.SetParameter( 1, lfVolume,      &KAU_AI_SKID_PARAMETERS[1] );
        mSkidsVoice.SetParameter( 2, lfAzimuth,     &KAU_AI_SKID_PARAMETERS[2] );
        mSkidsVoice.SetParameter( 3, mfDriftFactor, &KAU_AI_SKID_PARAMETERS[3] );
        mSkidsVoice.SetGain( 0, 4.0f,     &KAU_AI_SKID_SENDS[0] );
        mSkidsVoice.SetGain( 1, lfReverb, &KAU_AI_SKID_SENDS[1] );
    }
}

// @ 0x826F4E98: the four-stage detach switch (0/1: Release; 2; 3: BrnEffectObject::
// Detach; return 1) falls straight through in one call.
bool AISkidEffect::Detach()
{
    mSkidsVoice.Release();
    BrnSound::Logic::BrnEffectObject::Detach();
    return true;
}

// ---------------------------------------------------------------------------
// AISkidEffect::ReceiveImpact(EImpactType, bool)  @ 0x826B93A0
//   scale = hitByPlayer ? 2.0 : 1.0
//   types 1,2:   Initialize(current, KF_SMALL_SKID_INTENSITY, 100, LINEAR); AddLinkedStage(0, KF_SMALL_SKID_DURATION * scale, LINEAR)
//   types 3..6:  Initialize(current, KF_LARGE_SKID_INTENSITY, 100, LINEAR); AddLinkedStage(0, KF_LARGE_SKID_DURATION * scale, LINEAR)
// ---------------------------------------------------------------------------
void AISkidEffect::ReceiveImpact( BrnPhysics::Vehicle::EImpactType aeType, bool abHitByPlayer )
{
    const f32 lfScale = abHitByPlayer ? 2.0f : 1.0f;
    const s32 liType = static_cast<s32>( aeType );
    if ( liType == 1 || liType == 2 )
    {
        mDriftInterp.Initialize( mDriftInterp.mfCurrentValue, KF_SMALL_SKID_INTENSITY, 100.0f,
                                 CgsSound::Utils::Curve::E_LINEAR );
        mDriftInterp.AddLinkedStage( 0.0f, KF_SMALL_SKID_DURATION * lfScale, CgsSound::Utils::Curve::E_LINEAR );
    }
    else if ( liType >= 3 && liType <= 6 )
    {
        mDriftInterp.Initialize( mDriftInterp.mfCurrentValue, KF_LARGE_SKID_INTENSITY, 100.0f,
                                 CgsSound::Utils::Curve::E_LINEAR );
        mDriftInterp.AddLinkedStage( 0.0f, KF_LARGE_SKID_DURATION * lfScale, CgsSound::Utils::Curve::E_LINEAR );
    }
}

// ---------------------------------------------------------------------------
// AISkidEffect::Notify(const MessageHeader*)  @ 0x826D0920
//   if (id == 19 RACE_CAR_IMPACT) {
//     player = module->GetBrnInputStructure()->GetVehicleInterface()->GetPlayerActiveRaceCarIndex()
//              (assert "Player car index hasn't been set")
//     if (player == carA) { if (myIndex == carB) ReceiveImpact(type, true);
//                           if (myIndex == carA) ReceiveImpact(type, false); } }
// ---------------------------------------------------------------------------
void AISkidEffect::Notify( const CgsSound::Io::MessageHeader* apkMessage )
{
    if ( apkMessage->GetEventId() != BrnSound::E_SOUNDMESSAGE_RACE_CAR_IMPACT )
        return;

    const CgsSound::Io::Message<RaceCarImpactData>* lpMessage =
        static_cast<const CgsSound::Io::Message<RaceCarImpactData>*>( apkMessage );
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpModule->GetBrnInputStructure()->GetVehicleInterface();
    const s32 liPlayer = static_cast<s32>( lpVehicles->GetPlayerActiveRaceCarIndex() );
    CGS_ASSERT( liPlayer != -1, "Player car index hasn't been set" );

    if ( liPlayer == lpMessage->mData.miRaceCarA )
    {
        const BrnSound::Vehicles::VehicleState* lpVehicleState =
            static_cast<const BrnSound::Vehicles::VehicleState*>( GetStateBase() );
        const s32 liMine = static_cast<s32>( lpVehicleState->GetVehicleIndex() );
        const BrnPhysics::Vehicle::EImpactType leType =
            static_cast<BrnPhysics::Vehicle::EImpactType>( lpMessage->mData.miImpactType );
        if ( liMine == lpMessage->mData.miRaceCarB )
            ReceiveImpact( leType, true );
        if ( liMine == lpMessage->mData.miRaceCarA )
            ReceiveImpact( leType, false );
    }
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
