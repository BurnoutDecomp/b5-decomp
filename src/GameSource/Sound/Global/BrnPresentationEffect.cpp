#include "GameSource/Sound/Global/BrnPresentationEffect.h"
#include "GameSource/Sound/BrnDebugComponent.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // E_MODE_ONLINE_FREE_BURN_LOBBY, E_GMS_*
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// =============================================================================
// BrnSound::Logic::PresentationEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PresentationEffect is a multiple-inheritance effect-object leaf: BrnEffectObject dual
// base (@ +0/+4) + Streaming::IStreamUser (@ +0x38). See BrnPresentationEffect.h for the
// triple-vptr layout rationale, the maVoices[4] AgingVoice array, and the
// deferred-member FLAGs.
//
// This TU's SHIPPED function set:
//   PresentationEffect()                        @ 0x826E7628  (ctor)
//   FindFreeVoice()                             @ 0x82687D68  (DWARF cpp:554)
//   FindOrStealAVoice(const PresentationEntry&) @ 0x826D2AD8  (DWARF cpp:485)
//   `vector deleting destructor'                @ 0x826E78A8  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}'   @ 0x826E7728  (compiler-synthesised)
// =============================================================================

namespace BrnSound
{
namespace Logic
{
bool KB_SPEW_PRESENTATION_MESSAGES = false; // ARTIST82FFB8D3, DecFIGS :23

// ARTIST pointer table 0x82F2CFC0, indexed as in the original: action labels from +0, and
// off_82F2CFF8 (entry 14, "Pitch") is the mixer-output label table Play reads.
static const char* const KAPC_PRESENTATION_LABELS[27] = {
    "OnEnter", "OnExit", "OnFocus", "OnLoseFocus", "OnAccept", "OnCancel",
    "OnTick", "OnChange", "OnUp", "OnDown", "OnLeft", "OnLeftSweep",
    "OnRight", "OnRightSweep", "Pitch", "Azimuth", "Cutoff", "VolumeMenu",
    "VolumeMenu2", "VolumeMenu3", "VolumeInGame", "VolumeInGame2",
    "VolumeInGame3", "VolumeStream", "Off", "On", "SwitchedOn"
};
static const char* const* const KAPC_PRESENTATION_OUTPUT_LABELS = KAPC_PRESENTATION_LABELS + 14;

// AttribSys::Enums::PresentationBehaviour::SustainedByTick (DWARF PresentationBehaviour.h:12).
// No home for that enum in the tree yet; UpdateParams compares the literal at 0x826D2994.
static const u8 KU8_BEHAVIOUR_SUSTAINED_BY_TICK = 1;

// [FLAG PC witness] [presentation] NOT IN THE X360 BINARY. BRN_AEMS_DIAG=1, capped.
static void PresentationDiag(const char* lpcFormat, s32 aiA, s32 aiB, s32 aiC)
{
    static const bool sbEnabled = std::getenv("BRN_AEMS_DIAG") != 0;
    static u32 suLines = 0;
    if (!sbEnabled || suLines >= 120u)
        return;
    ++suLines;
    char lacMsg[160];
    std::snprintf(lacMsg, sizeof(lacMsg), lpcFormat, aiA, aiB, aiC);
    CgsDev::Log::WriteToLog(lacMsg);
}

// ---------------------------------------------------------------------------
// PresentationEffect::PresentationEffect()  @ 0x826E7628  (MINIMAL -- see FLAGs)
//
// X360 STORE ORDER (0x826E7628-0x826E7720):
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C ; sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8
//   stb 0,0x30 ; stw 0,0x28 ; stw 0,0x24 ; sth 0,0x12       ; leaf scalars (un-homed)
//   stw off_820AE954,4 ; stw off_820ABB88,0x38               ; (transient base vptrs)
//   stw off_820B5F44,0 ; stw off_820B5F10,4 ; stw off_820B5F04,0x38 ; (final 3 vptrs)
//   for (i = 3; i >= 0; --i) over v2 = this+0x40, v2 += 0x80: ; maVoices[4]
//       sth 0, 0(v2)                            ; AgingVoice::mu16Age = 0  @slot+0x00
//       CgsSound::Logic::VoiceWrapper::VoiceWrapper(v2 + 4);   ; mVoice ctor @slot+0x04
//   stw 0, 0x25C..0x284 ; stw -1, 0x288         ; leaf words (un-homed)
//   Attrib::Gen::presentationactionlist::presentationactionlist(this+0x28C, 0, 0);
//   stw 0, 0x29C ; stw 0, 0x2A0                 ; leaf words (un-homed)
//   return this
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor -- it inlines the
// base member zero-inits and installs THREE leaf vptrs directly (this+0 primary + this+4
// IResourceRequester sub-object == the committed BrnEffectObject dual base; this+0x38 ==
// the IStreamUser interface sub-object). In reconstructed C++ those three vptr installs +
// base member zero-inits are produced structurally by the two base sub-objects' own
// default constructors (BrnEffectObject BY NAME + IStreamUser BY NAME). The only
// hand-written tail effect is the attested maVoices[] AgingVoice array (each slot zeroes
// mu16Age and default-constructs its embedded VoiceWrapper).
//
// FLAG (un-homed leaf members): the inlined leaf scalar zero-inits (+0x08..+0x34) and the
// words in the +0x25C..+0x2A0 span (all zero except the single -1 @ +0x288) target
// PresentationEffect's OWN un-homed leaf members (mau8DataOffsets/mau8DataEnds/
// mStreamParams/mMode/mu8StreamOutput). They are DECLARATION-ONLY / DEFERRED per the
// anti-fabrication rule -- NOT re-emitted as raw-offset stores or invented fields.
//
// FLAG (Attrib::Gen::presentationactionlist mActions DEFERRED): the tail
// `presentationactionlist(this+0x28C, 0, 0)` is an AttribSys-generated table ctor with
// the SAME (ptr,0,0) shape as the committed StreamingEffect::streamsettings sibling; no
// generated-class home exists for it, so it is DEFERRED (not materialised, not fabricated).
// ---------------------------------------------------------------------------
PresentationEffect::PresentationEffect()
    : BrnEffectObject()                       // primary vptr @+0 + IResourceRequester vptr @+4, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    , maVoices()                              // 4x { mu16Age = 0 ; VoiceWrapper ctor } -- this+0x40, stride 0x80
    , mau8DataOffsets()
    , mau8DataEnds()
    , mStreamParams()
    , mActions()
    , mMode()                                 // stw 0, 0x29C ; stw 0, 0x2A0
    , mu8StreamOutput(9)
{
    // The inlined leaf scalar zero-inits (mau8DataOffsets/mau8DataEnds/mStreamParams/
    // mMode/mu8StreamOutput region) and the Attrib::Gen::presentationactionlist mActions
    // tail construction are DECLARATION-ONLY / DEFERRED (see FLAGs); NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~PresentationEffect  (the out-of-line anchor the vector deleting destructor
// @ 0x826E78A8 and its adjustor{4} @ 0x826E7728 forward to).
//
//   0x826E78A8 vector deleting destructor:
//     bl ~PresentationEffect ; if (a2 & 1) { free via off_82FFB954 (slot +0x14) } ; return this
//   0x826E7728 adjustor{4}: addi r3,r3,-4 ; b <vector deleting destructor>
//     -> recovers the primary PresentationEffect `this` from the +4 IResourceRequester
//        sub-object pointer, then tail-forwards to the vector deleting destructor.
//
// Both are MSVC compiler-synthesised thunks over this virtual destructor. All observable
// member teardown lives in the committed BrnEffectObject / IStreamUser base chains + the
// leaf members (maVoices), so this anchor body is empty; the host toolchain re-synthesises
// the vector deleting destructor + the +4 MI adjustor from this class's MI base list +
// virtual dtor. The (a2 & 1) tail frees the object through the global sound MemBase
// allocator (off_82FFB954); that allocator is not homed here, so the `delete` half is
// left to the host operator delete (no fabricated allocator).
// ---------------------------------------------------------------------------
PresentationEffect::~PresentationEffect()
{
}

CgsSound::Logic::EffectObject* PresentationEffect::CreateObject(u32)
{
    return new PresentationEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
PresentationEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x00, "PresentationEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &PresentationEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
PresentationEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* PresentationEffect::GetTypeName() const
{
    return "PresentationEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpPresentationEffectReg = CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
        PresentationEffect::GetStaticTypeInfo());

bool PresentationEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    mActions.ChangeWithDefault(lpModule->GetGlobalData().PresentationActions());
    CGS_ASSERT(mActions.NumOfActions() > 0, "mActions.NumOfActions() > 0");
    CalculateDataOffsets(mActions);                 // ARTIST 0x8269E348
    mMode.Flush(E_MODE_NONE);                       // ARTIST 0x8269E350 / 0x8269E358
    mu8StreamOutput = 9;                            // ARTIST 0x8269E35C
    return true;
}

// ---------------------------------------------------------------------------
// PresentationEffect::CalculateDataOffsets  @ 0x82687DC0
//   Per action: the first and last row of the action list that carries it. Resolve
//   @0x82687990 walks only that range. r27 = this+0x240 == mau8DataOffsets,
//   r24 = this+0x24E == mau8DataEnds. An action with no row keeps first 0xFF / last 0,
//   an empty range. The asserts are not guards: the asm stores straight after them.
// ---------------------------------------------------------------------------
void PresentationEffect::CalculateDataOffsets(
    const Attrib::Gen::presentationactionlist& arActionList)
{
    for (s32 liIndex = 0; liIndex < 14; ++liIndex)
    {
        mau8DataOffsets[liIndex] = 0xFF;
        mau8DataEnds[liIndex] = 0;
    }

    const u16 lu16NumActions = arActionList.NumOfActions();
    CGS_ASSERT(lu16NumActions < 0xFF, "lu16NumActions < 0xFF");

    for (s32 liIndex = 0; liIndex < static_cast<s32>(lu16NumActions); ++liIndex)
    {
        const s32 leAction = arActionList.Actions(static_cast<u32>(liIndex));
        CGS_ASSERT(leAction < 14, "leAction < PresentationAction::KI_NUM_ENUMS");

        if (mau8DataOffsets[leAction] == 0xFF)
            mau8DataOffsets[leAction] = static_cast<u8>(liIndex);
        mau8DataEnds[leAction] = static_cast<u8>(liIndex);
    }

    PresentationDiag("[FLAG PC witness] [presentation] rows=%d firstAction0=%d lastAction0=%d\n",
                     lu16NumActions, mau8DataOffsets[0], mau8DataEnds[0]);
}

const CgsSound::Logic::VoiceWrapper::CreateParams&
PresentationEffect::GetCreateParams() const
{
    return mStreamParams;
}

// ---------------------------------------------------------------------------
// PresentationEffect::UpdateVoiceParams  @ 0x826BC4E0
//   The stream's gain follows the mixer output Play or UpdateParams selected:
//     0x826BC554 GetDMixOutput(mu8StreamOutput, 0), scaled by 0x820AA8F8 == 1/32767
//     0x826BC5AC SetParameter(1, 0.0f, "PauseControl") (name: sub_82C63890)
//     0x826BC5D8 SetGain(0, output * afGain, send name)
//   The assert at 0x826BC518 is not a guard.
//   [FLAG] The console reads the send name from dword_83006064, which has no writer in the
//   exports or in a scan for `stw .., 0x6064(..)`. SpeechEffect reads the same word; both trees
//   pass their stream's mSendName, so that stands here unproven.
// ---------------------------------------------------------------------------
void PresentationEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                           f32 afGain, f32)
{
    CGS_ASSERT(mu8StreamOutput <= 9,
               "mu8StreamOutput <= AttribSys::Enums::PresentationOutput::VolumeStream");
    const f32 lfOutput = GetMixerOutputValue(mu8StreamOutput, Nicotine::DMixIO::DMX_VOL) / 32767.0f;

    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    arVoice.SetParameter(1, 0.0f, &luPauseControl);

    const u32 luSend = mStreamParams.mSendName;
    arVoice.SetGain(0, lfOutput * afGain, &luSend);

    // [FLAG PC witness] NOT IN THE X360 BINARY. The first few updates of a run, in thousandths.
    static u32 suGainLines = 0;
    if (suGainLines < 4u)
    {
        ++suGainLines;
        PresentationDiag("[FLAG PC witness] [presentation] streamgain slot=%d output=%d in=%d\n",
                         mu8StreamOutput, static_cast<s32>(lfOutput * 1000.0f),
                         static_cast<s32>(afGain * 1000.0f));
    }
}

void PresentationEffect::StreamStopped()
{
    mu8StreamOutput = 9;
}

bool PresentationEffect::Resolve(s32 aiAction, u64 auStringId, u64 auScreenId,
                                 PresentationEntry& arEntry) const
{
    // FLAG PC-platform leaf: ARTIST @0x82687990 indexes the two tables with no bound, so an
    // action >= 14 (the GUI's "Pitch".."SwitchedOn" labels) reads whatever bytes follow them.
    // CalculateDataOffsets asserts every row < 14, so no row can match such an action and the
    // console result is always "not found"; the x64 member layout after the tables differs,
    // so the out-of-range read is answered directly instead of being reproduced.
    if (aiAction >= 14)
        return false;

    Attrib::Gen::presentationactionlist::ResolvedAction lResolved;
    const bool lbFound = mActions.Resolve(static_cast<u32>(aiAction), mau8DataOffsets[aiAction],
                                          mau8DataEnds[aiAction], auStringId, auScreenId, lResolved);
    PresentationDiag("[FLAG PC witness] [presentation] resolve action=%d found=%d splice=%d\n",
                     aiAction, lbFound ? 1 : 0, lbFound ? lResolved.mu16Splice : -1);
    if (!lbFound)
        return false;
    arEntry.mu64StringId = lResolved.muStringId;
    arEntry.mu64ScreenId = lResolved.muScreenId;
    arEntry.mu32ContentSpec = lResolved.muContentSpec;
    arEntry.mu16Splice = lResolved.mu16Splice;
    arEntry.mu8ChokeGroup = lResolved.mu8ChokeGroup;
    arEntry.mu8Valid = lResolved.mu8Valid;
    arEntry.mu8Behaviour = lResolved.mu8Behaviour;
    arEntry.mu8MixerOutput = lResolved.mu8MixerOutput;
    return true;
}

// ---------------------------------------------------------------------------
// The spew tail all three arms of Play share, ARTIST 0x826F7D08..0x826F7D3C.
// ---------------------------------------------------------------------------
static void SpewPresentationEntry(CgsDev::StrStreamBase& arLog,
                                  const PresentationEffect::PresentationEntry& arEntry,
                                  const char* apcBehaviourLabel)
{
    arLog << " Slider:" << KAPC_PRESENTATION_OUTPUT_LABELS[arEntry.mu8MixerOutput]
          << " Choke:" << static_cast<s32>(arEntry.mu8ChokeGroup)
          << apcBehaviourLabel << static_cast<s32>(arEntry.mu8Behaviour) << "\n";
}

// ---------------------------------------------------------------------------
// PresentationEffect::Play  @ 0x826F78D8
//   Three arms on GetContent(), each a body of its own in the asm. The assert after
//   FindOrStealAVoice is not a guard. The stored entry and the timer are written AFTER
//   Create/Play (0x826F7AD0 / 0x826F7C70), and mu16Age is not written at all: ProcessUpdate
//   @0x826E7930 owns it, clearing it while the voice is dead.
// ---------------------------------------------------------------------------
void PresentationEffect::Play(const PresentationEntry& arEntry)
{
    CGS_ASSERT(arEntry.IsValid(), "lPresentationEntry.IsValid()");

    const ePresentationContent leContent = arEntry.GetContent();
    PresentationDiag("[FLAG PC witness] [presentation] play content=%d splice=%d mixer=%d\n",
                     leContent, arEntry.mu16Splice, arEntry.mu8MixerOutput);

    if (leContent == E_PRESENTATION_CONTENT_STREAM)
    {
        PlayStream(arEntry.mu32ContentSpec);               // 0x826F7960
        mu8StreamOutput = arEntry.mu8MixerOutput;          // 0x826F7968
        if (KB_SPEW_PRESENTATION_MESSAGES)
        {
            CgsDev::StrStreamBase& lrLog = BrnSound::Debug::DebugComponent::mLogCombined
                << "Presentation Stream: Playing Stream Spec:" << arEntry.mu32ContentSpec;
            SpewPresentationEntry(lrLog, arEntry, " Behaviour:");
        }
        return;
    }

    AgingVoice* lpVoice = FindOrStealAVoice(arEntry);      // 0x826F79E4 / 0x826F7B74
    if (!lpVoice)
        return;
    CGS_ASSERT(!lpVoice->mVoice.IsAlive(), "!lpVoice->mVoice.IsAlive()");

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    lParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex = 0;

    if (leContent == E_PRESENTATION_CONTENT_WAVE_FILE)
    {
        lParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::GenericRwacFactorySkName().GetValue());
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("StereoWavVoiceSpec"));
        lParams.mContentSpecName = arEntry.mu32ContentSpec;
        lParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
        lpVoice->mVoice.Create(lParams);                   // 0x826F7AA8
        lpVoice->mVoice.Play(0);                           // 0x826F7AB4
    }
    else
    {
        GlobalStateManager* lpGlobal = static_cast<GlobalStateManager*>(
            mpState->GetStateManager());
        lParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~"));
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("SplicerVoiceSpec"));
        lParams.mpContent = &lpGlobal->GetPresentationSpliceBank();
        lParams.mContentSpecName = 0;
        lParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~"));
        lpVoice->mVoice.Create(lParams);                   // 0x826F7C48
        lpVoice->mVoice.Play(arEntry.mu16Splice);          // 0x826F7C54
    }

    lpVoice->mDataEntry = arEntry;
    lpVoice->mfTimeSinceLastTick = 0.0f;
    SetMixerInputValue(arEntry.mu8MixerOutput, 0x7FFF);

    if (KB_SPEW_PRESENTATION_MESSAGES)
    {
        if (leContent == E_PRESENTATION_CONTENT_WAVE_FILE)
        {
            CgsDev::StrStreamBase& lrLog = BrnSound::Debug::DebugComponent::mLogCombined
                << "Presentation WAV: Playing WAV Spec:" << arEntry.mu32ContentSpec;
            SpewPresentationEntry(lrLog, arEntry, " Behaviour:");
        }
        else
        {
            CgsDev::StrStreamBase& lrLog = BrnSound::Debug::DebugComponent::mLogCombined
                << "Presentation: Playing Splice idx:" << static_cast<s32>(arEntry.mu16Splice);
            SpewPresentationEntry(lrLog, arEntry, " behavior:");
        }
    }
}

