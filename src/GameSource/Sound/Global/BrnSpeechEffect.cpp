#include "GameSource/Sound/Global/BrnSpeechEffect.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/AttribSys/Generated/classes/streammappings.h"
#include "GameSource/AttribSys/Generated/classes/speechdata.h"
#include "GameSource/AttribSys/Generated/classes/languagestreamconfiguration.h"
#include "GameSource/AttribSys/Generated/classes/languagestreamcollection.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"          // CgsCore::SPrintf (the event-intro name builder)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // [DIAG] the BRN_SPEECH_DIAG witness only
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"           // the two intro cursors' seed draw
#include "GameSource/AttribSys/Generated/attrib_findcollection.h" // Attrib::FindCollection (cases 25/35)
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"  // BrnSound::GameModeLostResults
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

#include <cstdlib>   // getenv -- [DIAG] only

namespace BrnSound
{
namespace Logic
{

namespace
{
// ARTIST dword_820AA668: ETrainingType 0..76 -> speechdata::FirstTimeTips index.
// This table is read directly by SpeechEffect::Notify @ 0x826E7B20, case 0x22.
const u8 KAE_TRAINING_TYPE_TO_TIP_INDEX[77] = {
    54, 55, 56, 57,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,
    13, 14, 15,  0, 17, 18, 19, 20, 42, 21, 22, 29, 30, 23, 24, 25,
    26, 27, 28, 31, 32, 33, 39, 34, 35, 43, 44, 45, 36, 37, 38, 40,
    41, 46, 47, 47, 47, 47, 47, 58, 59, 60, 61, 62, 63, 65, 66, 67,
    67, 67, 68, 69, 70, 64,  0,  0,  0,  0,  0,  0,  0
};

// BrnProgression::ETrainingType landmarks the console's case 0x22 switches on
// (the assert strings at BrnSpeechEffect.cpp:523/:538 name them).
const s32 KI_TRAINING_TYPE_COUNT       = 77;   // E_TRAINING_COUNT (the FirstTimeTips domain)
const s32 KI_TRAINING_TYPE_DLC1_START  = 77;
const s32 KI_TRAINING_TYPE_DLC1_END    = 78;
const s32 KI_TRAINING_TYPE_TIMED_TIP_1 = 128;  // the Atomika free-burn VO block (128..235)

// The .bss CgsSound::Playback::Name constants the console's dynamic initialisers fill
// in. Each was recovered from its own MakeHash thunk (address in the trailing comment);
// dword_82FFBF24 has no export entry at all and was read out of the image
// (thunk @0x82C62FB0: `addi r3,r11,-0x6c7c ; 820B9384` -> "Complete_All_Challenges",
//  `stw r3,-0x40dc(r11) ; 82FFBF24`).
const char* const KAPC_TROPHY_VO_1      = "Complete_All_Stunts";           // 8300A6F4 @0x82C62EC0
const char* const KAPC_TROPHY_VO_2      = "Complete_All_Jumps";            // 83008098 @0x82C62F20
const char* const KAPC_TROPHY_VO_3      = "Complete_All_Smashes";          // 8300833C @0x82C62EF0
const char* const KAPC_TROPHY_VO_6      = "Complete_All_TimeRoadRules";    // 82FFBF28 @0x82C62F50
const char* const KAPC_TROPHY_VO_7      = "Complete_All_CrashRoadRules";   // 82FFBF2C @0x82C62F80
const char* const KAPC_TROPHY_VO_10     = "Complete_All_Challenges";       // 82FFBF24 @0x82C62FB0
const char* const KAPC_DONE_RACE        = "Already_Completed_Race";           // 83005FD4 @0x82C63040
const char* const KAPC_DONE_ROAD_RAGE   = "Already_Completed_Road_Rage";      // 83005FB4 @0x82C63010
const char* const KAPC_DONE_BURNING_RT  = "Already_Completed_Burning_Route";  // 830082AC @0x82C630A0
const char* const KAPC_DONE_STUNT_RUN   = "Already_Completed_Stunt_Run";      // 830080B4 @0x82C62FE0
const char* const KAPC_DONE_MARKED_MAN  = "Already_Completed_Marked_Man";     // 8300869C @0x82C63070
const char* const KAPC_ONLINE_SR_LOSE   = "online_sr_lose";                   // 830080A4 @0x82C63100
const char* const KAPC_ONLINE_SR_INT    = "online_sr_int";                    // 830060AC @0x82C630D0

const u32 KU_MAX_SPEECH_NAME_LENGTH = 32;   // DWARF BrnSpeechEffect.h:234

// The sound-message 31 payload is the GUI event's own 24-byte record, copied verbatim by
// the dispatch (BrnSoundLogicModule.cpp case 464 -> `GuiAudioEventIntrosPayload`, a
// 24-byte opaque block in that TU's anonymous namespace). Layout-identical local mirror
// so this TU can name the Message<T> it receives; the six fields below are read at the
// console's own byte offsets (Notify reads *(msg+16/24/28/32/36/37), i.e. payload
// 0/8/12/16/20/21).
struct GuiAudioEventIntrosData
{
    u8 maData[24];
    s32 GetS32At(u32 luOffset) const
    {
        s32 liValue = 0;
        for (u32 luByte = 0; luByte < 4u; ++luByte)
            reinterpret_cast<u8*>(&liValue)[luByte] = maData[luOffset + luByte];
        return liValue;
    }
    u8 GetU8At(u32 luOffset) const { return maData[luOffset]; }
};

// ---------------------------------------------------------------------------------
// [DIAG] NOT IN THE X360 BINARY. Off unless BRN_SPEECH_DIAG is set. It reports, per
// speech message, the three quantities that decide whether a line is heard:
//   the message id + its payload, the ContentSpec the mapping/collection resolved to
//   (0 == resolved to nothing), and the PlayStream outcome (requested / queued /
//   dropped). A `[speech]` line with cs=0 is a data miss; no `[speech] notify` line at
//   all is a producer/dispatch miss. Bounded so an event storm cannot run the log away.
// DELETE-WHEN: the Atomika free-burn VOs are audible on a plain -Drive run.
// ---------------------------------------------------------------------------------
bool SpeechDiagOn()
{
    static const bool sbOn = (getenv("BRN_SPEECH_DIAG") != 0);
    return sbOn;
}
bool SpeechDiagBudget()
{
    static const s32 KI_MAX_LINES = 2000;
    static s32 siLines = 0;
    if (siLines >= KI_MAX_LINES)
        return false;
    ++siLines;
    return CgsDev::Log::gpDebugPrint != 0;
}
}

SpeechEffect::SpeechEffect()
    : BrnEffectObject(), Streaming::IStreamUser(), mpStreamingManager(0),
      mCreateParams(), mePlayState(E_STOPPED), mbSpeechStillPlaying(false),
      mbFirstTimeTipPlaying(false), meLanguage(0), muNextRoadRageIntroIndex(0),
      muNextStuntRunIntroIndex(0), muQueuedContentSpec(0),
      mbQueuedSpeechIsFirstTimeTip(false) {}

SpeechEffect::~SpeechEffect() {}

CgsSound::Logic::EffectObject* SpeechEffect::CreateObject(u32)
{
    return new SpeechEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SpeechEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x50, "SpeechEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &SpeechEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SpeechEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* SpeechEffect::GetTypeName() const { return "SpeechEffect"; }

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpSpeechEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(SpeechEffect::GetStaticTypeInfo());

bool SpeechEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    mpStreamingManager = static_cast<Streaming::StreamingStateManager*>(
        lpModule->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(mpStreamingManager != 0, "lpStreamingStateMan");

    mCreateParams.Clear();
    mCreateParams.mpLogicModule = lpModule;
    mCreateParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    mCreateParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("MusicVoiceSpec"));
    mCreateParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
    mCreateParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    mCreateParams.mSubMixVoiceID = 1;
    mCreateParams.miSendIndex = 0;
    mePlayState = E_STOPPED;
    mbSpeechStillPlaying = false;
    mbFirstTimeTipPlaying = false;
    muQueuedContentSpec = 0;
    mbQueuedSpeechIsFirstTimeTip = false;
    // X360 @0x826F7D48: meLanguage starts at 0 and the two intro cursors are seeded
    // from the module's own Random modulo the authored array length.
    meLanguage = 0;
    {
        Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());
        const u32 luRoadRage = lSpeechData.Num_RoadRageIntros();
        const u32 luStuntRun = lSpeechData.Num_StuntRunIntros();
        CGS_ASSERT(luRoadRage > 0, "luMod > 0");
        CGS_ASSERT(luStuntRun > 0, "luMod > 0");
        CgsNumeric::Random& lrRandom = lpModule->GetRandomGenerator();
        muNextRoadRageIntroIndex = luRoadRage ? (lrRandom.RandomUInt() % luRoadRage) : 0u;
        muNextStuntRunIntroIndex = luStuntRun ? (lrRandom.RandomUInt() % luStuntRun) : 0u;
    }
    return mpStreamingManager != 0;
}

// ---------------------------------------------------------------------------
// SpeechEffect::Detach  @ 0x826F7F50
//
// ⛔ CORRECTED 2026-09-16 -- this body had an INVENTED ARM and read the wrong source.
// It used to be `if (mpStreamingManager) ...`, i.e. a silent skip on a CACHED member.
// The console does neither:
//   826F7F68  bl 0x826EBF88            ; BrnEffectObject::Detach
//   826F7F80  lwz r11, 0x28(r31)       ; mpLogicModule, read LIVE
//   826F7F84  lwz r30, 0x296C(r11)     ; GetEnvironment().GetStateManager(6)
//   826F7F88  cmplwi r30, 0 / bne      ; ...and on NULL it ASSERTS (BrnSpeechEffect.cpp:225,
//   826F7F90..  BeginAssert/FireAssert ;    li r5, 0xE1) -- it does not skip
//   826F7FB0  addic. r11, r31, -4      ; the null-preserving (IStreamUser*)this adjust
//   826F7FD0  lfs f0, [0x82003F40]     ; 0.25f, the stop request's fade-out
//   ...then PostStreamRequest UNCONDITIONALLY.
// A silent `if` where the console asserts is the documented invented-arm defect class:
// it converts a loud programming error into a missing sound. The identical tail is at
// PresentationEffect::Detach @0x826F785C and AmbienceEffect::Detach @0x826F50DC.
// ---------------------------------------------------------------------------
bool SpeechEffect::Detach()
{
    if (!BrnEffectObject::Detach())                          // 826F7F68 / 826F7F78
        return false;

    // 826F7F80 / 826F7F84 -- read LIVE off the module, not from a cached member.
    Streaming::StreamingStateManager* lpStreamingStateMan =
        static_cast<Streaming::StreamingStateManager*>(
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)
                ->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");   // 826F7F90.. (cpp:225)

    // 826F7FB0..826F7FEC -- unconditional. .rdata 0x82003F40 == 0.25f.
    lpStreamingStateMan->PostStreamRequest(Streaming::StreamStopRequest(this, 0.25f));
    return true;
}

const CgsSound::Logic::VoiceWrapper::CreateParams& SpeechEffect::GetCreateParams() const
{
    return mCreateParams;
}

void SpeechEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                     f32 afGain, f32)
{
    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    // ARTIST @ 0x826BCBF0 passes ParameterIndexes::MusicVoiceSpec::PauseControl
    // as index 1.  Index 0 is a different authored parameter in MusicVoiceSpec.
    arVoice.SetParameter(1, 0.0f, &luPauseControl);
    const u32 luSend = mCreateParams.mSendName;
    const f32 lfMixerGain = GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    arVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex),
                    afGain * lfMixerGain, &luSend);
    // [DIAG] NOT IN THE X360 BINARY. The E_PLAY_REQUESTED -> E_PLAYING edge is the only
    // place in the effect that proves a VOICE IS ACTUALLY FEEDING: UpdateVoiceParams is
    // driven by the streaming manager once the stream has buffers. "requested" alone
    // cannot distinguish a played line from a stream that never opened.
    if (SpeechDiagOn() && mePlayState != E_PLAYING && SpeechDiagBudget())
        *CgsDev::Log::gpDebugPrint << "[speech] VOICE PLAYING cs="
                                   << static_cast<s32>(mCreateParams.mContentSpecName)
                                   << " gain=" << afGain << "\n";
    mbSpeechStillPlaying = true;
    mePlayState = E_PLAYING;
}

