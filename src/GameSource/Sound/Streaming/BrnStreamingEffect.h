#ifndef BRN_SOUND_LOGIC_STREAMING_STREAMING_EFFECT_H
#define BRN_SOUND_LOGIC_STREAMING_STREAMING_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Streaming/BrnStreamingState.h"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // BrnSound::Logic::BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)
#include "GameSource/AttribSys/Generated/classes/streamsettings.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"

// =============================================================================
// BrnSound::Logic::Streaming::StreamingEffect
//   GameSource/Sound/Streaming/BrnStreamingEffect.h (DWARF home) +
//   GameSource/Sound/Streaming/BrnStreamingEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// StreamingEffect is the sound-logic effect that drives a streaming voice. It owns
// (by pointer, at +0x0C) the StreamingState that holds the live stream request.
//
// This TU (ledger id class:BrnSound::Logic::Streaming) bodies:
//   BrnSound::Logic::Streaming::StreamingEffect::StreamingEffect  @ 0x826C9D10 (ctor)
//   BrnSound::Logic::Streaming::StreamingEffect::GetRequest       @ 0x82683B40
//   BrnSound::Logic::Streaming::StreamingEffect::~StreamingEffect (anchors the X360
//     `vector deleting destructor' @ 0x826E24B8)
// The X360 GetRequest symbol is rendered by IDA as the truncated "StreamingEffect_";
// its two call sites (ProcessUpdate @ 0x826E2540 and ::Attach @ 0x826EE8D0) use the
// return value as `GetRequest().mpAttachment` (asserted at BrnStreamingEffect.cpp)
// and block-copy the returned 24-byte Request -- i.e. it is the GetRequest()
// accessor returning StreamingState::mRequest.
//
// DUAL-BASE (ctor @ 0x826C9D10): the ctor installs the SAME transient
// IResourceRequester sub-vptr (off_820AE954 @ this+4) seen in the committed
// ExplosionEffect/CollisionEffect ctors before overwriting it with the final leaf
// vtable pair (off_820B38A0 @ this+0, off_820B386C @ this+4) -- i.e. the same
// BrnEffectObject dual-base shape (primary vptr @ this+0, IResourceRequester
// sub-object vptr @ this+4) reused BY NAME from BrnEffectObject.h (same as
// ExplosionEffect). mpState (the +0x0C member GetRequest() asserts non-null) is
// nulled by the ctor and read by name in GetRequest().
//
// The ctor's stores through +0x34 belong to the BrnEffectObject base; +0x38..+0x64
// is the twelve-word mCreateParams (DWARF CgsVoice::VoiceWrapper::CreateParams).
// The base and mCreateParams constructors below account for those stores by name.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ctor/accessor reach members by
// absolute byte offset; on the 64-bit host pointer/vptr widths differ, so members
// are pinned BY NAME and SEQUENCE only and absolute offsets are NOT static_asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// DWARF: StreamingEffect : public BrnSound::Logic::BrnEffectObject. Reuses the
// committed BrnEffectObject dual base BY NAME (same shape as the ExplosionEffect
// sibling) and embeds a CgsSound::Logic::VoiceWrapper (+0x68) plus an
// Attrib::Gen::streamsettings block (+0xB8).
struct StreamingEffect : public BrnSound::Logic::BrnEffectObject
{
    // @ 0x826C9D10 -- the leaf constructor. Bodied in BrnStreamingEffect.cpp.
    StreamingEffect();

    // Anchors the X360 `vector deleting destructor' @ 0x826E24B8 (empty out-of-line
    // body; the dual-base settle + embedded member teardown are compiler-synthesised,
    // and the (a2&1) free through off_82FFB954 is host codegen / DEFERRED). Declared
    // virtual to mirror the committed ExplosionEffect/CollisionEffect siblings.
    virtual ~StreamingEffect();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);

    virtual bool Attach();
    virtual void UpdateParams(f32 af32DeltaTime);
    virtual void ProcessUpdate();
    virtual bool Detach();
    virtual void Notify(const CgsSound::Io::MessageHeader* apkMessage);

    // BrnStreamingEffect.cpp (DWARF assert site, "lpState") + BrnStreamingState.h
    // ("IsAttached()"). Asserts the owned state exists and is attached, then returns
    // a reference to the state's embedded request (the manager-ring StreamRequest
    // type -- see the wave-5 reconciliation note in BrnStreamingState.h).
    // @ 0x82683B40.
    const StreamRequest& GetRequest() const;

    f32 GetElapsedTime() const { return mfElapsedTime; }
    bool IsBusy() const;
    f32 GetTimeThroughFade() const { return mfTimeThroughFade; }
    CgsSound::Logic::VoiceWrapper& GetVoiceWrapper() { return mVoice; }
    const CgsSound::Logic::VoiceWrapper& GetVoiceWrapper() const { return mVoice; }

protected:
    f32 GetFadeOut() const;
    f32 FindStreamSettings(const Attrib::Gen::streamsettings& arSettings,
                           CgsSound::Logic::Command::QueueElement auContentSpec) const;

    CgsSound::Logic::VoiceWrapper::CreateParams mCreateParams;
    CgsSound::Logic::VoiceWrapper mVoice;
    Attrib::Gen::streamsettings mStreamSettings;
    // ARTIST tail after the 0x10-byte streamsettings instance:
    //   +0xC8 elapsed, +0xCC fade time, +0xD0 gain, +0xD4 pre-fade gain,
    //   +0xD8 voice id, +0xDC buffer-release flag; object size is 0xE0.
    f32 mfElapsedTime;
    f32 mfTimeThroughFade;
    f32 mfGain;
    f32 mfGainPreFade;
    CgsSound::Logic::Command::QueueElement mVoiceId;
    bool mbBufferReleased;
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_STREAMING_EFFECT_H