// ---------------------------------------------------------------------------
// PresentationEffect::PlayStream  @ 0x8269E418  (no export file; decoded from the image)
//   Fills mStreamParams, then posts a stop for this user FIRST and the play request second:
//     0x8269E4FC bl 0x826835D8  StreamStopRequest { this+0x38, fade 0.25f (.rdata 0x82003F40) }
//     0x8269E508 bl 0x826834F0  StreamRequest { this+0x38, priority 4, lag 0.1f (.rdata 0x820ABB14) }
//   Priority 4 is the console's (`li r7, 4` at 0x8269E490): below music (5) and speech (6), so
//   GetFreeState @0x826B0EB8 evicts this stream for them and never evicts them for it. DecFIGS
//   @0x845CDC has 3, the merge-window delta; ARTIST decides.
//   The assert at 0x8269E43C is not a guard. mpOnPostInit, mpContent and the reverb fields are
//   not written: the ctor's Clear left them zero.
//   "MusicVoiceSpec" (.rdata 0x820AF470), not the WAV spec: a stream entry built on
//   "StereoWavVoiceSpec" handed its hybrid sample to the resident-only SnP1 player, which
//   faults opening the stream (StreamPool null) -- the race-start crash on RACESTARTMUSICLOOP.SNS.
// ---------------------------------------------------------------------------
void PresentationEffect::PlayStream(u32 auContentSpec)
{
    static const f32 KF_STREAM_STOP_FADE_OUT = 0.25f;     // .rdata 0x82003F40
    static const u32 KU_STREAM_PRIORITY = 4;              // 0x8269E490
    static const f32 KF_STREAM_LAG_TOLERANCE = 0.1f;      // .rdata 0x820ABB14

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    Streaming::StreamingStateManager* lpStreamingStateMan =
        static_cast<Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");

    mStreamParams.mpLogicModule = lpModule;                                     // +0x25C
    mStreamParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());             // +0x264
    mStreamParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("MusicVoiceSpec"));                  // +0x268
    mStreamParams.mContentSpecName = auContentSpec;                             // +0x270
    mStreamParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());       // +0x274
    mStreamParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));                          // +0x278
    mStreamParams.mSubMixVoiceID = 1;                                           // +0x27C
    mStreamParams.miSendIndex = 0;                                              // +0x288

    PresentationDiag("[FLAG PC witness] [presentation] playstream spec=%d priority=%d stop=%d\n",
                     static_cast<s32>(auContentSpec), KU_STREAM_PRIORITY, 1);
    lpStreamingStateMan->PostStreamRequest(
        Streaming::StreamStopRequest(this, KF_STREAM_STOP_FADE_OUT));
    lpStreamingStateMan->PostStreamRequest(
        Streaming::StreamRequest(this, KU_STREAM_PRIORITY, KF_STREAM_LAG_TOLERANCE));
}

void PresentationEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    if (!apMessage || apMessage->GetEventId() != 6)
        return;
    const CgsSound::Io::Message<BrnGui::GuiAudioTriggerWirePayload457>* lpMessage =
        static_cast<const CgsSound::Io::Message<BrnGui::GuiAudioTriggerWirePayload457>*>(apMessage);
    const BrnGui::GuiAudioTriggerWirePayload457& lrEvent = lpMessage->mData;
    if (KB_SPEW_PRESENTATION_MESSAGES)
    {
        const char* lpcAction = KAPC_PRESENTATION_LABELS[lrEvent.meAction];
        BrnSound::Debug::DebugComponent::mLogCombined << "[" << mfRunningTime << "] "
            << "PresentationEffect : Received audio trigger: Component " << lrEvent.macComponent
            << " Action " << (lpcAction ? lpcAction : "<NULLSTRING>")
            << " Movie " << lrEvent.macMovie << " Label " << lrEvent.macLabel << "\n";
    }
    const char* lpString = std::strcmp(lrEvent.macLabel, "uninitialised") == 0
        ? lrEvent.macComponent : lrEvent.macLabel;
    const u64 luStringId = static_cast<u32>(CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lpString)));
    const u64 luScreenId = static_cast<u32>(CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lrEvent.macMovie)));
    PresentationEntry lEntry = {};
    Resolve(lrEvent.meAction, luStringId, luScreenId, lEntry);
    // ARTIST 0x8269E404 / 0x8269E408: the event's own hashes, found or not, because a matched
    // row may be a wildcard with a zero id. HandleTick compares against these.
    lEntry.mu64StringId = luStringId;
    lEntry.mu64ScreenId = luScreenId;

    // 0x826FFA4C: PresentationAction 6, "OnTick" in the label table.
    if (lrEvent.meAction == 6)
        HandleTick(lEntry);
    if (lEntry.IsValid())
        Play(lEntry);
}

