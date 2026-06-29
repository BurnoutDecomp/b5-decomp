#ifndef BRN_NETWORK_CONNECTION_MANAGER_H
#define BRN_NETWORK_CONNECTION_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"   // CgsSystem::Time (mLastKickTime, by value)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::NetworkPlayerID

// ===========================================================================
// BrnNetwork::ConnectionManager
//   Home: GameSource/Network/Managers/BrnNetworkConnectionManager.{h,cpp}
//
// The host-side NAT-kick policy manager. It is owned by (and back-pointers to)
// the BrnNetworkManager. During the session UpdateNATData() walks the player
// registry and, on a host that has not yet started the game, kicks the player
// that cannot NAT-traverse to its peers -- either an unconnectable player once
// all peers are otherwise linked, or, while peers are still connecting, the
// pivot player a failed connection pair identifies. KickUnNATablePlayer() issues
// the actual kick through whichever server-interface component (game or userset)
// the local player currently belongs to.
//
// LAYOUT (X360 ARTIST asm, member accesses by offset on `this`):
//   +0x00  mLastKickTime    (CgsSystem::Time -- seconds@+0, fraction@+4; the asm
//                            reads/writes this+0 (s32) and this+4 (f32) as a unit)
//   +0x08  mpNetworkManager (BrnNetworkManager* -- Construct stores its arg here
//                            via `stw r4, 8(r3)`; every method reads *(this+8))
//
// The X360 +0x08 member offset is a 32-bit-pointer fact; it is NOT static_asserted
// on a 64-bit host (the embedded Time + the pointer widen there). Members are
// pinned BY NAME.
//
// KF_UN_NAT_KICK_TIMEOUT: the minimum elapsed time between NAT kicks (2.0s). The
// X360 UpdateNATData builds a `Time(2.0f)` (flt_8207C624, resolved by Hex-Rays to
// 2.0) and only proceeds once `currentTime - mLastKickTime >= that timeout`.
// ===========================================================================

namespace BrnNetwork
{
    class BrnNetworkManager;

    class ConnectionManager
    {
    public:
        // @ 0x82544510 -- stash the owning network manager (`*(this+8) = lpNetworkManager`).
        void Construct(BrnNetworkManager* lpNetworkManager);

        // Prepare/Release/Destruct/Update did not run before the boot-trace milestone and
        // carry no recovered body in the available exports (the DWARF lists them as empty
        // shells). Declared for the class shape; bodied as trivial no-ops in the .cpp.
        bool Prepare();
        bool Release();
        void Destruct();
        void Update();

    private:
        // @ 0x8256CFF0 -- the per-frame NAT-kick policy walk (host only).
        void UpdateNATData();

        // @ 0x82566860 -- issue the NAT kick for liPlayerID through the game or userset
        // server-interface component (whichever the local player is currently in).
        void KickUnNATablePlayer(NetworkPlayerID liPlayerID);

        // The minimum spacing between NAT kicks, in seconds (DWARF
        // BrnNetworkConnectionManager.h:68). X360 builds a Time(2.0f) from it.
        static const f32 KF_UN_NAT_KICK_TIMEOUT;

        CgsSystem::Time    mLastKickTime;     // +0x00
        BrnNetworkManager* mpNetworkManager;  // +0x08
    };
}

#endif // BRN_NETWORK_CONNECTION_MANAGER_H