void SpeechEffect::StreamStopped()
{
    mbSpeechStillPlaying = false;
}

void SpeechEffect::PlayStream(u32 auContentSpec, bool abFirstTimeTip)
{
    if (!auContentSpec || !mpStreamingManager)
    {
        if (SpeechDiagOn() && SpeechDiagBudget())
            *CgsDev::Log::gpDebugPrint << "[speech] play DROPPED cs=" << auContentSpec
                                       << " streamman=" << (mpStreamingManager != 0) << "\n";
        return;
    }
    // X360 @0x8269EAF0: `if (mePlayState) { queue and return; }` -- ANY non-STOPPED
    // state queues, not just E_PLAYING (a second request that lands between the request
    // and the first buffer must not clobber mContentSpecName).
    if (mePlayState != E_STOPPED)
    {
        muQueuedContentSpec = auContentSpec;
        mbQueuedSpeechIsFirstTimeTip = abFirstTimeTip;
        if (SpeechDiagOn() && SpeechDiagBudget())
            *CgsDev::Log::gpDebugPrint << "[speech] play QUEUED cs=" << auContentSpec
                                       << " state=" << static_cast<s32>(mePlayState) << "\n";
        return;
    }
    mCreateParams.mContentSpecName = auContentSpec;
    mbFirstTimeTipPlaying = abFirstTimeTip;
    mbSpeechStillPlaying = false;
    mePlayState = E_PLAY_REQUESTED;
    mpStreamingManager->PostStreamRequest(Streaming::StreamRequest(this, 6, 0.1f));
    if (SpeechDiagOn() && SpeechDiagBudget())
        *CgsDev::Log::gpDebugPrint << "[speech] play REQUESTED cs=" << auContentSpec
                                   << " firsttimetip=" << (abFirstTimeTip ? 1 : 0) << "\n";
}