// ---------------------------------------------------------------------------
// PresentationEffect::HandleTick  DWARF cpp:774, inlined in Notify at 0x826FFA58..0x826FFA98.
//   A tick keeps a SustainedByTick voice alive: every slot holding the same string and screen
//   has its timer cleared, whether or not the voice is live. UpdateParams releases past 1.25f.
// ---------------------------------------------------------------------------
void PresentationEffect::HandleTick(const PresentationEntry& arEntry)
{
    s32 liMatched = 0;
    for (s32 liVoice = 0; liVoice < 4; ++liVoice)
    {
        AgingVoice& lrVoice = maVoices[liVoice];
        if (lrVoice.mDataEntry.mu64StringId == arEntry.mu64StringId &&
            lrVoice.mDataEntry.mu64ScreenId == arEntry.mu64ScreenId)
        {
            lrVoice.mfTimeSinceLastTick = 0.0f;
            ++liMatched;
        }
    }
    PresentationDiag("[FLAG PC witness] [presentation] tick matched=%d valid=%d splice=%d\n",
                     liMatched, arEntry.mu8Valid, arEntry.mu16Splice);
}

// ---------------------------------------------------------------------------
// PresentationEffect::GetMode  DWARF cpp:732, inlined at 0x826D2A4C..0x826D2A70.
//   lwz 8 == current mode type (cmpwi 0xF), lwz 0xC == current mode state (cmpwi 6 / 1).
// ---------------------------------------------------------------------------
PresentationEffect::eMode PresentationEffect::GetMode(
    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface* apGameModeInterface) const
{
    using namespace BrnGameState::GameStateModuleIO;
    if (apGameModeInterface->miCurrentGameModeType == E_MODE_ONLINE_FREE_BURN_LOBBY &&
        (apGameModeInterface->miCurrentGameModeState == E_GMS_ONLINE_LOADING ||
         apGameModeInterface->miCurrentGameModeState == E_GMS_INTRO))
    {
        return E_MODE_ONLINE_ENTRANCE;
    }
    return E_MODE_NONE;
}

