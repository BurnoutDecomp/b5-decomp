#ifndef SDKS_PACKAGES_GAMETALK_IGAMETALKPROTOCOL_H
#define SDKS_PACKAGES_GAMETALK_IGAMETALKPROTOCOL_H

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
// MINIMAL SURFACE -- only the polymorphic base destructor (the TU in scope,
// X360 @ 0x827DB430) is committed here. The GameTalk protocol's remaining
// pure-virtual transport hooks are NOT modelled: their vtable slots / argument
// shapes are out of scope for this TU and are reconstructed (GROWING this header
// additively) when a TU that dispatches through them is decompiled. Declaring
// only the destructor keeps the polymorphic base the deleting-destructor thunk
// needs without fabricating sibling vtable offsets.
// ============================================================================

namespace EA
{
namespace GameTalk
{
    // EA::GameTalk::IGameTalkProtocol -- abstract GameTalk transport protocol base.
    class IGameTalkProtocol
    {
    public:
        // Polymorphic base destructor. Defined OUT-OF-LINE in the sibling
        // IGameTalkProtocol.cpp so the interface owns a real .cpp definition home --
        // the X360 base scalar-deleting destructor @ 0x827DB430 (stores the
        // IGameTalkProtocol vtable off_820CDC0C into *this, then for the deleting
        // variant conditionally operator-delete's). The body is empty (no members,
        // no base); the vtable store + delete-tail is what the compiler emits.
        virtual ~IGameTalkProtocol();
    };
}  // namespace GameTalk
}  // namespace EA

#endif // SDKS_PACKAGES_GAMETALK_IGAMETALKPROTOCOL_H