// X360 @0x8269E918 -- `bool GetSpeechMapping(SoundLogicModule*, Name, RefSpec&)`:
// walk streammappings::UserStringsHashed for the name and hand back the matching
// LanguageStreamConfigurations RefSpec. It does NOT resolve a ContentSpec: the
// language index is applied later, by PlaySpeech.
bool SpeechEffect::GetSpeechMapping(u32 auMappingName, Attrib::RefSpec& arConfiguration) const
{
    const BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpModule != 0, "lpLogicModule");
    Attrib::Gen::streammappings lMappings(lpModule->GetGlobalData().StreamMappings());
    return lMappings.Find(auMappingName, arConfiguration);
}

bool SpeechEffect::PlaySpeechMapping(u32 auMappingName, bool abFirstTimeTip)
{
    Attrib::RefSpec lConfiguration;
    if (!GetSpeechMapping(auMappingName, lConfiguration))
    {
        if (SpeechDiagOn() && SpeechDiagBudget())
            *CgsDev::Log::gpDebugPrint << "[speech] mapping MISS name=0x"
                                       << static_cast<s32>(auMappingName) << "\n";
        return false;
    }
    return PlaySpeech(lConfiguration, abFirstTimeTip);
}

// X360 @0x826BCC68 -- the RefSpec overload (BrnSpeechEffect.cpp:885) wraps the spec as
// a languagestreamconfiguration and falls into the configuration overload.
bool SpeechEffect::PlaySpeech(const Attrib::RefSpec& arRefSpec, bool abFirstTimeTip)
{
    Attrib::Gen::languagestreamconfiguration lLanguageStream(arRefSpec);
    CGS_ASSERT(lLanguageStream.IsValid(), "lLanguageStream.IsValid()");
    return PlaySpeech(lLanguageStream, abFirstTimeTip);
}