// [FLAG PC witness] NOT IN THE X360 BINARY. One line per voice state change, for BRN_AEMS_DIAG.
static void PresentationVoiceDiag(s32 aiVoice, s32 aiState, s32 aiAge)
{
    static s32 saiStates[4] = {};
    if (saiStates[aiVoice] == aiState)
        return;
    saiStates[aiVoice] = aiState;
    PresentationDiag("[FLAG PC witness] [presentation] voice=%d state=%d age=%d\n",
                     aiVoice, aiState, aiAge);
}

// ---------------------------------------------------------------------------
// PresentationEffect::UpdateParams  @ 0x826D2938
//   Ticks each live voice's timer; a SustainedByTick voice is released past 1.25f
//   (.rdata 0x820AA55C; 0x826D29A4 `ble` skips, so exactly 1.25f keeps playing). mu16Age is
//   not touched here (0x826D295C..0x826D29BC): ProcessUpdate is its only incrementer.
//   Then the online-entrance sting: 0x826D2A74..0x826D2A80 is mMode.Update(GetMode(..)), and
//   0x826D2A88..0x826D2A98 is HasChangedTo(E_MODE_ONLINE_ENTRANCE). None of the asserts is a
//   guard; GetBrnInputStructure carries the mpBrnLogicInputBuffer one (BrnSoundLogicModule.h:432)
//   and GetGameModeInterface is RootInputB @0x826945F8.
// ---------------------------------------------------------------------------
void PresentationEffect::UpdateParams(f32 afDeltaTime)
{
    for (s32 liVoice = 0; liVoice < 4; ++liVoice)
    {
        AgingVoice& lrVoice = maVoices[liVoice];
        PresentationVoiceDiag(liVoice, lrVoice.mVoice.GetState(), lrVoice.mu16Age);
        if (!lrVoice.mVoice.IsAlive())
            continue;
        lrVoice.mfTimeSinceLastTick += afDeltaTime;
        if (lrVoice.mDataEntry.mu8Behaviour == KU8_BEHAVIOUR_SUSTAINED_BY_TICK &&
            lrVoice.mfTimeSinceLastTick > 1.25f)
            lrVoice.mVoice.Release();
    }

    BrnSound::Module::SoundLogicModule* lpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpLogicModule != 0, "lpLogicModule");
    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface* lpGameModeInterface =
        lpLogicModule->GetBrnInputStructure()->GetGameModeInterface();
    CGS_ASSERT(lpGameModeInterface != 0, "lpGameModeInterface");

    mMode.Update(GetMode(lpGameModeInterface));
    if (mMode.HasChangedTo(E_MODE_ONLINE_ENTRANCE))
    {
        PlayStream(static_cast<u32>(CgsSound::Playback::Name::MakeHash("OnlineEntry")));
        mu8StreamOutput = 9;                               // 0x826D2AC8
    }
}

