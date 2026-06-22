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
// LAYOUT (dwarfdump + X360 asm @ 0x828798A8 SerialiseFromUser). Offsets are the
// exact strncpy/store destinations the asm uses (this == r23):
//   +0x00  vptr
//   +0x04  macName[16]
//   +0x14  macMotto[132]
//   +0x98  macLocation[20]
//   +0xAC  macClanTag[8]
//   +0xB4  macTitleId[8]
//   +0xBC  maAttr[4]              (4 bytes, copied byte-for-byte)
//   +0xC0  miID                   (s32)   (0 -> -1)
//   +0xC4  muInfoFlags            (u32)   folded from lobby user flags
//   +0xC8  miRank                 (s32)
//   +0xCC  miLocality             (s32)
//   +0xD0  miGameID               (s32)
//   +0xD4  mi0xD4 / miField_D4     (s32)
//   +0xD8  mi0xD8                  (s32)
//   +0xDC  mi0xDC                  (s32)
//   +0xE0  mi0xE0                  (s32)
//   +0xE4  mi0xE4                  (s32)
//   +0xE8  muHWFlags              (u32)   ConvertHWFlags(...)
//   +0xEC  mi0xEC                  (s32)
//   +0xF0  mi0xF0                  (s32)
// The trailing scalar fields keep raw offset names (mi0x..) where the DWARF gives
// no descriptive name; their stores are reproduced verbatim from the asm.
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

        // CgsServerInterfacePlayerInfoData.cpp (virtual; SerialiseFromUser fills the
        // record from a DirtySDK lobby user struct then parses the custom blob).
        virtual bool SerialiseFromUser(const void* lpUser);

        // CgsServerInterfacePlayerInfoData.h accessors.
        const char* GetName() const { return macName; }
        s32 GetID() const { return miID; }
        s32 GetRank() const { return miRank; }
        u32 GetLocality() const { return static_cast<u32>(miLocality); }
        s32 GetGameID() const { return miGameID; }

    protected:
        char macName[16];        // +0x04
        char macMotto[132];      // +0x14
        char macLocation[20];    // +0x98
        char macClanTag[8];      // +0xAC
        char macTitleId[8];      // +0xB4
        u8   maAttr[4];          // +0xBC
        s32  miID;               // +0xC0
        u32  muInfoFlags;        // +0xC4
        s32  miRank;             // +0xC8
        s32  miLocality;         // +0xCC
        s32  miGameID;           // +0xD0
        s32  miField_D4;         // +0xD4
        s32  miField_D8;         // +0xD8
        s32  miField_DC;         // +0xDC
        s32  miField_E0;         // +0xE0
        s32  miField_E4;         // +0xE4
        u32  muHWFlags;          // +0xE8
        s32  miField_EC;         // +0xEC
        s32  miField_F0;         // +0xF0
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_INFO_DATA_H