bool SpeechEffect::PlaySpeech(const Attrib::Gen::languagestreamconfiguration& arStream,
                              bool abFirstTimeTip)
{
    // `luLanguage = *(this+128); if (luLanguage >= Num_ContentSpecs()) -> the shared
    // zeroed default word` -- ContentSpec()'s own bound check does exactly that.
    const u32 luContentSpec = arStream.ContentSpec(static_cast<u32>(meLanguage));
    if (SpeechDiagOn() && SpeechDiagBudget())
        *CgsDev::Log::gpDebugPrint << "[speech] resolve lang=" << meLanguage
                                   << " cs=" << static_cast<s32>(luContentSpec) << "\n";
    if (!luContentSpec)
        return false;
    PlayStream(luContentSpec, abFirstTimeTip);
    return true;
}

// X360 @0x826D3098: draw Items[random % Num_Items()] and play it.
bool SpeechEffect::PlayRandomSpeechVariation(
    const Attrib::Gen::languagestreamcollection& arVariations, bool abFirstTimeTip)
{
    CGS_ASSERT(arVariations.IsValid(), "lSpeechVariations.IsValid()");
    const u32 luCount = arVariations.Num_Items();
    CGS_ASSERT(luCount > 0, "lSpeechVariations.Num_Items() > 0");
    if (!luCount)
        return false;
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    const u32 luIndex = lpModule->GetRandomGenerator().RandomUInt() % luCount;
    return PlaySpeech(arVariations.Items(luIndex), abFirstTimeTip);
}

bool SpeechEffect::PlayFirstTimeTip(s32 aiTrainingType)
{
    const BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());

    // ⭐ THE THREE ARMS OF X360 Notify case 0x22 (@0x826E7C34..0x826E7DB0). The old body
    // here had only the first, and `return false` for everything at or above 77 -- which
    // is EVERY Atomika free-burn line (TrainingManager::PlayNewAtomikaFreeburnVO
    // @0x82365FC8 requests ids 128..235).
    if (aiTrainingType >= KI_TRAINING_TYPE_TIMED_TIP_1)
    {
        // `sub_8269E638(v83, mSpeechData.mpAttributeData + 368, 0)` -- the Atomika
        // free-burn VO collection, indexed by (type - E_TRAINING_TYPE_TIMED_TIP_1).
        Attrib::Gen::languagestreamcollection lAtomikaFreeburnVos(
            lSpeechData.AtomikaFreeburnVos());
        CGS_ASSERT(lAtomikaFreeburnVos.IsValid(), "lAtomikaFreeburnVos.IsValid()");
        const u32 luIndex = static_cast<u32>(aiTrainingType - KI_TRAINING_TYPE_TIMED_TIP_1);
        const u32 luCount = lAtomikaFreeburnVos.Num_Items();
        CGS_ASSERT(luIndex < luCount,
                   "(lpMessage->mData - BrnProgression::E_TRAINING_TYPE_TIMED_TIP_1)"
                   " < static_cast<int32_t>(lAtomikaFreeburnVos.Num_Items())");
        if (SpeechDiagOn() && SpeechDiagBudget())
            *CgsDev::Log::gpDebugPrint << "[speech] atomika-freeburn type=" << aiTrainingType
                                       << " idx=" << static_cast<s32>(luIndex)
                                       << " items=" << static_cast<s32>(luCount)
                                       << " valid=" << (lAtomikaFreeburnVos.IsValid() ? 1 : 0)
                                       << "\n";
        if (luIndex >= luCount)
            return false;
        return PlaySpeech(lAtomikaFreeburnVos.Items(luIndex), true);
    }

    if (aiTrainingType >= KI_TRAINING_TYPE_DLC1_START)
    {
        // The console indexes dword_830060AC[type - 77]; its own assert restricts the
        // range to [DLC1_START, DLC1_END) == exactly 77, whose Name is "online_sr_int".
        CGS_ASSERT(aiTrainingType < KI_TRAINING_TYPE_DLC1_END,
                   "lpMessage->mData >= BrnProgression::E_TRAINING_TYPE_DLC1_START &&"
                   " lpMessage->mData < BrnProgression::E_TRAINING_TYPE_DLC1_END");
        if (aiTrainingType >= KI_TRAINING_TYPE_DLC1_END)
            return false;
        return PlaySpeechMapping(
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(KAPC_ONLINE_SR_INT)), true);
    }

    if (aiTrainingType < 0)
        return false;

    const u32 luTipIndex = KAE_TRAINING_TYPE_TO_TIP_INDEX[aiTrainingType];
    CGS_ASSERT(luTipIndex <= 0x46u,
               "leTrainingType >= 0 && leTrainingType < AttribSys::Enums::Training::E_TRAINING_COUNT");
    if (SpeechDiagOn() && SpeechDiagBudget())
        *CgsDev::Log::gpDebugPrint << "[speech] first-time-tip type=" << aiTrainingType
                                   << " tipidx=" << static_cast<s32>(luTipIndex) << "\n";
    return PlaySpeech(lSpeechData.FirstTimeTips(luTipIndex), true);
}