void PresentationEffect::ProcessUpdate()
{
    const u32 luSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luRwacPitchName = static_cast<u32>(CgsSound::Playback::Name::MakeHash(
        "~GenericRwacPlayerVoice::SK_PLAYER_PARAMETER_PITCH~"));
    const u32 luSplicerPitchName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));

    for (s32 liVoice = 0; liVoice < 4; ++liVoice)
    {
        AgingVoice& lrVoice = maVoices[liVoice];
        lrVoice.mVoice.Update();

        const s32 liState = lrVoice.mVoice.GetState();
        if (liState == 7 || liState == 0)
        {
            lrVoice.mu16Age = 0;
            continue;
        }

        CGS_ASSERT(lrVoice.mDataEntry.mu8MixerOutput <= 9,
                   "lrVoice.mDataEntry.mu8MixerOutput <= 9");
        const f32 lfGain = GetMixerOutputValue(
            lrVoice.mDataEntry.mu8MixerOutput, Nicotine::DMixIO::DMX_VOL) / 32767.0f;
        ++lrVoice.mu16Age;
        lrVoice.mVoice.SetGain(0, lfGain, &luSendName);

        if (lrVoice.mDataEntry.mu16Splice == PresentationEntry::KU16_SPECIAL_SPLICE_WAVE)
            lrVoice.mVoice.SetParameter(0, 1.0f, &luRwacPitchName);
        else if (lrVoice.mDataEntry.mu16Splice != PresentationEntry::KU16_SPECIAL_SPLICE_STREAM)
            lrVoice.mVoice.SetParameter(0, 1.0f, &luSplicerPitchName);
    }

    for (s32 liInput = 0; liInput < 10; ++liInput)
        SetMixerInputValue(liInput, 0);
}

