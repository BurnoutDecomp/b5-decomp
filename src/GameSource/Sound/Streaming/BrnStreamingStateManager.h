#ifndef BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)

// =============================================================================
// BrnSound::Logic::Streaming::StreamingStateManager
//   GameSource/Sound/Streaming/BrnStreamingStateManager.{h,cpp}
//   (canonical home -- CONFIRMED by the FireAssert source-path string inside
//    Prepare @ 0x826EE680:
//    "d:\p4\b5_main\burnout\main\code\gamesource\unity\../Sound/Streaming/BrnStreamingStateManager.cpp")
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// StreamingStateManager is the sound-logic state manager that owns the streamed
// audio voices (the per-stream playback bookkeeping). It is one of the 9 managers
// the SoundLogicModule factory CreateStateManagers (0x826AFEF8) creates via
// CreateStateMan.
//
// BASE CHAIN: StreamingStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x826FBFE0 installs a
//   primary vtable @ +0 (off_820B7F80) AND a secondary sub-object vtable @ +0x90
//   (*(result+144) = off_820B7F78, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and Prepare @ 0x826EE680 routes through it.
//   Same shape as the committed siblings AIVehicleStateManager / PassbyStateManager.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 552 bytes (0x228)
// behind 4-byte pointers/vptrs (CreateObject @ 0x82700B68 allocates 552); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the 0x228
// size / absolute offsets are NOT static_asserted. The ctor zero-inits a dense block
// of f32 + int fields from +0x98..+0x214 (the per-stream voice/parameter table); no
// recovered body in this slice names an individual data member by a recovered field
// name, so the ~360 bytes of streaming-voice state are deferred (see FLAG) and a
// single opaque pad honestly names it.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

class StreamingStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // StreamingStateManager @ 0x826FBFE0. Forwards to the BrnStateManager base ctor
    // and zero-inits the per-stream voice/parameter table.
    StreamingStateManager();
    virtual ~StreamingStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x82700B68 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x82700B68

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826EE680  (vtable +0x0C; stub -- see .cpp FLAG)

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void                            ResourcesAreReady();    // (stub -- domain cascade; see .cpp FLAG)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed; see .cpp FLAG)

private:
    // FLAG (deferred body -- ~360 bytes): the X360 object is 552 bytes (0x228). The
    // per-stream voice/parameter state (the dense f32 + int table the ctor zero-inits
    // from +0x98 to +0x214 -- per-stream gains, fades, voice handles, the streaming
    // bookkeeping driven by Prepare / the per-frame update) is NOT modelled in this
    // minimal shell -- the streaming audio domain is not reconstructed. The shell
    // exists only to be a CONCRETE, registrable leaf whose Prepare() returns true for
    // PrepareStateManagersOnBoot. A single opaque pad keeps the deferred state honestly
    // named without fabricating field meanings. Size is UNVERIFIED on host (the X360
    // 0x228 is a 32-bit fact); NOT static_asserted.
    u8 maDeferredStreamingState[1]; // placeholder for the un-reconstructed streaming-voice members
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_MANAGER_H
