#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "lobbytagfield.h"                            // DirtySDK TagFieldSet*
#include <cstring>                                    // std::strlen, std::memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceGameSearchParamsBase::SerialiseToString @ 0x82878898
//
// Serialises a game-search request into a tagfield record. CgsNetwork::ConvertFlags
// (homed in CgsServerInterfaceGameFlags.cpp) maps engine game flags onto the wire
// "system" flag space; the EConversionFlags argument selects the conversion table.
// The custom-flags block is emitted only when the structure exposes a non-empty
// data/pattern blob (the GetData/GetDataSize/GetPattern virtuals), guarded by the
// strlen(GetPattern()) < GetPatternLength() bounds assert.

namespace CgsNetwork
{
    // CgsNetwork::EConversionFlags -- conversion-table selector (homed alongside
    // ConvertFlags in CgsServerInterfaceGameFlags.*). Only the "to wire" direction
    // (value 1) is referenced here; declared minimally so the call type-checks.
    enum EConversionFlags
    {
        E_CONVERSION_FROM_WIRE = 0,
        E_CONVERSION_TO_WIRE   = 1,
    };

    // Homed in CgsServerInterfaceGameFlags.cpp (another component TU).
    extern u32 ConvertFlags(u32 luFlags, EConversionFlags leDirection);
}

namespace CgsNetwork
{

ServerInterfaceGameSearchParamsBase::ServerInterfaceGameSearchParamsBase()
{
}

ServerInterfaceGameSearchParamsBase::~ServerInterfaceGameSearchParamsBase()
{
}

bool ServerInterfaceGameSearchParamsBase::Prepare()
{
    // Base virtual; the X360 leaf override below is the one bodied in this group.
    return false;
}

void ServerInterfaceGameSearchParamsBase::SerialiseToString(char* lpcRecord, s32 liRecLen) const
{
    CGS_ASSERT(miNumGames > 0, "Invalid number of games to search for");

    TagFieldSetNumber(lpcRecord, liRecLen, "START", 0);
    TagFieldSetNumber(lpcRecord, liRecLen, "COUNT", miNumGames);
    if (miRoomID != -1)
    {
        TagFieldSetNumber(lpcRecord, liRecLen, "ROOM", miRoomID);
    }
    TagFieldSetNumber(lpcRecord, liRecLen, "ASYNC", 1);
    TagFieldSetNumber(lpcRecord, liRecLen, "SYSMASK",
                      static_cast<s32>(ConvertFlags(muGameFlagsMask, E_CONVERSION_TO_WIRE)));
    TagFieldSetNumber(lpcRecord, liRecLen, "SYSFLAGS",
                      static_cast<s32>(ConvertFlags(muGameFlagsValue, E_CONVERSION_TO_WIRE)));
    if (mbReturnPlayers)
    {
        TagFieldSetNumber(lpcRecord, liRecLen, "PLAYERS", 1);
    }

    void* lpData = const_cast<ServerInterfaceGameSearchParamsBase*>(this)->GetData();
    if (lpData && GetDataSize())
    {
        CGS_ASSERT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
                   "strlen( GetPattern() ) < (size_t)GetPatternLength()");

        const char* lpcPattern = GetPattern();
        const u32   luDataSize = GetDataSize();
        lpData = const_cast<ServerInterfaceGameSearchParamsBase*>(this)->GetData();
        TagFieldSetStructure(lpcRecord, liRecLen, "CUSTOM",
                             lpData, static_cast<s32>(luDataSize), lpcPattern);
    }

    TagFieldSetNumber(lpcRecord, liRecLen, "CUSTMASK",
                      static_cast<s32>(GetCustomFlagsMask()));
    TagFieldSetNumber(lpcRecord, liRecLen, "CUSTFLAGS",
                      static_cast<s32>(GetCustomFlagsValue()));
}

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceGameSearchParamsX360::Prepare @ 0x82878B20
//       (called by BrnNetwork::GameSearchParamsBase::Prepare)
//
// Seeds the base fields then zeroes the X360-only payload. The asm store order:
//   stw 0   @+0x0C (muGameFlagsMask)   stw 10  @+0x04 (miNumGames)
//   stw -1  @+0x08 (miRoomID)          stw 0   @+0x10 (muGameFlagsValue)
//   stb 0   @+0x14 (mbReturnPlayers)   stw 0   @+0x68 (muX360Field_68)
//   memset(this+0x18, 0, 80)           return 1
ServerInterfaceGameSearchParamsX360::ServerInterfaceGameSearchParamsX360()
{
}

bool ServerInterfaceGameSearchParamsX360::Prepare()
{
    muGameFlagsMask  = 0;       // stw 0  @+0x0C
    miNumGames       = 10;      // stw 10 @+0x04
    miRoomID         = -1;      // stw -1 @+0x08
    muGameFlagsValue = 0;       // stw 0  @+0x10
    mbReturnPlayers  = false;   // stb 0  @+0x14
    muX360Field_68   = 0;       // stw 0  @+0x68
    std::memset(maX360Payload, 0, sizeof(maX360Payload));   // memset(this+0x18, 0, 80)
    return true;
}

}