// ---------------------------------------------------------------------------
// PresentationEffect::FindFreeVoice()  @ 0x82687D68
//   Return the first FREE maVoices[] slot, else null. A slot's per-VoiceWrapper
//   state word (console slot+0x4C == the wrapper's +0x48 miState) reads 0 (unused)
//   or 7 (idle) when available.
//   asm: v1=0; for(cur=this+0x8C;;cur+=0x80){ if(*cur==7||*cur==0) return
//        this+0x40+(v1<<7); if(++v1>=4) return 0; }
//   (2026-08-25 wave 4: the raw this+0x8C/stride-0x80 cursor is retired -- the walk
//   subscripts maVoices[] and reads the state via VoiceWrapper::GetState by name.)
// ---------------------------------------------------------------------------
PresentationEffect::AgingVoice* PresentationEffect::FindFreeVoice()
{
    for (s32 liSlot = 0; liSlot < 4; ++liSlot)            // r10
    {
        const s32 liState = maVoices[liSlot].mVoice.GetState();   // console slot+0x4C
        if (liState == 7 || liState == 0)
            return &maVoices[liSlot];
    }
    return 0;
}

// ---------------------------------------------------------------------------
// PresentationEffect::FindOrStealAVoice(rEntry)  @ 0x826D2AD8
//   Pick the AgingVoice slot to (re)use for the new presentation entry rEntry:
//     1. If a busy STREAM request (query mu8Behaviour == 2) content-matches the
//        stored entry field-for-field, the exact stream is already playing ->
//        return 0 (do NOT allocate a voice). [asm: match -> break -> result = 0]
//     2. Track a steal candidate: nonzero choke-group -> if it equals the query
//        choke-group Release() its voice + prefer; if it is LOWER, prefer;
//        choke-group 0 -> prefer the OLDEST (max mu16Age).
//     3. After the 4-slot walk: prefer FindFreeVoice()'s free slot; else the
//        tracked steal candidate (Release its voice first); else 0.
//
//   X360 walks cur=this+0x8C (maVoices[0] slot+0x4C) in 0x80 strides. The stored
//   PresentationEntry sits at slot+0x58 (AgingVoice::mDataEntry); its compared
//   fields mirror the query entry 1:1 (+0x10 mContentSpec leading word, +0x14
//   mu16Splice, +0x16 mu8ChokeGroup, +0x17 mu8Valid, +0x18 mu8Behaviour, +0x19
//   mu8MixerOutput). mu16Age is slot+0x00; the wrapper (Release target) slot+0x04.
//
//   (2026-08-25 wave 4: the raw u8* cursor walk is retired -- AgingVoice now
//   materialises mDataEntry, so both sides of every compare are named members; the
//   console byte offsets stay in the comments as the asm anchors.)
// ---------------------------------------------------------------------------
PresentationEffect::AgingVoice* PresentationEffect::FindOrStealAVoice(const PresentationEntry& rEntry)
{
    AgingVoice* lpFree = FindFreeVoice();                 // r25

    s32 liCandidate = -1;                                 // r28: best steal-slot index
    u32 luBestAge   = 0;                                  // r27: oldest mu16Age (choke 0)

    for (u32 luSlot = 0; luSlot < 4; ++luSlot)            // r29 / r31 cursor
    {
        AgingVoice&              lrSlot   = maVoices[luSlot];
        const PresentationEntry& lrStored = lrSlot.mDataEntry;          // slot+0x58

        const s32  liState = lrSlot.mVoice.GetState();    // slot+0x4C
        const bool lbBusy  = !(liState == 7 || liState == 0);

        // A busy STREAM request whose stored entry content-matches rEntry means the
        // exact stream is already playing -> the X360 breaks the walk and returns 0.
        if (lbBusy && rEntry.mu8Behaviour == 2)           // E_PRESENTATION_CONTENT_STREAM
        {
            const bool lbMatch =
                   rEntry.mu32ContentSpec == lrStored.mu32ContentSpec   // slot+0x68
                && rEntry.mu16Splice      == lrStored.mu16Splice        // slot+0x6C
                && rEntry.mu8ChokeGroup   == lrStored.mu8ChokeGroup     // slot+0x6E
                && rEntry.mu8Valid        == lrStored.mu8Valid          // slot+0x6F
                && lrStored.mu8Behaviour  == 2                          // slot+0x70
                && rEntry.mu8MixerOutput  == lrStored.mu8MixerOutput;   // slot+0x71
            if (lbMatch)
                return 0;                                 // asm: break -> result = 0
        }

        // Steal-candidate selection, keyed on the STORED entry's choke group.
        if (lrStored.mu8ChokeGroup != 0)
        {
            if (lrStored.mu8ChokeGroup == rEntry.mu8ChokeGroup)
            {
                lrSlot.mVoice.Release();                  // maVoices[i].mVoice (slot+0x04)
                liCandidate = static_cast<s32>(luSlot);
            }
            else if (lrStored.mu8ChokeGroup < rEntry.mu8ChokeGroup)
            {
                liCandidate = static_cast<s32>(luSlot);
            }
        }
        else
        {
            if (lrSlot.mu16Age > luBestAge)               // mu16Age (slot+0x00)
            {
                luBestAge   = lrSlot.mu16Age;
                liCandidate = static_cast<s32>(luSlot);
            }
        }
    }

    if (lpFree != 0)
        return lpFree;
    if (liCandidate >= 0)
    {
        CGS_ASSERT(liCandidate < 4, "liCandidate < E_POLYPHONY");
        AgingVoice& lrSteal = maVoices[liCandidate];
        lrSteal.mVoice.Release();
        return &lrSteal;
    }
    return 0;
}


