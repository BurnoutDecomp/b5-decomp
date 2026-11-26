#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert Begin/Fire/End
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::StrnCpy (over-length assert)
#include "lobbyapi.h"                                         // LobbyApiPlayerT
#include "lobbytagfield.h"                                    // DirtySDK TagFieldGet/Set*
#include <cstring>                                            // std::strlen / std::strncpy
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfacePlayerParams.cpp"
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePlayerParamsBase::SerialiseFromPlayer @ 0x82879F28
//   CgsNetwork::ServerInterfacePlayerParamsBase::SerialiseToString   @ 0x8287A120
//
// SerialiseFromPlayer copies the wire-side player record (a DirtySDK LobbyApiPlayerT)
// into the named members, then parses the custom per-player structure blob out of the
// record. SerialiseToString emits the user-params structure + the user flags into a
// tagfield record. Both guard the structure pattern with the
// strlen(GetPattern()) < GetPatternLength() bounds assert.
//
// FLAGGED: LobbyApiPlayerT has no reconstructed home; its fields are read by the
// documented byte offsets the X360 asm uses (verified store-for-store below).

// The StrnCpy over-length guard is CgsCore::StrnCpy's own (CgsStringUtils.h:53, assert
// at :55); a file-local copy of it collided with its siblings in CgsNetworkUnity.

namespace CgsNetwork
{

void ServerInterfacePlayerParamsBase::SerialiseFromPlayer(const LobbyApiPlayerT* lpPlayer)
{
    CgsCore::StrnCpy(macMachineAddress, lpPlayer->strMachineAddr, 64);

    mpcName           = lpPlayer->strPers;
    miIdent           = lpPlayer->iIdent;
    miPresence        = lpPlayer->iPresence;
    muExternalAddress = lpPlayer->uAddr;
    muInternalAddress = lpPlayer->uLocalAddr;
    muFlags           = lpPlayer->uFlags;

    // Bounds-check the serialisation pattern, then parse the custom structure blob
    // (held at LobbyApiPlayerT + 0x68) into the structure-interface data buffer.
    CGS_ASSERT_AT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
        "strlen( GetPattern() ) < (size_t) GetPatternLength()",
        CGS_ARTIST_FILE, 88 /* ARTIST 0x8287A0A8 */);

    // The mutable GetData() is vtable slot +0x14 (MSVC lays the two overloads out in
    // reverse declaration order), ARTIST 0x8287A0F8; arguments evaluated right to left.
    TagFieldGetStructure(lpPlayer->strParams, GetData(), GetDataSize(), GetPattern());
}

void ServerInterfacePlayerParamsBase::SerialiseToString(char* lpcRecord, s32 liRecLen) const
{
    CGS_ASSERT_AT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
        "strlen( GetPattern() ) < (size_t) GetPatternLength()",
        CGS_ARTIST_FILE, 108 /* ARTIST 0x8287A19C */);

    const char* lpcPattern = GetPattern();
    const u32   luDataSize = GetDataSize();
    const void* lpData     = const_cast<ServerInterfacePlayerParamsBase*>(this)->GetData();
    TagFieldSetStructure(lpcRecord, liRecLen, "USERPARAMS",
                         lpData, static_cast<s32>(luDataSize), lpcPattern);

    TagFieldSetNumber(lpcRecord, liRecLen, "USERFLAGS", static_cast<s32>(muFlags));
}

}
