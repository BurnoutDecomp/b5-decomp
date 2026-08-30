#include "GameSource/Sound/Global/BrnPresentationEffect.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
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
    mu8StreamOutput = 9;

    mStreamParams.Clear();
    mStreamParams.mpLogicModule = lpModule;
    mStreamParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    mStreamParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("StereoWavVoiceSpec"));
    mStreamParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Player"));
    mStreamParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    mStreamParams.mSubMixVoiceID = 1;
    mStreamParams.miSendIndex = 0;
    return true;
}

const CgsSound::Logic::VoiceWrapper::CreateParams&
PresentationEffect::GetCreateParams() const
{
    return mStreamParams;
}

void PresentationEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                           f32 afGain, f32)
{
    const u32 luSend = mStreamParams.mSendName;
    arVoice.SetGain(static_cast<u32>(mStreamParams.miSendIndex), afGain, &luSend);
}

void PresentationEffect::StreamStopped()
{
    mu8StreamOutput = 9;
}

bool PresentationEffect::Resolve(s32 aiAction, u64 auStringId, u64 auScreenId,
                                 PresentationEntry& arEntry) const
{
    Attrib::Gen::presentationactionlist::ResolvedAction lResolved;
    if (!mActions.Resolve(static_cast<u32>(aiAction), auStringId, auScreenId, lResolved))
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

void PresentationEffect::Play(const PresentationEntry& arEntry)
{
    if (arEntry.mu16Splice == PresentationEntry::KU16_SPECIAL_SPLICE_STREAM)
    {
        BrnSound::Module::SoundLogicModule* lpModule =
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        Streaming::StreamingStateManager* lpStreaming =
            static_cast<Streaming::StreamingStateManager*>(
                lpModule->GetEnvironment().GetStateManager(6));
        CGS_ASSERT(lpStreaming != 0, "lpStreamingStateManager");
        if (lpStreaming)
        {
            mStreamParams.mContentSpecName = arEntry.mu32ContentSpec;
            mu8StreamOutput = arEntry.mu8MixerOutput;
            lpStreaming->PostStreamRequest(Streaming::StreamRequest(this, 6, 0.1f));
        }
        return;
    }

    AgingVoice* lpVoice = FindOrStealAVoice(arEntry);
    if (!lpVoice)
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = lpModule;
    lParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex = 0;

    if (arEntry.mu16Splice == PresentationEntry::KU16_SPECIAL_SPLICE_WAVE)
    {
        lParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::GenericRwacFactorySkName().GetValue());
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("StereoWavVoiceSpec"));
        lParams.mContentSpecName = arEntry.mu32ContentSpec;
        lParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("Player"));
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
    }

    lpVoice->mDataEntry = arEntry;
    lpVoice->mu16Age = 0;
    lpVoice->mfTimeSinceLastTick = 0.0f;
    lpVoice->mVoice.Create(lParams);
    lpVoice->mVoice.Play(arEntry.mu16Splice);
    SetMixerInputValue(arEntry.mu8MixerOutput, 0x7FFF);
}

void PresentationEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    if (!apMessage || apMessage->GetEventId() != 6)
        return;
    const CgsSound::Io::Message<BrnGui::GuiAudioTriggerEvent>* lpMessage =
        static_cast<const CgsSound::Io::Message<BrnGui::GuiAudioTriggerEvent>*>(apMessage);
    const BrnGui::GuiAudioTriggerEvent& lrEvent = lpMessage->mData;
    const char* lpString = std::strcmp(lrEvent.macLabel, "uninitialised") == 0
        ? lrEvent.macComponent : lrEvent.macLabel;
    const u64 luStringId = static_cast<u32>(CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lpString)));
    const u64 luScreenId = static_cast<u32>(CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lrEvent.macMovie)));
    PresentationEntry lEntry = {};
    const bool lbResolved = Resolve(lrEvent.meAction, luStringId, luScreenId, lEntry);
    if (lbResolved && lEntry.mu8Valid)
        Play(lEntry);
}

void PresentationEffect::UpdateParams(f32 afDeltaTime)
{
    for (s32 liVoice = 0; liVoice < 4; ++liVoice)
    {
        AgingVoice& lrVoice = maVoices[liVoice];
        if (!lrVoice.mVoice.IsAlive())
            continue;
        ++lrVoice.mu16Age;
        lrVoice.mfTimeSinceLastTick += afDeltaTime;
        if (lrVoice.mDataEntry.mu8Behaviour == 1 &&
            lrVoice.mfTimeSinceLastTick >= 1.25f)
            lrVoice.mVoice.Release();
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

} // namespace Logic
} // namespace BrnSound