// ---------------------------------------------------------------------------
// PresentationEffect::Detach  @ 0x826F77D0   (vtable 0x820B5F10 slot +0x20)
//
// BASE POINTER: this == primary + 4 (the IResourceRequester sub-object). Three
// independent tells:
//   * slot 0 of the table this body lives in (0x820B5F10) is
//     PresentationEffect::`vector deleting destructor'`adjustor{4}' @0x826E7728;
//   * 0x826F788C `addic. r11, r27, -4` -- the body itself materialises the primary;
//   * 0x826F785C `lwz r11, 0x28(r27)` == primary +0x2C == mpLogicModule, exactly the
//     committed BrnEffectObject model.
//
//   826F77E0  bl 0x826EBF88             ; BrnEffectObject::Detach, r3 passed through
//   826F77E4  clrlwi/bne ; 826F77F0 li r3,0  -> return false
//   826F77FC  addi r31, r27, 0x88       ; cursor = &maVoices[0].mVoice.meUpdateStage
//   826F7800  li   r30, 4               ; four iterations
//   826F7850  addi r31, r31, 0x80       ; stride 0x80 == sizeof(AgingVoice)
//   the loop body is CgsSound::Logic::VoiceWrapper::Stop() @0x826DC570 INLINED:
//       lwz 0(cursor)      meUpdateStage      (VoiceWrapper +0x48)
//       stb 0, 4(cursor)   mbPlay = false     (VoiceWrapper +0x4C)
//       if (stage == 6 && mVoiceObject.mVoiceHandle != 0)
//            Voice::Stop() @0x826C5148 ; meUpdateStage = 7
//       else if (stage < 6)
//            VoiceWrapper::Release() @0x826C5270
//   826F785C  lwz r11, 0x28(r27) / lwz r31, 0x296C(r11)
//   826F786C  assert "lpStreamingStateMan"   (BrnPresentationEffect.cpp:234)
//   826F788C  addic./addi/bne/mr        ; the null-preserving (IStreamUser*)this adjust
//   826F78B0  lfs f0, [0x82003F40] = 0.25f   ; the stop request's fade-out
//   826F78C4  bl 0x826835D8             ; StreamingStateManager::PostStreamRequest
//   826F78C8  li r3, 1
//
// module+0x296C is GetEnvironment().GetStateManager(6) -- the same expression this TU's
// Play() and SpeechEffect::Attach already write. (CreateStateManagers @0x826AFF18 does
// `addi r26, r29, 0x2950` before Environment::AddStateManager, so the Environment
// sub-object is module+0x2950, and the committed CgsEnvironment.h puts
// mapStateManagers[0] at Environment+0x04: 0x2950 + 0x1C == 0x296C.)
//
// ⚠️ NOTE THE ASSERT IS NOT A GUARD. The console asserts the manager and then posts
// UNCONDITIONALLY. The sibling SpeechEffect::Detach in this tree replaces that with a
// silent `if (mpStreamingManager)`, which is an invented arm -- deliberately not
// copied here.
//
// [FLAG] KF_STREAM_STOP_FADE_OUT: the NAME is ours. .rdata 0x82003F40 == 0.25f carries no
// symbol and the DWARF has no constant for it; the value and the address are the console's.
// [FLAG] The state-manager slot is written as the literal 6, matching this TU's existing
// Play()/Attach() sites; no enumerator name for the streaming slot was recovered.
// ---------------------------------------------------------------------------
bool PresentationEffect::Detach()
{
    static const f32 KF_STREAM_STOP_FADE_OUT = 0.25f;   // .rdata 0x82003F40

    // 826F77E0 / 826F77EC / 826F77F0
    if (!BrnEffectObject::Detach())
        return false;

    // 826F77FC..826F7858 -- VoiceWrapper::Stop() @0x826DC570 inlined over maVoices[4].
    for (s32 liVoice = 0; liVoice < 4; ++liVoice)
        maVoices[liVoice].mVoice.Stop();

    // 826F785C / 826F7860
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    Streaming::StreamingStateManager* lpStreamingStateMan =
        static_cast<Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6));
    // 826F786C..826F7888 -- the console's own expression string; NO guard follows it.
    CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");

    // 826F788C..826F78C4
    lpStreamingStateMan->PostStreamRequest(
        Streaming::StreamStopRequest(this, KF_STREAM_STOP_FADE_OUT));

    return true;                                        // 826F78C8  li r3, 1
}

} // namespace Logic
} // namespace BrnSound
