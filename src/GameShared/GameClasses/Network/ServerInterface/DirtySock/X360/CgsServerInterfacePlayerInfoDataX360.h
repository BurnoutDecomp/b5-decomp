#ifndef CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_X360_H
#define CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_X360_H

#include "types.hpp"
#include "../Components/CgsServerInterfacePlayerInfoData.h"   // ServerInterfacePlayerInfoDataBase

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerInfoDataX360
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/
//         CgsServerInterfacePlayerInfoDataX360.{h,cpp}
//
// The X360 (and PC-remaster) platform leaf of ServerInterfacePlayerInfoDataBase --
// the lobby player-info record. On top of the shared player-info block it adds the
// 8-byte Xbox secure host address (XUID) that peers exchange to reach one another,
// and overrides Prepare / SerialiseFromUser to additionally clear / decode that
// address. (Sibling shape: CgsServerInterfacePlayerParamsX360, which likewise adds
// a platform address blob and chains its overrides to the base.)
//
// Like the other X360 leaves in this tree (PlayerParamsX360 / UsersetParamsX360)
// this class does NOT implement the ServerInterfaceStructureInterface pure virtuals
// (GetPattern / GetPatternLength / GetDataSize / GetData); NONE are attested in the
// X360 image for this type, so none are fabricated and the class is left ABSTRACT.
//
// LAYOUT (X360 asm @ 0x82879E80 Prepare / 0x82879EE0 SerialiseFromUser):
//   the base ends at +0xF4 (miField_F0 at +0xF0); the 8-byte-aligned leaf member
//   lands at +0xF8, making the whole record 0x100 (256) bytes:
//     +0xF8  muXUID   (u64) -- secure host address decoded from the lobby user
//                             record (DirtyAddrToHostAddr, 8 bytes).
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfacePlayerInfoDataX360 : public ServerInterfacePlayerInfoDataBase
    {
    public:
        // @ X360 0x82879E80 -- chain to the base Prepare; on success clear the
        // 8-byte secure host address (asm: std r11=0, 0xF8) and return true.
        virtual bool Prepare();

        // @ X360 0x82879EE0 -- chain to the base, then decode the secure host
        // address out of the lobby user record (user +0x1D0) into muXUID via
        // DirtyAddrToHostAddr(&muXUID, 8, ...). Returns that call's result.
        virtual bool SerialiseFromUser(const void* lpUser);

        // The decoded 8-byte secure address / XUID. ADDITIVE GROW
        // (GamerPictureManagerX360 TU): BrnNetwork::GamerPictureManagerX360::AddPlayer
        // @ 0x825616B8 reads the filled record's +0xF8 whole (`ld r10, var_48(r1)` @
        // 0x8256184C) to key the gamer-picture download -- the inlined read of this
        // accessor (same inline-accessor idiom as the base's GetID/GetRank).
        u64 GetXUID() const { return muXUID; }

    protected:
        // CgsServerInterfacePlayerInfoDataX360 leaf member, +0xF8 (after the base
        // tail-pad); 8-byte-aligned (the asm touches it with std / an 8-byte len).
        u64 muXUID;
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_X360_H
