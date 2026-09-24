#include "GameSource/Sound/Streaming/BrnStreamingEffect.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"  // GetGlobalData().StreamSettings()
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lpState / IsAttached tripwires)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (FindStreamSettings' own miss print)
#include "GameShared/GameClasses/Sound/CgsStreamDiag.h"  // [DIAG] NOT IN THE X360 BINARY
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"  // CgsSound::Utils::Curve (Detach's equal-power fade)
#include "rw/math/fpu/scalar_operation.h"                // rw::math::fpu::Clamp (Detach's fsel clamp)
#include <algorithm>

// =============================================================================
// BrnSound::Logic::Streaming::StreamingEffect -- out-of-line bodies for the
// functions owned by this TU (ledger id class:BrnSound::Logic::Streaming):
//   StreamingEffect::StreamingEffect  @ 0x826C9D10 (ctor)
//   StreamingEffect::GetRequest       @ 0x82683B40
//   StreamingEffect::~StreamingEffect (anchors the vector deleting dtor @ 0x826E24B8)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnStreamingEffect.h / BrnStreamingState.h for the layout and the
// dual-base / minimal-flagged-home notes.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// ---------------------------------------------------------------------------
// StreamingEffect::StreamingEffect  @ 0x826C9D10
//
//   stfs 0.0f, 0x20(r31) / 0x1C(r31)           ; base leaf f32 = 0.0f (un-homed)
//   stw  off_820AE954, 4(r31)                   ; (transient) IResourceRequester base vptr
//   sth  0, 0x10(r31) / 0x12(r31)               ; base leaf s16 = 0 (un-homed)
//   stw  0, 0xC(r31)                            ; mpState = nullptr
//   stw  0, 0x34/0x08/0x28/0x24(r31); stb 0, 0x30(r31) ; base leaf fields = 0 (un-homed)
//   stw  off_820B38A0, 0(r31)                   ; primary leaf vptr (this+0)   -- final
//   stw  off_820B386C, 4(r31)                   ; IResourceRequester vptr (this+4) -- final
//   stw  0, 0x38(r31) .. 0x60(r31)              ; leaf word run = 0 (un-homed, 11 words)
//   stw  -1, 0x64(r31)                          ; leaf word = -1 (un-homed, sentinel)
//   bl   CgsSound::Logic::VoiceWrapper::VoiceWrapper(r31+0x68)        ; mVoiceWrapper ctor
//   bl   Attrib::Gen::streamsettings::streamsettings(r31+0xB8, 0, 0)  ; maStreamSettings ctor
//   return this
//
// This is MSVC's INLINED full-object constructor (same shape as the committed
// ExplosionEffect::ExplosionEffect @ 0x826D5480 sibling): it does NOT `bl` a base
// constructor -- it inlines the BrnEffectObject dual-base member zero-init and
// installs the two leaf vptrs directly (off_820B38A0 @ this+0, off_820B386C @
// this+4), then constructs the embedded VoiceWrapper at +0x68 and the embedded
// generated streamsettings block at +0xB8. In reconstructed C++ the two vptr
// installs and the base member zero-init are produced implicitly by the
// BrnEffectObject base sub-object's own default constructor (reused BY NAME);
// mpState's explicit nullptr-init is reproduced by name (the +0x0C member
// GetRequest() asserts non-null); the tail effects are the embedded VoiceWrapper
// sub-object construction and the embedded streamsettings sub-object construction.
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits (the
// two f32 @ +0x1C/+0x20, the two s16 @ +0x10/+0x12, the word/byte fields @
// +0x08/+0x24/+0x28/+0x30/+0x34, and the word run @ +0x38..+0x60 plus the -1
// sentinel @ +0x64) target un-homed leaf members between mpState and mVoiceWrapper;
// their names/types are un-homed (no DWARF, no Feb-2007 source for this TU). Per the
// project anti-fabrication rule they are NOT invented as named fields and NOT
// raw-offset-hacked: only the base construction, mpState's nullptr-init, the
// VoiceWrapper construction and the streamsettings construction are bodied.
//
// The tail `Attrib::Gen::streamsettings::streamsettings(this+0xB8, 0, 0)` is the
// AttribSys-generated ctor (@0x82697400, homed in Generated/classes/streamsettings.h)
// constructing the instance with NO collection: the console binds it later, on every
// Attach @0x826EE8D0, from BurnoutGlobalData's `StreamSettings` RefSpec (see Attach).
// ---------------------------------------------------------------------------
StreamingEffect::StreamingEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mCreateParams()
    , mVoice()
    , mStreamSettings()
    , mfElapsedTime(0.0f)
    , mfTimeThroughFade(0.0f)
    , mfGain(0.0f)
    , mfGainPreFade(0.0f)
    , mVoiceId(static_cast<CgsSound::Logic::Command::QueueElement>(-1))
    , mbBufferReleased(false)
    , meDiagLastStage(CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE)  // [DIAG]
{
}

// ---------------------------------------------------------------------------
// StreamingEffect::GetRequest  @ 0x82683B40
//
//   lwz   state, 0xC(this)                 ; mpState
//   if (!state) assert("lpState", BrnStreamingEffect.cpp)   ; non-gating
//   lbz   r11, 0x48(state)                 ; state->mbAttached (IsAttached())
//   if (!r11) assert("IsAttached()", BrnStreamingState.h:168)   ; non-gating
//   addi  r3, state, 0x54                  ; return &state->mRequest
//   blr
//
// Returns a reference to the owned StreamingState's embedded Request (at state
// +0x54), after asserting the state exists and is attached. Both asserts are the
// CGS_ASSERT-vacuous tripwires (non-gating). Members reached BY NAME.
// ---------------------------------------------------------------------------
const StreamRequest& StreamingEffect::GetRequest() const
{
    CGS_ASSERT( mpState != nullptr, "lpState" );
    const StreamingState* lpState = static_cast<const StreamingState*>(mpState);
    CGS_ASSERT( lpState->IsAttached(), "IsAttached()" );
    return lpState->GetRequest();
}

// ---------------------------------------------------------------------------
// StreamingEffect::~StreamingEffect  (anchors the X360 `vector deleting destructor'
// @ 0x826E24B8)
//
//   bl   StreamingEffect::~StreamingEffect          ; chain to the real (out-of-line) dtor
//   if (a2 & 1) { <sound allocator>.Free(this) }     ; the `delete' half (off_82FFB954 slot +0x14)
//   return this
//
// Identical in shape to the committed sibling ExplosionEffect/CollisionEffect
// `vector deleting destructor'. The inner dtor adds no teardown of its own here (the
// dual-base settle + embedded VoiceWrapper/streamsettings destruction is
// compiler-synthesised from the virtual dtor declared in the header). The (a2 & 1)
// tail frees the object through the global sound MemBase allocator (off_82FFB954);
// that dispatch is host codegen and is DEFERRED rather than reproduced. Only the
// empty out-of-line virtual destructor body is authored, anchoring vtable/thunk
// emission to this TU.
// ---------------------------------------------------------------------------
StreamingEffect::~StreamingEffect()
{
}

CgsSound::Logic::EffectObject* StreamingEffect::CreateObject(u32 /*auType*/)
{
    return new StreamingEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* StreamingEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x60000, "StreamingEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &StreamingEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* StreamingEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* StreamingEffect::GetTypeName() const
{
    return "StreamingEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpStreamingEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(StreamingEffect::GetStaticTypeInfo());

bool StreamingEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    const StreamRequest& lrRequest = GetRequest();
    CGS_ASSERT(lrRequest.mpAttachment != 0, "GetRequest().mpAttachment");
    if (!lrRequest.mpAttachment)
        return false;

    mCreateParams = lrRequest.mpAttachment->GetCreateParams();
    mVoice.Create(mCreateParams);
    mVoice.Play(0);

    // @0x826EE97C..0x826EEA30 (export hole, read from the image): the console binds
    // mStreamSettings HERE, on every attach, to the BurnoutGlobalData `StreamSettings`
    // RefSpec (layout +0x440, reached through mpLogicModule+0x1350C ==
    // SoundLogicModule::mBurnoutGlobalData), asserts the two arrays are the same
    // length, then looks this stream's ContentSpec up for its authored volume:
    //     lwz  r11, 0x28(r31) ; lwzx r11, r11, 0x1350C ; addi r4, r11, 0x440
    //     bl   Attrib::Instance::ChangeWithDefault(&mStreamSettings, r4)
    //     Get/GetLength(ContentSpecs) == Get/GetLength(Volumes)   ; assert :0x87
    //     bl   FindStreamSettings(&mStreamSettings, mCreateParams.mContentSpecName)
    //     stfs f1, 0xCC(r31)                                      ; mfGain
    // There is no "collection present?" guard on the console: an unresolved RefSpec
    // leaves the instance empty, both lengths read 0 and FindStreamSettings returns
    // its not-found 1.0f (flt_82001C98) -- the same value the old invented else-arm
    // produced, which is why every stream in the build sat at gain 1.0.
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    mStreamSettings.ChangeWithDefault(lpModule->GetGlobalData().StreamSettings());
    CGS_ASSERT(mStreamSettings.Num_ContentSpecs() == mStreamSettings.Num_Volumes(),
               "mStreamSettings.Num_ContentSpecs() == mStreamSettings.Num_Volumes()");
    mfGain = FindStreamSettings(mStreamSettings, mCreateParams.mContentSpecName);

    mVoiceId = static_cast<CgsSound::Logic::Command::QueueElement>(mVoice.GetVoice().GetIdent());
    mfElapsedTime = 0.0f;
    mfTimeThroughFade = 0.0f;
    mfGainPreFade = 0.0f;
    mbBufferReleased = false;

    // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). The voice bring-up, in the
    // order the arithmetic runs: the ContentSpec asked for, whether a Playback::Content
    // object exists for it, the voice object + ident, and the streamsettings gain this
    // effect will multiply the user's volume by.
    CgsSound::Diag::StreamDiagPrintf(
        "[sndstream] attach spec=0x%08X voicespec=0x%08X voiceobj=%d ident=%u "
        "settings=%d n=%u gain=%.4f\n",
        static_cast<u32>(mCreateParams.mContentSpecName),
        static_cast<u32>(mCreateParams.mVoiceSpecName),
        mVoice.HasLiveVoice() ? 1 : 0,
        static_cast<u32>(mVoiceId),
        mStreamSettings.GetCollection() ? 1 : 0,
        mStreamSettings.Num_ContentSpecs(), mfGain);

    // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). The settings TABLE as the
    // binary search sees it: is ContentSpecs ascending (the search's precondition),
    // and does a LINEAR scan find this spec where the search did not? Printed once
    // per session for the shape, once per attach for the scan.
    if (CgsSound::Diag::StreamDiagEnabled())
    {
        const u32 luCount = mStreamSettings.Num_ContentSpecs();
        static bool sbShapePrinted = false;
        if (!sbShapePrinted && luCount)
        {
            sbShapePrinted = true;
            u32 luDescents = 0, luZeroSpecs = 0, luNotUnity = 0;
            f32 lfMin = mStreamSettings.Volumes(0), lfMax = lfMin;
            u32 luFirstNotUnity = 0;
            for (u32 i = 0; i < luCount; ++i)
            {
                if (i && mStreamSettings.ContentSpecs(i) < mStreamSettings.ContentSpecs(i - 1))
                    ++luDescents;
                if (mStreamSettings.ContentSpecs(i) == 0)
                    ++luZeroSpecs;
                const f32 lfVol = mStreamSettings.Volumes(i);
                if (lfVol != 1.0f)
                {
                    if (!luNotUnity)
                        luFirstNotUnity = i;
                    ++luNotUnity;
                }
                lfMin = lfVol < lfMin ? lfVol : lfMin;
                lfMax = lfVol > lfMax ? lfVol : lfMax;
            }
            CgsSound::Diag::StreamDiagPrintf(
                "[sndstream] settings-table n=%u volumes=%u descents=%u zeros=%u "
                "spec[0]=0x%08X vol[0]=%.4f spec[1]=0x%08X spec[n-1]=0x%08X vol[n-1]=%.4f "
                "notUnity=%u first=%u(0x%08X %.4f) min=%.4f max=%.4f\n",
                luCount, mStreamSettings.Num_Volumes(), luDescents, luZeroSpecs,
                mStreamSettings.ContentSpecs(0), mStreamSettings.Volumes(0),
                luCount > 1 ? mStreamSettings.ContentSpecs(1) : 0u,
                mStreamSettings.ContentSpecs(luCount - 1), mStreamSettings.Volumes(luCount - 1),
                luNotUnity, luFirstNotUnity, mStreamSettings.ContentSpecs(luFirstNotUnity),
                mStreamSettings.Volumes(luFirstNotUnity), lfMin, lfMax);
        }
        s32 liLinearHit = -1;
        for (u32 i = 0; i < luCount; ++i)
        {
            if (mStreamSettings.ContentSpecs(i) == static_cast<u32>(mCreateParams.mContentSpecName))
            {
                liLinearHit = static_cast<s32>(i);
                break;
            }
        }
        CgsSound::Diag::StreamDiagPrintf(
            "[sndstream] settings-scan spec=0x%08X linearHit=%d vol=%.4f\n",
            static_cast<u32>(mCreateParams.mContentSpecName), liLinearHit,
            liLinearHit >= 0 ? mStreamSettings.Volumes(static_cast<u32>(liLinearHit)) : -1.0f);
    }
    return true;
}

void StreamingEffect::UpdateParams(f32 af32DeltaTime)
{
    mfElapsedTime += af32DeltaTime;
    mfTimeThroughFade += af32DeltaTime;
}

void StreamingEffect::ProcessUpdate()
{
    const StreamRequest lRequest = GetRequest();
    CGS_ASSERT(lRequest.mpAttachment != 0, "GetRequest().mpAttachment");
    if (!lRequest.mpAttachment)
        return;

    mVoice.Update();
    const CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE leStage = mVoice.GetUpdateStage();

    // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). EDGE-TRIGGERED: one line per
    // VoiceWrapper stage transition for this effect. Names the stage number, so a
    // stream stuck at 4 (E_UPDATE_STAGE_WAIT, content never IsLoaded) is visible as a
    // stage that stops advancing, not as an absence of sound.
    if (leStage != meDiagLastStage)
    {
        meDiagLastStage = leStage;
        CgsSound::Diag::StreamDiagPrintf(
            "[sndstream] stage spec=0x%08X ident=%u stage=%d t=%.2f\n",
            static_cast<u32>(mCreateParams.mContentSpecName),
            static_cast<u32>(mVoiceId), static_cast<int>(leStage), mfElapsedTime);
    }

    if (leStage != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE &&
        leStage != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED)
    {
        lRequest.mpAttachment->UpdateVoiceParams(mVoice, mfGain, mfElapsedTime);
        return;
    }

    StreamingState* lpState = static_cast<StreamingState*>(mpState);
    if (lpState && lpState->Detach())
    {
        CgsSound::Diag::StreamDiagPrintf(
            "[sndstream] finished spec=0x%08X ident=%u after %.2f s -> StreamStopped\n",
            static_cast<u32>(mCreateParams.mContentSpecName),
            static_cast<u32>(mVoiceId), mfElapsedTime);
        lRequest.mpAttachment->StreamStopped();
    }
}

f32 StreamingEffect::GetFadeOut() const
{
    const StreamingState* lpState = static_cast<const StreamingState*>(mpState);
    CGS_ASSERT(lpState != 0, "lpState");
    CGS_ASSERT(lpState && lpState->GetUpdateState() == CgsSound::Logic::State::E_UPDATE_DETATCHING,
               "lpState->GetUpdateState() == E_UPDATE_DETATCHING");
    return lpState ? lpState->GetFadeOut() : 0.0f;
}

// ---------------------------------------------------------------------------
// StreamingEffect::Detach  @0x826EEA68 (switch on meDetachState, jpt_826EEAAC)
//   case 0 (0x826EEAC0..0x826EEAE4)  mfTimeThroughFade = 0.0 (flt_82001CC0); mfGainPreFade =
//                                    VoiceWrapper::GetGain @0x826C5218 of the "Send01" send --
//                                    dword_8300A6E0, MakeHash("Send01") by the CRT thunk
//                                    0x82C61A18..0x82C61A34 (string 0x820AF630) -- not the stream's
//                                    own send name
//   case 1 (0x826EEAE8..0x826EEBE8)  both clocks += dt; f = mfTimeThroughFade / GetFadeOut() (`fdivs`,
//                                    no guard on the length) clamped to [0, 1] by `fneg ; fsel` +
//                                    `fsubs ; fsel` (a NaN -> 1.0) = rw::math::fpu::Clamp; the send
//                                    gain = Curve::GetOutput(1 - f, E_ONE_MINUS_EQPWR) * mfGainPreFade --
//                                    GetOutput inlined at 0x826EEB38..0x826EEBC8 (its CgsSoundUtils.cpp:161
//                                    assert, `fmsubs x*511 - 511` flt_820AA7B0, `fctiwz`, 1 -
//                                    gafArraySinTable[..]): the equal-power fade, 1 - sin(f * pi/2),
//                                    not a linear 1 - f -- written through the wrapper's voice-present
//                                    test (0x826EEB70..0x826EEB7C) to the stream's own send (index,
//                                    name); then GetFadeOut() AGAIN (0x826EEBD4): still fading ->
//                                    false (`blt`, a NaN goes on), else release the voice
//   case 2 (0x826EEBEC..0x826EEBFC)  wait for the stream buffer (mbBufferReleased)
//   case 3 (0x826EEC00..0x826EEC1C)  BrnEffectObject::Detach
// ---------------------------------------------------------------------------
bool StreamingEffect::Detach()
{
    switch (meDetachState)
    {
    case E_DETACH_STATE_NONE:
    {
        mfTimeThroughFade = 0.0f;
        const s32 liSendName = static_cast<s32>(CgsSound::Playback::Name::MakeHash("Send01"));
        mfGainPreFade = mVoice.GetGain(&liSendName);
        meDetachState = E_DETACH_STATE_BEGIN;
        // fall through
    }
    case E_DETACH_STATE_BEGIN:
    {
        mfElapsedTime += mfDeltaTime;
        mfTimeThroughFade += mfDeltaTime;
        const f32 lfFadeOut = GetFadeOut();                                          // 0x826EEB14
        const f32 lfFraction = rw::math::fpu::Clamp(mfTimeThroughFade / lfFadeOut, 0.0f, 1.0f);
        const f32 lfGain = CgsSound::Utils::Curve::GetOutput(
                               1.0f - lfFraction, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR) *
                           mfGainPreFade;
        const u32 luSendName = mCreateParams.mSendName;
        mVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex), lfGain, &luSendName);

        // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1): each fading frame's position and the gain
        // written to the send, so a live run can re-derive the console's curve from f and the pre-fade
        // gain. Capped at 96 lines per run.
        static u32 su32FadeLines = 0;
        if (CgsSound::Diag::StreamDiagEnabled() && su32FadeLines < 96u)
        {
            ++su32FadeLines;
            CgsSound::Diag::StreamDiagPrintf(
                "[sndstream] fade spec=0x%08X t=%.6f fade=%.6f f=%.6f pre=%.6f gain=%.6f\n",
                static_cast<u32>(mCreateParams.mContentSpecName), mfTimeThroughFade, lfFadeOut,
                lfFraction, mfGainPreFade, lfGain);
        }

        if (mfTimeThroughFade < GetFadeOut())                                        // 0x826EEBD4
            return false;
        mVoice.Release();
        meDetachState = E_DETACH_STATE_UPDATING;
        // fall through
    }
    case E_DETACH_STATE_UPDATING:
        if (!mbBufferReleased)
            return false;
        meDetachState = E_DETACH_STATE_FINISHED;
        // fall through
    case E_DETACH_STATE_FINISHED:
        return BrnEffectObject::Detach();
    default:
        return false;
    }
}

void StreamingEffect::Notify(const CgsSound::Io::MessageHeader* apkMessage)
{
    CGS_ASSERT(apkMessage != 0, "lpMessageHeader");
    if (!apkMessage)
        return;
    CGS_ASSERT(apkMessage->GetEventId() == 16,
               "lpMessageHeader->GetEventId() == E_SOUNDMESSAGE_QUEUE_ELEMENT");
    const CgsSound::Io::Message<CgsSound::Io::QueueElement>* lpMessage =
        static_cast<const CgsSound::Io::Message<CgsSound::Io::QueueElement>*>(apkMessage);
    if (lpMessage->mData == mVoiceId)
        mbBufferReleased = true;
}

bool StreamingEffect::IsBusy() const
{
    const CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE leStage = mVoice.GetUpdateStage();
    return leStage >= CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_CREATE &&
           leStage <= CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_START;
}

f32 StreamingEffect::FindStreamSettings(const Attrib::Gen::streamsettings& arSettings,
                                        CgsSound::Logic::Command::QueueElement auContentSpec) const
{
    s32 liLow = 0;
    s32 liHigh = static_cast<s32>(arSettings.Num_ContentSpecs()) - 1;
    while (liLow <= liHigh)
    {
        const s32 liMiddle = (liLow + liHigh) / 2;
        const u32 luSpec = arSettings.ContentSpecs(static_cast<u32>(liMiddle));
        if (auContentSpec < luSpec)
            liHigh = liMiddle - 1;
        else if (auContentSpec > luSpec)
            liLow = liMiddle + 1;
        else
            return arSettings.Volumes(static_cast<u32>(liMiddle));
    }
    // @0x82683CAC: the console's own miss report, gated on message-filter bit 0, then
    // flt_82001C98 == 1.0f.
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint)
    {
        *CgsDev::Log::gpDebugPrint << "FindStreamSettings : Not found for "
                                   << static_cast<s32>(auContentSpec) << "\n";
    }
    return 1.0f;
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
