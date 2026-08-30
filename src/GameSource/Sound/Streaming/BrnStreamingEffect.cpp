#include "GameSource/Sound/Streaming/BrnStreamingEffect.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lpState / IsAttached tripwires)
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
// FLAG (Attrib::Gen::streamsettings not yet homed): the tail
// `Attrib::Gen::streamsettings::streamsettings(this+0xB8, 0, 0)` is an AttribSys-
// generated attribute-table ctor (same (ptr,0,0) shape as the committed
// CollisionStateManager siblings). No `streamsettings` generated-class home exists
// yet, so it is modelled as an opaque, correctly-sized byte span (maStreamSettings)
// explicitly zeroed to mirror the generated ctor's default-data-area construction
// -- without fabricating the generated class's body.
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

    if (mStreamSettings.GetCollection())
    {
        CGS_ASSERT(mStreamSettings.Num_ContentSpecs() == mStreamSettings.Num_Volumes(),
                   "mStreamSettings.Num_ContentSpecs() == mStreamSettings.Num_Volumes()");
        mfGain = FindStreamSettings(mStreamSettings, mCreateParams.mContentSpecName);
    }
    else
    {
        mfGain = 1.0f;
    }

    mVoiceId = static_cast<CgsSound::Logic::Command::QueueElement>(mVoice.GetVoice().GetIdent());
    mfElapsedTime = 0.0f;
    mfTimeThroughFade = 0.0f;
    mfGainPreFade = 0.0f;
    mbBufferReleased = false;
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
    if (leStage != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE &&
        leStage != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED)
    {
        lRequest.mpAttachment->UpdateVoiceParams(mVoice, mfGain, mfElapsedTime);
        return;
    }

    StreamingState* lpState = static_cast<StreamingState*>(mpState);
    if (lpState && lpState->Detach())
        lRequest.mpAttachment->StreamStopped();
}

f32 StreamingEffect::GetFadeOut() const
{
    const StreamingState* lpState = static_cast<const StreamingState*>(mpState);
    CGS_ASSERT(lpState != 0, "lpState");
    CGS_ASSERT(lpState && lpState->GetUpdateState() == CgsSound::Logic::State::E_UPDATE_DETATCHING,
               "lpState->GetUpdateState() == E_UPDATE_DETATCHING");
    return lpState ? lpState->GetFadeOut() : 0.0f;
}

bool StreamingEffect::Detach()
{
    switch (meDetachState)
    {
    case E_DETACH_STATE_NONE:
    {
        mfTimeThroughFade = 0.0f;
        const s32 liSendName = static_cast<s32>(mCreateParams.mSendName);
        mfGainPreFade = mVoice.GetGain(&liSendName);
        meDetachState = E_DETACH_STATE_BEGIN;
        // fall through
    }
    case E_DETACH_STATE_BEGIN:
    {
        mfElapsedTime += mfDeltaTime;
        mfTimeThroughFade += mfDeltaTime;
        const f32 lfFadeOut = GetFadeOut();
        const f32 lfFraction = lfFadeOut > 0.0f
            ? std::min(1.0f, std::max(0.0f, mfTimeThroughFade / lfFadeOut))
            : 1.0f;
        const u32 luSendName = mCreateParams.mSendName;
        mVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex),
                       (1.0f - lfFraction) * mfGainPreFade, &luSendName);
        if (mfTimeThroughFade < lfFadeOut)
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
    return 1.0f;
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
