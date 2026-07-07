#ifndef SDKS_PACKAGES_GAMETALK_IGAMETALKPROTOCOL_H
#define SDKS_PACKAGES_GAMETALK_IGAMETALKPROTOCOL_H

#include "types.hpp"  // s32 / u32

// ============================================================================
// SDKs/Packages/GameTalk/1.8.0/include/GameTalk/IGameTalkProtocol.h  (DWARF home)
//
// EA::GameTalk::IGameTalkProtocol -- the GameTalk package's abstract transport
// protocol interface. A GameTalk protocol implementation (the platform/transport
// backend that actually ferries serialized GameTalk messages between the running
// game and an external authoring tool) derives from this and overrides the
// protocol hooks. The GameTalk manager dispatches send/receive through this
// polymorphic base.
//
// This is EA vendor (GameTalk package 1.8.0) code: members/methods follow EA's
// PascalCase convention, not the project Brn/Cgs K..._ naming.
//
// GROWN (2026-07) for EA::GameTalk::GameTalkManager, the first reconstructed TU
// that dispatches through this interface. The manager reaches the transport in
// three grounded ways (all read straight off the manager asm, so the vtable slots
// / member offset are NOT fabricated):
//   * it stores its receive trampoline directly into the transport's +0x04
//     callback member (GameTalkManager ctor: `stw ReceiverCallback, 4(protocol)`);
//   * it calls the connected-state virtual at vtable slot 1 / offset +0x04
//     (ctor gate `(*(*protocol + 4))(protocol)`, bool);
//   * it sends serialized wire buffers through the Send virtual at vtable slot 2 /
//     offset +0x08 (`(*(*protocol + 8))(protocol, buffer, size)`).
// The X360 transport backend (CgsGameTalk::GameTalkProtocol) overrides IsConnected
// then Send then Poll in that order, which pins the vtable order below.
//
// The base scalar-deleting destructor (the original TU in scope, X360 @ 0x827DB430)
// is still defined OUT-OF-LINE in the sibling IGameTalkProtocol.cpp.
// ============================================================================

namespace EA
{
namespace GameTalk
{
    // EA::GameTalk::IGameTalkProtocol -- abstract GameTalk transport protocol base.
    class IGameTalkProtocol
    {
    public:
        // Polymorphic base destructor (vtable slot 0). Defined OUT-OF-LINE in the
        // sibling IGameTalkProtocol.cpp so the interface owns a real .cpp definition
        // home -- the X360 base scalar-deleting destructor @ 0x827DB430 (stores the
        // IGameTalkProtocol vtable off_820CDC0C into *this, then for the deleting
        // variant conditionally operator-delete's). The body is empty (only the
        // function-pointer member below, which needs no destruction); the vtable
        // store + delete-tail is what the compiler emits.
        virtual ~IGameTalkProtocol();

        // vtable slot 1 (offset +0x04): does the transport currently have a live
        // connection to the authoring tool? GameTalkManager gates its config
        // handshake on this at construction.
        virtual bool IsConnected() = 0;

        // vtable slot 2 (offset +0x08): ship liMsgSize bytes of an already-serialized
        // GameTalk wire buffer over the transport. GameTalkManager drives every
        // outgoing message through here.
        virtual bool Send(void* lpaBuffer, s32 liMsgSize) = 0;

        // +0x04: the raw-wire receive callback. GameTalkManager installs its
        // ReceiverCallback trampoline here at construction; the transport backend
        // fans each complete inbound message out through it (a no-op if null). Owned
        // by the interface because the manager -- which holds only this base pointer
        // -- writes it directly.
        void (*mpfnMessageHandler)(const char* lpacMessage, u32 luLength); // +0x04
    };
}  // namespace GameTalk
}  // namespace EA

#endif // SDKS_PACKAGES_GAMETALK_IGAMETALKPROTOCOL_H