void SpeechEffect::PostSpeechFinished()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    u8 luPayload = 0;
    if (mbFirstTimeTipPlaying)
    {
        lpModule->GetPreUpdateOutput().GetAudioEffectsMessageQueue().AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&luPayload), 2, 1);
        mbFirstTimeTipPlaying = false;
    }
    CgsModule::VariableEventQueue<256, 16>* lpGuiQueue =
        reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
            lpModule->GetPreUpdateOutput().maGuiOutEventQueueStorage);
    lpGuiQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luPayload), 467, 1);
}

void SpeechEffect::UpdateParams(f32)
{
    if (mePlayState == E_PLAYING && !mbSpeechStillPlaying)
    {
        if (SpeechDiagOn() && SpeechDiagBudget())
            *CgsDev::Log::gpDebugPrint << "[speech] VOICE FINISHED cs="
                                       << static_cast<s32>(mCreateParams.mContentSpecName)
                                       << " firsttimetip="
                                       << (mbFirstTimeTipPlaying ? 1 : 0) << "\n";
        mePlayState = E_STOPPED;
        PostSpeechFinished();
        if (muQueuedContentSpec)
        {
            const u32 luQueued = muQueuedContentSpec;
            const bool lbFirstTime = mbQueuedSpeechIsFirstTimeTip;
            muQueuedContentSpec = 0;
            mbQueuedSpeechIsFirstTimeTip = false;
            PlayStream(luQueued, lbFirstTime);
        }
    }
    SetMixerInputValue(0, mbSpeechStillPlaying ? 0x7FFF : 0);
    mbSpeechStillPlaying = false;
}

// X360 @0x8269E6C8. "One on one" == no more active race cars than the player plus one.
bool SpeechEffect::IsOneOnOne() const
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpcIface =
        lpModule->GetBrnInputStructure()->GetVehicleInterface();
    if (!lpcIface)
        return false;
    // `v4 = (mePlayerActiveRaceCarIndex != -1) ? mbIsPlayerCarActive : 0;
    //  v7 = ((cntlzw(v4) & 0x20) == 0) + 1`  -- 2 when the player car is active, else 1.
    const bool lbPlayerActive =
        (lpcIface->GetPlayerActiveRaceCarIndex() != ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
        lpcIface->IsPlayerCarActive();
    const s32 liLimit = lbPlayerActive ? 2 : 1;
    s32 liCount = 0;
    for (s32 liIndex = 0; liIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
    {
        if (lpcIface->IsRaceCarActive(static_cast<EActiveRaceCarIndex>(liIndex)) &&
            ++liCount > liLimit)
        {
            return false;
        }
    }
    return true;
}

// X360 @0x8269E838: `*(4 * (mpScoringInterface[653] + 24) + mpOnlineScoringInterface) == 2`.
// Both interfaces are size-attested opaque blocks in this tree (BrnRootSoundModuleIo.h),
// so the console's own two reads are spelled out against those blocks rather than
// invented field names.
bool SpeechEffect::IsLocalPlayerRunner() const
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    const BrnSound::Module::Io::RootInputBuffer::ScoringOutputInterface* lpcScoring =
        lpInput->GetScoringInterface();
    const BrnSound::Module::Io::RootInputBuffer::OnlineScoringOutputInterface* lpcOnline =
        lpInput->GetOnlineScoringInterface();
    CGS_ASSERT(lpcScoring != 0 && lpcOnline != 0,
               "lpScoringInterface && lpOnlineScoringInterface");
    if (!lpcScoring || !lpcOnline)
        return false;
    const u32 luLocalPlayer = reinterpret_cast<const u8*>(lpcScoring)[653];
    return reinterpret_cast<const s32*>(lpcOnline)[luLocalPlayer + 24] == 2;
}

