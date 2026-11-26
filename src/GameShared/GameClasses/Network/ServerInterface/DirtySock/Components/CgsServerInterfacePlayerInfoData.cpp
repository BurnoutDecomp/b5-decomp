#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert Begin/Fire/End
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::StrnCpy (over-length assert)
#include "lobbyapi.h"                                         // LobbyApiUserT
#include "lobbytagfield.h"                                    // DirtySDK TagFieldGetStructure
#include <cstring>                                            // std::strlen / std::strncpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ConvertHWFlags                                  @ 0x82879768
//   CgsNetwork::ServerInterfacePlayerInfoDataBase::Prepare           @ 0x828797E0
//   CgsNetwork::ServerInterfacePlayerInfoDataBase::SerialiseFromUser @ 0x828798A8
//
// The flag tables, their counts and the generic ConvertFlags follow the DWARF
// (CgsServerInterfacePlayerInfoData.cpp:35/55/57/67/92/153). Each table row pairs the
// player-info flag (column E_FLAGS_GAMESHARED) with the lobby flag (column E_FLAGS_DIRTYSOCK).

#define CGS_ARTIST_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfacePlayerInfoData.cpp"

namespace CgsNetwork
{
    // cpp:35, read at 0x820E9260
    const s32 KAAI_LOBBY_USER_FLAGS_TO_PLAYER_INFO_FLAGS[17][2] =
    {
        { 0x00000001, 0x00000002 },
        { 0x00000002, 0x00000010 },
        { 0x00000004, 0x00000080 },
        { 0x00000008, 0x00000100 },
        { 0x00000010, 0x00001000 },
        { 0x00000020, 0x00002000 },
        { 0x00000040, 0x00004000 },
        { 0x00000080, 0x00010000 },
        { 0x00000100, 0x00100000 },
        { 0x00000200, 0x00200000 },
        { 0x00000400, 0x00400000 },
        { 0x00000800, 0x02000000 },
        { 0x00001000, 0x04000000 },
        { 0x00002000, 0x08000000 },
        { 0x00004000, 0x10000000 },
        { 0x00008000, 0x20000000 },
        { 0x00010000, 0x40000000 },
    };

    // cpp:55
    const s32 KI_NUM_FLAGS = 17;

    // cpp:57, read at 0x820E92E8
    const s32 KAAI_HW_FLAGS_TO_PLAYER_INFO_FLAGS[7][2] =
    {
        { 0x00000001, 0x00000004 },
        { 0x00000002, 0x00000020 },
        { 0x00000004, 0x00000100 },
        { 0x00000008, 0x00000800 },
        { 0x00000010, 0x00010000 },
        { 0x00000020, 0x00200000 },
        { 0x00000040, 0x00400000 },
    };

    // cpp:67. The original's count is twice the seven-row table: ConvertHWFlags walks 14
    // rows (ARTIST 0x8287978C: seven passes of two rows), the last seven past the table.
    const s32 KI_NUM_HW_FLAGS = 14;

    // cpp:92: map every flag whose source-column bits are all set onto its target column.
    s32 ConvertFlags(s32 liFlags, EConversionFlags leConvertTo, const s32 (*laaiTable)[2], s32 liNumFlags)
    {
        const EConversionFlags leConvertFrom =
            (leConvertTo == E_FLAGS_GAMESHARED) ? E_FLAGS_DIRTYSOCK : E_FLAGS_GAMESHARED;

        s32 liNewFlags = 0;
        for (s32 i = 0; i < liNumFlags; ++i)
        {
            if ((laaiTable[i][leConvertFrom] & liFlags) == laaiTable[i][leConvertFrom])
            {
                liNewFlags |= laaiTable[i][leConvertTo];
            }
        }
        return liNewFlags;
    }

