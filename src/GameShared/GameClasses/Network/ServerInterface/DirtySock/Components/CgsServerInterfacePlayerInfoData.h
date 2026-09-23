#ifndef CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_H
#define CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerInfoDataBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePlayerInfoData.{h,cpp}
//
// Player-info record received from the lobby (name, motto, location strings,
// id/rank/locality, the converted player-info + hardware flag sets, etc.).
// Derives from ServerInterfaceStructureInterface (vptr-only polymorphic base).
//
// LAYOUT (console offsets from SerialiseFromUser's stores; member names and order
// follow the reference declaration, each field matched to the lobby user field it
// is filled from):
//   +0x00  vptr
//   +0x04  macName[16]            <- user name
//   +0x14  macAuxiliaryData[132]  <- user aux
//   +0x98  macClubID[20]          <- user club id
//   +0xAC  macClubTag[8]          <- user club tag
//   +0xB4  macPing[8]             <- user ping
//   +0xBC  maColour[4]            <- user colour (4 bytes, copied byte-for-byte)
//   +0xC0  miIdent                <- user ident (0 -> -1)
//   +0xC4  miFlags                <- user flags, folded onto the player-info flags
//   +0xC8  miAttributes           <- user attr
//   +0xCC  miRank                 <- user rank (GetRank)
//   +0xD0  miGameID               <- user game
//   +0xD4  miReputation           <- user reputation
//   +0xD8  miUserSetID            <- user userset ident
//   +0xDC  muIPAddress            <- user addr
//   +0xE0  muLevel                <- user level
//   +0xE4  muMedals               <- user medals
//   +0xE8  muHwFlags              <- ConvertHWFlags(user hardware flags)
//   +0xEC  muLocalAddr            <- user local addr
//   +0xF0  muLocality             <- user locality (GetLocality)
// The create / modify game paths read +0xCC as the player's skill level and +0xF0
// as the host locality; the stats writer reads +0xCC through GetRank.
// ===========================================================================

namespace CgsNetwork
{
    // Player-info flag bits (CgsServerInterfacePlayerInfoData.h:50-66).
    const s32 KI_PLAYER_INFO_FLAG_ADMINISTRATOR     = 1;
    const s32 KI_PLAYER_INFO_FLAG_DETENTION         = 2;
    const s32 KI_PLAYER_INFO_FLAG_IN_GAME           = 4;
    const s32 KI_PLAYER_INFO_FLAG_ROOM_HOST         = 8;
    const s32 KI_PLAYER_INFO_FLAG_LOCKED            = 16;
    const s32 KI_PLAYER_INFO_FLAG_MODERATOR         = 32;
    const s32 KI_PLAYER_INFO_FLAG_NO_CHAT_FILTER    = 64;
    const s32 KI_PLAYER_INFO_FLAG_NO_PRIVATE_MESSAGE = 128;
    const s32 KI_PLAYER_INFO_FLAG_TEST_ACCOUNT      = 256;
    const s32 KI_PLAYER_INFO_FLAG_LOCAL_USER        = 512;
    const s32 KI_PLAYER_INFO_FLAG_ROOM_VICE_HOST    = 1024;
    const s32 KI_PLAYER_INFO_FLAG_AUDIT_SESSION     = 2048;
    const s32 KI_PLAYER_INFO_FLAG_TERMINATED        = 4096;
    const s32 KI_PLAYER_INFO_FLAG_ATTR0             = 8192;
    const s32 KI_PLAYER_INFO_FLAG_ATTR1             = 16384;
    const s32 KI_PLAYER_INFO_FLAG_ATTR2             = 32768;
    const s32 KI_PLAYER_INFO_FLAG_ATTR3             = 65536;

    // Hardware flag bits (CgsServerInterfacePlayerInfoData.h:68-74).
    const s32 KI_PLAYER_INFO_HW_FLAG_BROADBAND      = 1;
    const s32 KI_PLAYER_INFO_HW_FLAG_USB_ETHERNET   = 2;
    const s32 KI_PLAYER_INFO_HW_FLAG_USB_HEADSET    = 4;
    const s32 KI_PLAYER_INFO_HW_FLAG_USB_KEYBOARD   = 8;
    const s32 KI_PLAYER_INFO_HW_FLAG_PPPOE          = 16;
    const s32 KI_PLAYER_INFO_HW_FLAG_VOIP_USED      = 32;
    const s32 KI_PLAYER_INFO_HW_FLAG_VOIP_ACTIVE    = 64;

    // Conversion-table direction selector (homed with the Convert* family).
    enum EConversionFlags
    {
        E_CONVERSION_FROM_WIRE = 0,
        E_CONVERSION_TO_WIRE   = 1,
    };

    // Fold a lobby hardware-flag set onto the player-info hardware-flag space.
    //   Home: CgsServerInterfacePlayerInfoData.cpp:153
    s32 ConvertHWFlags(s32 liFlags, EConversionFlags leDirection);

    struct ServerInterfacePlayerInfoDataBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfacePlayerInfoDataBase();
        virtual ~ServerInterfacePlayerInfoDataBase();

        // CgsServerInterfacePlayerInfoData.cpp:167 (virtual; DWARF). Resets the whole
        // record to its empty default -- clears every string/attr buffer and scalar,
        // seeds miID to -1 -- ready to be filled from a lobby user. Returns true.
        virtual bool Prepare();

        // CgsServerInterfacePlayerInfoData.cpp (virtual; SerialiseFromUser fills the
        // record from a DirtySDK lobby user struct then parses the custom blob).
        virtual bool SerialiseFromUser(const void* lpUser);

        // CgsServerInterfacePlayerInfoData.h accessors.
        const char* GetName() const { return macName; }
        s32 GetID() const { return miIdent; }
        s32 GetRank() const { return miRank; }
        u32 GetLocality() const { return muLocality; }
        s32 GetGameID() const { return miGameID; }

    protected:
        char macName[16];            // +0x04
        char macAuxiliaryData[132];  // +0x14
        char macClubID[20];          // +0x98
        char macClubTag[8];          // +0xAC
        char macPing[8];             // +0xB4
        u8   maColour[4];            // +0xBC
        s32  miIdent;                // +0xC0
        s32  miFlags;                // +0xC4
        s32  miAttributes;           // +0xC8
        s32  miRank;                 // +0xCC
        s32  miGameID;               // +0xD0
        s32  miReputation;           // +0xD4
        s32  miUserSetID;            // +0xD8
        u32  muIPAddress;            // +0xDC
        u32  muLevel;                // +0xE0
        u32  muMedals;               // +0xE4
        u32  muHwFlags;              // +0xE8
        u32  muLocalAddr;            // +0xEC
        u32  muLocality;             // +0xF0
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_H
