#ifndef BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_H
#define BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"          // BrnSound::Logic::BrnState (the DWARF base)
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"   // StreamRequest (DWARF home BrnStreamingStateManager.h:49)

// =============================================================================
// BrnSound::Logic::Streaming::StreamingState
//   GameSource/Sound/Streaming/BrnStreamingState.h (DWARF home) +
//   GameSource/Sound/Streaming/BrnStreamingState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF. StreamingState
// is the per-stream state record owned by a StreamingEffect.
//
// (2026-08-25, audio-faithfulness wave 5 RECONCILIATION): this header used to be a
// base-less minimal model (opaque maLeading[0x48] + mbAttached@0x48 + gap + a
// local rival `StreamingRequest`), and the same class had a SECOND rival inside
// GameShared CgsState.h (ctor-derived numeric names mi84..mu104). The DWARF
// (BrnStreamingState.h:36) is unambiguous:
//   struct StreamingState : public BrnSound::Logic::BrnState
//   { StreamRequest mRequest; float mfFadeOut; }      (:105/:106)
// and every prior numeric decodes onto it exactly (console offsets):
//   +0x48 (72)  "mbAttached"        == State::mbIsAttached (the base member --
//               IsAttached() now comes from the canonical State base)
//   +0x54 (84)  mRequest            == the mi84..mu104 ctor span, field-for-field
//               the manager's StreamRequest: mpAttachment@84, mu32Priority@88,
//               mfLagTolerance@92, mfTimeStamp@96, mu32UniqueId@100, mbDirty@104
//   +0x6C (108) mfFadeOut           (not stored by the ctor)
// The old local `StreamingRequest` was the SAME type as the manager-ring
// StreamRequest (DWARF types mRequest as StreamRequest; the effect's re-issue
// path block-copies it into the manager's re-post ring), so it is retired in
// favour of the real DWARF home included above.
//
// DEFER (DWARF-listed surfaces not reconstructed here -- each its own slice):
// the RTTI set (sTypeInfo/GetTypeInfo/GetTypeName/GetStaticTypeInfo/CreateObject
// @0x826C9AA8, descriptor rodata @0x82F2E83C), virtual Attach/Detach/UpdateParams,
// GetStreamingStateManager, SetFadeOut/GetFadeOut, private SetRequest.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// the console offsets above are comments, not asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// DWARF BrnStreamingState.h:36. The per-stream sound-logic state.
struct StreamingState : public BrnSound::Logic::BrnState
{
    // @ 0x826B0CB0 (was homed in the GameShared CgsState.cpp rival). Zeroes the
    // embedded request field-for-field (5 words + the mbDirty byte). Bodied in
    // BrnStreamingState.cpp.
    StreamingState();

    // @ 0x826C9B28 -- scalar deleting destructor. Installs StreamingState's own
    // vtable (off_820AE1F4), calls State::DestroyEffects() to tear down attached
    // effects, re-installs the MemBase base vtable (off_820AA820), and (deleting
    // flavour) routes the storage back through the sound allocator. Observable
    // body = the DestroyEffects() call; the vtable installs + conditional
    // allocator free are the compiler-synthesised deleting-destructor parts.
    // Bodied in BrnStreamingState.cpp.
    virtual ~StreamingState();

    virtual void Attach(void* apvAttachment); // @ 0x826C9BC8
    virtual bool Detach();                     // @ 0x826C9C58
    virtual void UpdateParams(f32 af32DeltaTime)
    {
        CgsSound::Logic::State::UpdateParams(af32DeltaTime);
    }

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();
    static CgsSound::Logic::State* CreateObject(u32 auType);

    // @ 0x82683A00 (DWARF h:166: `const StreamRequest& GetRequest() const`).
    // Asserts IsAttached() (the State-base flag at +0x48, `lbz r11, 0x48(state)`;
    // assert cite BrnStreamingState.h:168 -- a non-gating tripwire) and returns
    // the embedded request at +0x54. Bodied in BrnStreamingState.cpp.
    const StreamRequest& GetRequest() const;

    StreamingStateManager* GetStreamingStateManager() const;
    void SetFadeOut(f32 afFadeOut) { mfFadeOut = afFadeOut; }
    f32 GetFadeOut() const { return mfFadeOut; }
    CgsSound::Logic::State::EUpdateState GetUpdateState() const
    {
        return static_cast<CgsSound::Logic::State::EUpdateState>(mauUpdateState[0]);
    }

private:
    // DWARF :105. The stream request this state is servicing (the manager-ring
    // StreamRequest type, assigned in via the deferred SetRequest).
    StreamRequest mRequest;                 // +0x54 (84)

    // DWARF :106. Stop fade-out seconds (paired with StreamStopRequest::mfFadeOut).
    f32 mfFadeOut;                          // +0x6C (108)
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_H
