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
//   +0x04  mpGameInterface (the ctor's 2nd argument, `stw r4, 4(this)`; callback
//                           handlers call through this IGameInterface vtable.)
//   +0x08  mpQueue         (the ctor's 3rd argument, `stw r5, 8(this)`; the
//                           RealmcCore::MessageQueue ProcessMessage posts onto --
//                           `lwz r3, 8(this)` is the PostResponse `this`.)
//   +0x0C  maMessage       (an embedded RealmcCore::MessagePtr -- own vtable @ +0xC,
//                           held message @ +0x10. Bound in the ctor to the shared
//                           empty message, and rebound to it again in ProcessMessage.)
//
// sizeof == 0x14 (20) on X360 -- the `vector deleting destructor' @0x82B549B8 frees
// 20 bytes, i.e. the vtable + mpGameInterface + mpQueue + the 8-byte MessagePtr. (On the
// LLP64 PC target the pointer members widen, so byte size/offsets are NOT matched,
// per the project's semantic-parity rule -- documented above, not asserted.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"           // IMessageProcessor base, MessagePtr, ResponsePtr, Response, FreeMemSize
#include "SDKs/Realmc/RealmcIGameInterface.h" // ARTIST +4 callback target
#include "SDKs/Realmc/RealmcMessageQueue.h"   // RealmcCore::MessageQueue (+ its PostResponse decl)

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// RealmcIface::GameCallbackProcessor -- the game-thread RealmcCore::IMessageProcessor
// the interface's update loop applies every queued message to. ProcessMessage
// (Response*) is its override of the processor's +0x50 slot. The remaining
// overloads follow the ARTIST table at off_821486B8; eight payload tag spellings
// are explicitly inferred in RealmcIfaceOpaqueMessages.h where their native
// class names are absent.
// ---------------------------------------------------------------------------
class GameCallbackProcessor : public RealmcCore::IMessageProcessor
{
public:
    // @ 0x82B54968 -- store mpGameInterface (+4) and mpQueue (+8), install the final
    //                 vtable (MSVC ctor prologue), then construct the embedded
    //                 MessagePtr (+0xC) bound to the shared empty message
    //                 (EMPTY_MESSAGE(), AddRefing it -- the X360 inlines the
    //                 MessagePtr ctor via sub_82C457F8(this+0xC, EMPTY_MESSAGE())).
    GameCallbackProcessor(IGameInterface* pGameInterface,
                          RealmcCore::MessageQueue* pQueue);

    // @ 0x82B54388 -- wrap the incoming response in a stack ResponsePtr (AddRef),
    //                 post the (held message, response) pair onto mpQueue, tear the
    //                 stack ResponsePtr down (Release), then rebind the held
    //                 MessagePtr to the empty message. This is the processor's
    //                 +0x50 slot, the one Response::Apply dispatches into.
    void ProcessMessage(RealmcCore::Response* pResponse) override;

    // Handler order matches the 21 ARTIST IMessageProcessor slots. Their bodies
    // are homed in this TU; the overload names are the native ProcessMessage
    // symbol where present, with slot/handler-role names otherwise.
    void ProcessMessage(RealmcIface::MessageSetActiveCardDone* pMessage) override;       // +0x04
    void ProcessMessage(RealmcIface::MessageSlot08Payload* pMessage) override;           // +0x08 shared stub
    void ProcessMessage(RealmcIface::MessageCardRemoved* pMessage) override;             // +0x0C
    void ProcessMessage(RealmcIface::MessageCheckLoadedData* pMessage) override;          // +0x10
    void ProcessMessage(RealmcIface::MessageClearEntriesPayload* pMessage) override;      // +0x14
    void ProcessMessage(RealmcIface::MessageSetAutosaveDone* pMessage) override;          // +0x18
    void ProcessMessage(RealmcIface::MessageSlot1CPayload* pMessage) override;           // +0x1C shared stub
    void ProcessMessage(RealmcIface::MessageCallback3CPayload* pMessage) override;        // +0x20
    void ProcessMessage(RealmcIface::MessageFoundEntryPayload* pMessage) override;        // +0x24
    void ProcessMessage(RealmcIface::MessageFindEntriesDonePayload* pMessage) override;  // +0x28
    void ProcessMessage(RealmcIface::MessageSlot2CPayload* pMessage) override;           // +0x2C shared stub
    void ProcessMessage(RealmcIface::MessageLoadDone* pMessage) override;                 // +0x30
    void ProcessMessage(RealmcIface::MessageSaveDone* pMessage) override;                 // +0x34
    void ProcessMessage(RealmcIface::MessageSaveCheckDonePayload* pMessage) override;    // +0x38
    void ProcessMessage(RealmcIface::MessageBootupDone* pMessage) override;              // +0x3C
    void ProcessMessage(RealmcIface::MessageShowAutosaveIcon* pMessage) override;        // +0x40
    void ProcessMessage(RealmcCore::MessageClear* pMessage) override;                    // +0x44
    void ProcessMessage(RealmcCore::MessageTrc* pMessage) override;                     // +0x48
    void ProcessMessage(RealmcCore::MessageString* pMessage) override;                  // +0x4C shared stub
    void ProcessMessage(RealmcCore::Message* pMessage) override;                        // +0x54 shared stub

    // @ 0x82B54340 -- the embedded MessagePtr member's destructor (Release the held
    //                 message + null it) runs as the member is destroyed; the vtable
    //                 stores frame the base-class teardown the compiler emits. Backs
    //                 the X360 `vector deleting destructor' @0x82B549B8, which frees
    //                 20 bytes (sizeof) when the delete flag bit0 is set -- the
    //                 virtual dtor + the class operator delete reproduce it.
    ~GameCallbackProcessor() ;

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    IGameInterface*           mpGameInterface; // +0x04 (ctor arg 2; callback target)
    RealmcCore::MessageQueue* mpQueue;    // +0x08  (ctor arg 3; the response queue)
    RealmcCore::MessagePtr    maMessage;  // +0x0C  (embedded MessagePtr; vtable @ +0xC, message @ +0x10)
};

} // namespace RealmcIface
