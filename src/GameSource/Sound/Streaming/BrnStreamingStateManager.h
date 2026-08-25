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

// Forward decl for StreamRequest.mpAttachment (pointer only; real home
// BrnStreamingEffect.h -- an interface, not reconstructed here).
class IStreamUser;

// BrnStreamingStateManager.h:49 (DWARF) -- 24 bytes / 6 dwords (== X360 slot stride).
// The maPlayRequests / maRePostRequests ring element. ALSO the type of
// StreamingState::mRequest (DWARF BrnStreamingState.h:105 types it StreamRequest;
// the state ctor @0x826B0CB0 zeroes it field-for-field) -- an earlier note here
// called the state's embedded request a distinct type; the wave-5 reconciliation
// (2026-08-25) proved them the same and retired the state-side rival.
struct StreamRequest
{
    IStreamUser* mpAttachment;   // +0x00
    u32          mu32Priority;   // +0x04
    f32          mfLagTolerance; // +0x08
    f32          mfTimeStamp;    // +0x0C  (stamped in PostStreamRequest)
    u32          mu32UniqueId;   // +0x10  (stamped in PostStreamRequest)
    bool         mbDirty;        // +0x14  (+3 pad -> 24)
};

// BrnStreamingStateManager.h:81 (DWARF) -- 16-byte stop-request ring element.
struct StreamStopRequest
{
    const IStreamUser* mpAttachment; // +0x00
    f32                mfFadeOut;    // +0x04
    f32                mfTimeStamp;  // +0x08
    u32                mu32UniqueId; // +0x0C
};

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

    // ---- request-ring API (DWARF-attested) ----
    // Capacity of the play/stop/re-post rings (asm cmplwi cr6, count, 6).
    enum { E_MAX_STREAM_REQUESTS = 6, E_MAX_REQUEST_RE_POSTS = 6 };

    // @ 0x826834F0 (DWARF h:255). Append a play request, stamp its timestamp + unique
    // id from the manager's running counters, then bump both.
    void PostStreamRequest( const StreamRequest& lStreamRequest );

private:
    // @ 0x826836A8 (DWARF h:305). Append a play request to the ring + bump the count.
    void PostStreamRequestInternal( const StreamRequest& arRequest );

    // @ 0x82683750 (DWARF h:347). Append a re-post request to the ring + bump the count.
    void RePostStreamRequest( const StreamRequest& request );

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

    // DWARF-attested request rings (BrnStreamingStateManager.h:229-237), in declaration
    // ORDER. The PostStreamRequest/PostStreamRequestInternal/RePostStreamRequest bodies
    // touch these BY NAME. mfCurrentTime (the timestamp source) is the committed base
    // member (CgsStateManager.h:253). Offsets are X360 facts; pinned BY NAME, the 0x228
    // total NOT static_asserted on host.
    StreamRequest     maRePostRequests[6];  // +0x098 (DWARF :229)
    StreamRequest     maPlayRequests[6];    // +0x128 (DWARF :230; the PostStreamRequest ring)
    StreamStopRequest maStopRequests[6];    // +0x1B8 (DWARF :231)
    u32               muPlayRequestCount;   // +0x218 (DWARF :233)
    u32               muStopRequestCount;   // +0x21C (DWARF :234)
    u32               muNumRePostRequests;  // +0x220 (DWARF :235)
    u32               muUniqueId;           // +0x224 (DWARF :237; monotonic id stamped per post)
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_MANAGER_H