s32 SpeechEffect::GetLanguage(s32 aeCgsLanguage)
{
    switch (aeCgsLanguage)
    {
    case 10: return 1;
    case 11: return 2;
    case 15: return 3;
    case 16: return 5;
    case 22: return 4;
    default: return 0;
    }
}

// =====================================================================================
// ⭐ Notify -- X360 @0x826E7B20, all fourteen jump-table entries. The tree carried only
// two (0x22 first-time tip and 0x24 trigger VO); the sound dispatch POSTS twelve, so
// every other speech line the game asked for was received and dropped on the floor.
// The payload offsets below are the console's own: a CgsSound::Io::Message<T>'s mData
// starts at header+16, so `*(a2+16)` is mData[0], `*(a2+20)` is mData[4], ...
// =====================================================================================
void SpeechEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    CGS_ASSERT(apMessage != 0, "lpMessageHeader");
    if (!apMessage)
        return;

    const s32 liEventId = static_cast<s32>(apMessage->GetEventId());
    if (SpeechDiagOn() && SpeechDiagBudget())
        *CgsDev::Log::gpDebugPrint << "[speech] notify id=" << liEventId
                                   << " state=" << static_cast<s32>(mePlayState) << "\n";

    switch (liEventId)
    {
    case 0x19:   // 25 -- new rival sequence
    case 0x23:   // 35 -- car won
    {
        // `ld r4, 0x10(r30)` then `FindCollection(<languagestreamconfiguration class>,
        // r4)`: the 64-bit payload IS the collection key of the line's stream config.
        const CgsSound::Io::Message<u64>* lpMessage =
            static_cast<const CgsSound::Io::Message<u64>*>(apMessage);
        static const u64 KU_LANGUAGESTREAMCONFIGURATION_CLASS = 0xE0E111DBD50CC1F0ull;
        Attrib::Collection* lpCollection =
            Attrib::FindCollection(KU_LANGUAGESTREAMCONFIGURATION_CLASS, lpMessage->mData);
        Attrib::Gen::languagestreamconfiguration lSpeechStream(lpCollection, 0);
        if (lSpeechStream.IsValid())
            PlaySpeech(lSpeechStream, false);
        break;
    }

    case 0x1B:   // 27 -- rank up (the licence-upgrade bank, 1-based)
    {
        const CgsSound::Io::Message<s32>* lpMessage =
            static_cast<const CgsSound::Io::Message<s32>*>(apMessage);
        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());
        const s32 liRank = lpMessage->mData;
        CGS_ASSERT(liRank >= 1 &&
                       liRank <= static_cast<s32>(lSpeechData.Num_LicenseUpgradeVoiceOvers()),
                   "Trying to play a rank up sequence out of range");
        if (liRank < 1)
            break;
        PlaySpeech(lSpeechData.LicenseUpgradeVoiceOvers(static_cast<u32>(liRank - 1)), false);
        break;
    }

    case 0x1D:   // 29 -- game mode started (the two ONLINE modes only)
    {
        const CgsSound::Io::Message<s32>* lpMessage =
            static_cast<const CgsSound::Io::Message<s32>*>(apMessage);
        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());
        if (lpMessage->mData == 10)
        {
            const u32 luVo = IsOneOnOne() ? 6u : 7u;
            Attrib::Gen::languagestreamcollection lVariations(
                *static_cast<const Attrib::RefSpec*>(lSpeechData.OnlineVoiceOvers(luVo)));
            PlayRandomSpeechVariation(lVariations, false);
        }
        else if (lpMessage->mData == 13)
        {
            const u32 luVo = IsLocalPlayerRunner() ? 8u : 9u;
            Attrib::Gen::languagestreamcollection lVariations(
                *static_cast<const Attrib::RefSpec*>(lSpeechData.OnlineVoiceOvers(luVo)));
            PlayRandomSpeechVariation(lVariations, false);
        }
        break;
    }

    case 0x1E:   // 30 -- trophy unlocked
    {
        const CgsSound::Io::Message<s32>* lpMessage =
            static_cast<const CgsSound::Io::Message<s32>*>(apMessage);
        const char* lpcName = 0;
        switch (lpMessage->mData)
        {
        case 1:  lpcName = KAPC_TROPHY_VO_1;  break;
        case 2:  lpcName = KAPC_TROPHY_VO_2;  break;
        case 3:  lpcName = KAPC_TROPHY_VO_3;  break;
        case 6:  lpcName = KAPC_TROPHY_VO_6;  break;
        case 7:  lpcName = KAPC_TROPHY_VO_7;  break;
        case 10: lpcName = KAPC_TROPHY_VO_10; break;
        default: break;   // the console's `goto LABEL_5` -- no line for the other trophies
        }
        if (lpcName)
            PlaySpeechMapping(static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcName)), false);
        break;
    }

    case 0x1F:   // 31 -- the event-start (Atomika) intro
    {
        const CgsSound::Io::Message<GuiAudioEventIntrosData>* lpMessage =
            static_cast<const CgsSound::Io::Message<GuiAudioEventIntrosData>*>(apMessage);
        const GuiAudioEventIntrosData& lrIntro = lpMessage->mData;
        const s32 liEventIndex = lrIntro.GetS32At(0);    // console *(a2+16)
        const s32 liCompass    = lrIntro.GetS32At(8);    // console *(a2+24)
        const s32 liGameMode   = lrIntro.GetS32At(12);   // console *(a2+28)
        const u32 luLength     = static_cast<u32>(lrIntro.GetS32At(16)); // console *(a2+32)
        const u8  lu8Completed = lrIntro.GetU8At(20);    // console *(a2+36)
        const u8  lu8Attempts  = lrIntro.GetU8At(21);    // console *(a2+37)
        const bool lbLong      = (luLength < 5u);

        if (lu8Completed && lu8Attempts < 5)
        {
            // Already completed this event: a fixed per-mode "you already did this" line.
            const char* lpcName = 0;
            switch (liGameMode)
            {
            case 0: lpcName = KAPC_DONE_RACE;       break;
            case 3: lpcName = KAPC_DONE_ROAD_RAGE;  break;
            case 5: lpcName = KAPC_DONE_BURNING_RT; break;
            case 7: lpcName = KAPC_DONE_STUNT_RUN;  break;
            case 8: lpcName = KAPC_DONE_MARKED_MAN; break;
            default: break;
            }
            if (lpcName)
                PlaySpeechMapping(static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcName)), false);
            break;
        }

        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());
        char laName[KU_MAX_SPEECH_NAME_LENGTH];
        switch (liGameMode)
        {
        case 0:   // race -- "<mode><compass>_<n>[_ooo]_<l|s>"
            CgsCore::SPrintf(laName, KU_MAX_SPEECH_NAME_LENGTH, "%s%s_%d%s_%s",
                             GameModeToString(0, liGameMode),
                             CompassDirectionToString(liCompass),
                             liEventIndex,
                             IsOneOnOne() ? "_ooo" : "",
                             lbLong ? "l" : "s");
            PlaySpeechMapping(static_cast<u32>(CgsSound::Playback::Name::MakeHash(laName)), false);
            break;

        case 5:   // burning route
        case 8:   // marked man -- "<mode><compass>_<n>_<l|s>"
            CgsCore::SPrintf(laName, KU_MAX_SPEECH_NAME_LENGTH, "%s%s_%d_%s",
                             GameModeToString(0, liGameMode),
                             CompassDirectionToString(liCompass),
                             liEventIndex,
                             lbLong ? "l" : "s");
            PlaySpeechMapping(static_cast<u32>(CgsSound::Playback::Name::MakeHash(laName)), false);
            break;

        case 3:   // road rage -- walk the authored intro bank
        {
            const u32 luCount = lbLong ? lSpeechData.Num_RoadRageIntros()
                                       : lSpeechData.Num_RoadRageIntrosShort();
            CGS_ASSERT(luCount > 0, "luMod > 0");
            if (!luCount)
                break;
            muNextRoadRageIntroIndex = (muNextRoadRageIntroIndex + 1u) % luCount;
            PlaySpeech(lbLong ? lSpeechData.RoadRageIntros(muNextRoadRageIntroIndex)
                              : lSpeechData.RoadRageIntrosShort(muNextRoadRageIntroIndex),
                       false);
            break;
        }

        case 7:   // stunt run -- same, on its own bank
        {
            const u32 luCount = lbLong ? lSpeechData.Num_StuntRunIntros()
                                       : lSpeechData.Num_StuntRunIntrosShort();
            CGS_ASSERT(luCount > 0, "luMod > 0");
            if (!luCount)
                break;
            muNextStuntRunIntroIndex = (muNextStuntRunIntroIndex + 1u) % luCount;
            PlaySpeech(lbLong ? lSpeechData.StuntRunIntros(muNextStuntRunIntroIndex)
                              : lSpeechData.StuntRunIntrosShort(muNextStuntRunIntroIndex),
                       false);
            break;
        }

        default:
            break;
        }
        break;
    }

    case 0x20:   // 32 -- game mode LOST ("you lost this one again")
    {
        if (mePlayState != E_STOPPED)   // console `if (!*(this+56))`
            break;
        const CgsSound::Io::Message<BrnSound::GameModeLostResults>* lpMessage =
            static_cast<const CgsSound::Io::Message<BrnSound::GameModeLostResults>*>(apMessage);
        const s32 liGameMode = lpMessage->mData.meGameMode;
        const s32 liLosses   = lpMessage->mData.miNumLossesForGameMode;
        if (liGameMode >= 10)
        {
            // The three online stunt-run modes share one "you lost" line.
            if (liGameMode == 12 || liGameMode == 14 || liGameMode == 17)
            {
                PlaySpeechMapping(
                    static_cast<u32>(CgsSound::Playback::Name::MakeHash(KAPC_ONLINE_SR_LOSE)), false);
            }
            break;
        }
        if (liLosses > 5)
            break;   // console `else if (*(a2+20) <= 5)`

        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());
        const Attrib::RefSpec* lpBank = 0;
        switch (liGameMode)
        {
        case 0: lpBank = &lSpeechData.RaceLostVoiceOvers();         break;  // data+296
        case 3: lpBank = &lSpeechData.RoadRageLostVoiceOvers();     break;  // data+272
        case 5: lpBank = &lSpeechData.BurningRouteLostVoiceOvers(); break;  // data+344
        case 7: lpBank = &lSpeechData.StuntRunLostVoiceOvers();     break;  // data+248
        case 8: lpBank = &lSpeechData.MarkedManLostVoiceOvers();    break;  // data+320
        default: break;
        }
        if (!lpBank)
            break;
        Attrib::Gen::languagestreamcollection lVoiceOvers(*lpBank);
        CGS_ASSERT(lVoiceOvers.IsValid(), "lVoiceOvers.IsValid()");
        CGS_ASSERT(liLosses > 0, "lpMessage->mData.miNumLossesForGameMode > 0");
        if (liLosses <= 0)
            break;
        PlaySpeech(lVoiceOvers.Items(static_cast<u32>(liLosses - 1)), false);
        break;
    }

    case 0x21:   // 33 -- set language
    {
        const CgsSound::Io::Message<s32>* lpMessage =
            static_cast<const CgsSound::Io::Message<s32>*>(apMessage);
        meLanguage = GetLanguage(lpMessage->mData);
        if (SpeechDiagOn() && SpeechDiagBudget())
            *CgsDev::Log::gpDebugPrint << "[speech] set-language cgs=" << lpMessage->mData
                                       << " -> " << meLanguage << "\n";
        break;
    }

    case 0x22:   // 34 -- first-time tip / DLC tip / Atomika free-burn VO
    {
        const CgsSound::Io::Message<s32>* lpMessage =
            static_cast<const CgsSound::Io::Message<s32>*>(apMessage);
        PlayFirstTimeTip(lpMessage->mData);
        break;
    }

    case 0x24:   // 36 -- trigger VO by mapping name
    {
        const CgsSound::Io::Message<CgsSound::Playback::Name>* lpMessage =
            static_cast<const CgsSound::Io::Message<CgsSound::Playback::Name>*>(apMessage);
        PlaySpeechMapping(static_cast<u32>(lpMessage->mData.GetValue()), false);
        break;
    }

    case 0x25:   // 37 -- online VO by index
    {
        if (mePlayState != E_STOPPED)   // console `if (!*(this+56))`
            break;
        const CgsSound::Io::Message<s32>* lpMessage =
            static_cast<const CgsSound::Io::Message<s32>*>(apMessage);
        const BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        Attrib::Gen::speechdata lSpeechData(lpModule->GetGlobalData().SpeechData());
        Attrib::Gen::languagestreamcollection lVariations(
            *static_cast<const Attrib::RefSpec*>(
                lSpeechData.OnlineVoiceOvers(static_cast<u32>(lpMessage->mData))));
        PlayRandomSpeechVariation(lVariations, false);
        break;
    }

    case 0x26:   // 38 -- fade / stop whatever is playing
    {
        if (mePlayState == E_STOPPED)   // console `if (*(this+56))`
            break;
        const CgsSound::Io::Message<f32>* lpMessage =
            static_cast<const CgsSound::Io::Message<f32>*>(apMessage);
        CGS_ASSERT(mpStreamingManager != 0, "lpStreamingStateMan");
        if (mpStreamingManager)
        {
            mpStreamingManager->PostStreamRequest(
                Streaming::StreamStopRequest(this, lpMessage->mData));
        }
        break;
    }

    default:
        CGS_ASSERT(false, "Unexpected sound message.");
        break;
    }
}

const char* SpeechEffect::CompassDirectionToString(int aiDirection)
{
    static const char* const kapDirections[] = { "n", "nw", "w", "sw", "s", "se", "e", "ne" };
    if (aiDirection >= 0 && aiDirection < 8)
        return kapDirections[aiDirection];
    CGS_ASSERT(false, "Unknown compass direction");
    return "";
}

const char* SpeechEffect::GameModeToString(int, int aiMode)
{
    switch (aiMode)
    {
    case 0: return "r";
    case 3: return "rr";
    case 5: return "pc";
    case 7: return "sr";
    case 8: return "mm";
    default: return "";
    }
}

} // namespace Logic
} // namespace BrnSound
