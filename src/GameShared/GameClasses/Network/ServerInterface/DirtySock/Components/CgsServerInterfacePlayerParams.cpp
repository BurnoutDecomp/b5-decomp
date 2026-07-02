#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (over-length assert)
#include "lobbytagfield.h"                                    // DirtySDK TagFieldGet/Set*
#include <cstring>                                            // std::strlen / std::strncpy

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

namespace
{
    // The over-length guard inlined from CgsStringUtils.h:55 (StrnCpy bounds check).
    void FireStringTooLong(const char* lpcValue)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "String too long: " << (lpcValue ? lpcValue : "<NULLSTRING>");
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
            55);
        CgsDev::Assert::EndAssert();
    }
}

namespace CgsNetwork
{

void ServerInterfacePlayerParamsBase::SerialiseFromPlayer(const void* lpPlayer)
{
    const u8* lpcRec = static_cast<const u8*>(lpPlayer);

    // Machine address string lives at LobbyApiPlayerT + 0x1C; copy it into the
    // 64-byte machine-address buffer (inlined StrnCpy<64> bounds guard first).
    const char* lpcMachineAddr = reinterpret_cast<const char*>(lpcRec + 0x1C);
    if (std::strlen(lpcMachineAddr) >= 0x40u)
    {
        FireStringTooLong(lpcMachineAddr);
    }
    std::strncpy(macMachineAddress, lpcMachineAddr, 64);

    // mpcName points at the lobby record's name field (LobbyApiPlayerT + 4).
    mpcName           = reinterpret_cast<const char*>(lpcRec + 4);
    miIdent           = *reinterpret_cast<const s32*>(lpcRec + 0x00);
    miPresence        = *reinterpret_cast<const s32*>(lpcRec + 0x64);
    muExternalAddress = *reinterpret_cast<const u32*>(lpcRec + 0x14);
    muInternalAddress = *reinterpret_cast<const u32*>(lpcRec + 0x18);
    muFlags           = *reinterpret_cast<const u32*>(lpcRec + 0x60);

    // Bounds-check the serialisation pattern, then parse the custom structure blob
    // (held at LobbyApiPlayerT + 0x68) into the structure-interface data buffer.
    CGS_ASSERT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
               "strlen( GetPattern() ) < (size_t) GetPatternLength()");

    const char* lpcPattern = GetPattern();
    const u32   luDataSize = GetDataSize();
    // NOTE: the binary dispatches through vtable slot +0x14 here, i.e. the
    // const GetData() overload (not the non-const +0x10 one used elsewhere).
    const void* lpcData    = static_cast<const ServerInterfacePlayerParamsBase*>(this)->GetData();
    TagFieldGetStructure(reinterpret_cast<const char*>(lpcRec + 0x68),
                         const_cast<void*>(lpcData), static_cast<s32>(luDataSize), lpcPattern);
}

void ServerInterfacePlayerParamsBase::SerialiseToString(char* lpcRecord, s32 liRecLen) const
{
    CGS_ASSERT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
               "strlen( GetPattern() ) < (size_t) GetPatternLength()");

    const char* lpcPattern = GetPattern();
    const u32   luDataSize = GetDataSize();
    const void* lpData     = const_cast<ServerInterfacePlayerParamsBase*>(this)->GetData();
    TagFieldSetStructure(lpcRecord, liRecLen, "USERPARAMS",
                         lpData, static_cast<s32>(luDataSize), lpcPattern);

    TagFieldSetNumber(lpcRecord, liRecLen, "USERFLAGS", static_cast<s32>(muFlags));
}

}