    // cpp:153
    s32 ConvertHWFlags(s32 liFlags, EConversionFlags leConvertTo)
    {
        return ConvertFlags(liFlags, leConvertTo, KAAI_HW_FLAGS_TO_PLAYER_INFO_FLAGS, KI_NUM_HW_FLAGS);
    }

bool ServerInterfacePlayerInfoDataBase::Prepare()
{
    // Zero the string / colour buffers, then reset each scalar in member order.
    // The ident starts at -1.
    std::memset(macName,          0, sizeof(macName));           // +0x04, 16
    std::memset(macAuxiliaryData, 0, sizeof(macAuxiliaryData));  // +0x14, 132
    std::memset(macClubID,        0, sizeof(macClubID));         // +0x98, 20
    std::memset(macClubTag,       0, sizeof(macClubTag));        // +0xAC, 8
    std::memset(macPing,          0, sizeof(macPing));           // +0xB4, 8

    maColour[0]  = 0;   // +0xBC
    maColour[1]  = 0;   // +0xBD
    maColour[2]  = 0;   // +0xBE
    maColour[3]  = 0;   // +0xBF

    miIdent      = -1;  // +0xC0
    miFlags      = 0;   // +0xC4
    miAttributes = 0;   // +0xC8
    miRank       = 0;   // +0xCC
    miGameID     = 0;   // +0xD0
    miReputation = 0;   // +0xD4
    miUserSetID  = 0;   // +0xD8
    muIPAddress  = 0;   // +0xDC
    muLevel      = 0;   // +0xE0
    muMedals     = 0;   // +0xE4
    muHwFlags    = 0;   // +0xE8
    muLocalAddr  = 0;   // +0xEC
    muLocality   = 0;   // +0xF0

    return true;
}

void ServerInterfacePlayerInfoDataBase::SerialiseFromUser(const LobbyApiUserT* lpUser)
{
    CGS_ASSERT_AT(lpUser != 0, "lpUser", CGS_ARTIST_FILE, 210 /* ARTIST 0x828798D4 */);

    CgsCore::StrnCpy(macName,          lpUser->name,       sizeof(macName));
    CgsCore::StrnCpy(macAuxiliaryData, lpUser->aux,        sizeof(macAuxiliaryData));
    CgsCore::StrnCpy(macClubID,        lpUser->strClubID,  sizeof(macClubID));
    CgsCore::StrnCpy(macClubTag,       lpUser->strClubTag, sizeof(macClubTag));
    CgsCore::StrnCpy(macPing,          lpUser->ping,       sizeof(macPing));

    maColour[0] = lpUser->Color.uRed;
    maColour[1] = lpUser->Color.uGreen;
    maColour[2] = lpUser->Color.uBlue;
    maColour[3] = lpUser->Color.uAlpha;

    if (lpUser->ident)
        miIdent = lpUser->ident;
    else
        miIdent = -1;
    miFlags      = ConvertFlags(lpUser->flags, E_FLAGS_GAMESHARED,
                                KAAI_LOBBY_USER_FLAGS_TO_PLAYER_INFO_FLAGS, KI_NUM_FLAGS);
    miAttributes = lpUser->attr;
    miRank       = lpUser->rank;
    miGameID     = lpUser->game;
    miReputation = lpUser->iReputation;
    miUserSetID  = lpUser->iUserSetIdent;
    muIPAddress  = lpUser->addr;
    muLevel      = lpUser->uLevel;
    muMedals     = lpUser->uMedals;
    muHwFlags    = ConvertHWFlags(lpUser->uHwFlags, E_FLAGS_GAMESHARED);
    muLocalAddr  = lpUser->uLocalAddr;
    muLocality   = lpUser->uLocality;

    // The custom structure blob, parsed only when the structure exposes data. The mutable
    // GetData() is vtable slot +0x14 (MSVC lays the two overloads out in reverse
    // declaration order), ARTIST 0x82879D90; arguments evaluated right to left.
    if (GetData())
    {
        CGS_ASSERT_AT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
                      "strlen( GetPattern() ) < (size_t) GetPatternLength()",
                      CGS_ARTIST_FILE, 249 /* ARTIST 0x82879E08 */);
        TagFieldGetStructure(lpUser->stat, GetData(), GetDataSize(), GetPattern());
    }
}

}

#undef CGS_ARTIST_FILE
