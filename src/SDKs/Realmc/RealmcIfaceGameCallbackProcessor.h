#pragma once

// ===========================================================================
// RealmcIface::GameCallbackProcessor -- a Realmc save/load interface object that
// posts a response back onto the cross-thread request/response MessageQueue and
// resets its held message. It derives the shared Realmc message processor
// RealmcCore::IMessageProcessor (as RealmcCore::MessageFilter does) and embeds a
// RealmcCore::MessagePtr.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no Feb-2007 leak source, no DWARF).
// `Realmc` is a vendor library boundary, so its identifiers (RealmcIface,
// GameCallbackProcessor, ProcessMessage) are preserved verbatim per the naming
// convention. Per-function X360 addresses:
//
//     RealmcIface::GameCallbackProcessor::GameCallbackProcessor        @ 0x82B54968
//     RealmcIface::GameCallbackProcessor::ProcessMessage               @ 0x82B54388
//     RealmcIface::GameCallbackProcessor::~GameCallbackProcessor       @ 0x82B54340
//     RealmcIface::GameCallbackProcessor::`vector deleting destructor' @ 0x82B549B8
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// LAYOUT (from the ctor stores @0x82B54968 + the deleting-dtor free size = 20):
//   +0x00  vtable pointer  (base IMessageProcessor then the final
//                           GameCallbackProcessor vtable off_821486B8 -- MSVC's
//                           base-then-final derived-ctor sequence; the dtor
//                           installs off_821486B8 then restores off_82148660)
//   +0x04  mpContext       (the ctor's 2nd argument, `stw r4, 4(this)`; stored but
//                           NOT read by any of this TU's four functions.
//                           FLAG: role/type unrecovered -- modelled as an opaque
//                           pointer word.)
//   +0x08  mpQueue         (the ctor's 3rd argument, `stw r5, 8(this)`; the
//                           RealmcCore::MessageQueue ProcessMessage posts onto --
//                           `lwz r3, 8(this)` is the PostResponse `this`.)
//   +0x0C  maMessage       (an embedded RealmcCore::MessagePtr -- own vtable @ +0xC,
//                           held message @ +0x10. Bound in the ctor to the shared
//                           empty message, and rebound to it again in ProcessMessage.)
//
// sizeof == 0x14 (20) on X360 -- the `vector deleting destructor' @0x82B549B8 frees
// 20 bytes, i.e. the vtable + mpContext + mpQueue + the 8-byte MessagePtr. (On the
// LLP64 PC target the pointer members widen, so byte size/offsets are NOT matched,
// per the project's semantic-parity rule -- documented above, not asserted.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"           // IMessageProcessor base, MessagePtr, ResponsePtr, Response, FreeMemSize
#include "SDKs/Realmc/RealmcMessageQueue.h"   // RealmcCore::MessageQueue (+ its PostResponse decl)

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// RealmcIface::GameCallbackProcessor -- the game-thread RealmcCore::IMessageProcessor
// the interface's update loop applies every queued message to. ProcessMessage
// (Response*) is its override of the processor's +0x50 slot. Its handlers for
// the other message types drive the game-side callback object at +4 and are not
// declared here, so on the host the class stays abstract (no host code
// constructs one).
// ---------------------------------------------------------------------------
class GameCallbackProcessor : public RealmcCore::IMessageProcessor
{
public:
    // @ 0x82B54968 -- store mpContext (+4) and mpQueue (+8), install the final
    //                 vtable (MSVC ctor prologue), then construct the embedded
    //                 MessagePtr (+0xC) bound to the shared empty message
    //                 (EMPTY_MESSAGE(), AddRefing it -- the X360 inlines the
    //                 MessagePtr ctor via sub_82C457F8(this+0xC, EMPTY_MESSAGE())).
    GameCallbackProcessor(void* pContext, RealmcCore::MessageQueue* pQueue);

    // @ 0x82B54388 -- wrap the incoming response in a stack ResponsePtr (AddRef),
    //                 post the (held message, response) pair onto mpQueue, tear the
    //                 stack ResponsePtr down (Release), then rebind the held
    //                 MessagePtr to the empty message. This is the processor's
    //                 +0x50 slot, the one Response::Apply dispatches into.
    void ProcessMessage(RealmcCore::Response* pResponse) override;

    // @ 0x82B54340 -- the embedded MessagePtr member's destructor (Release the held
    //                 message + null it) runs as the member is destroyed; the vtable
    //                 stores frame the base-class teardown the compiler emits. Backs
    //                 the X360 `vector deleting destructor' @0x82B549B8, which frees
    //                 20 bytes (sizeof) when the delete flag bit0 is set -- the
    //                 virtual dtor + the class operator delete reproduce it.
    ~GameCallbackProcessor() override;

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    void*                     mpContext;  // +0x04  (ctor arg 2; FLAG: role unrecovered)
    RealmcCore::MessageQueue* mpQueue;    // +0x08  (ctor arg 3; the response queue)
    RealmcCore::MessagePtr    maMessage;  // +0x0C  (embedded MessagePtr; vtable @ +0xC, message @ +0x10)
};

} // namespace RealmcIface
