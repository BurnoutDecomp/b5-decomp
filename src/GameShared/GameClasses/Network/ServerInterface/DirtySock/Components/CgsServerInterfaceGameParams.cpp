#include "CgsServerInterfaceGameParams.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"          // DirtySock::LobbyApiPlayT
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"   // EConversionFlags
#include "lobbytagfield.h"   // TagFieldSet* / TagFieldGetStructure

#include <string.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceGameParamsBase::IsRankedGame  @ 0x825413E8
//   CgsNetwork::ServerInterfaceGameParamsBase::SetName       @ 0x825411E8
//   CgsNetwork::ServerInterfaceGameParamsBase::SetPassword   @ 0x825809F0
//   CgsNetwork::ServerInterfaceGameParamsBase::SetSession    @ 0x825412E8
//   CgsNetwork::ServerInterfaceGameParamsBase::operator=     @ 0x825504C8
//
// ---------------------------------------------------------------------------
// The three string setters share one shape (only the destination buffer, its
// capacity and the assert limit differ). Each:
//   1. measures strlen(src)            (asm: `while ( *v4++ ) ;`)
//   2. if strlen >= capacity, fires the CgsStringUtils.h:55 "String too long"
//      assert. The X360 build streams "String too long: " + src into the assert
//      message buffer (CgsDev::Assert::gpcMessageBuffer) via StrStream before
//      calling FireAssert. FLAGGED: that StrStream message construction is a
//      logging side effect; it is modelled here through CGS_ASSERT (which logs
//      the stringized condition + file/line) rather than re-streaming the live
//      input, matching the established assert-machinery convention in this tree.
//   3. strncpy(dest, src, capacity)    (asm: `strncpy(this+off, src, N)`)
//      and returns the strncpy result (the destination pointer).
// ---------------------------------------------------------------------------

#define CGS_ARTIST_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfaceGameParams.cpp"

namespace CgsNetwork
{
    // IsRankedGame @ 0x825413E8.
    //
    // The asm builds and emits a multi-line debug report through the global
    // debug-message stream (off_82F335C8) before returning:
    //     "Game flags: " << muGameFlags << " = " << (hex) muGameFlags << "\n"
    //     "Game flags & Ranked: " << (hex) (muGameFlags & 0x400) << "\n"
    //     "Server Interface thinks we are: " << ("RANKED MATCH"|"PLAYER MATCH")
    //     "\n"
    // FLAGGED: that stream output is a pure logging side effect (the same
    // off_82F335C8 sink flagged in CgsMessage.cpp); it is not part of the
    // observable contract and is intentionally not reproduced. The observable
    // result is the ranked bit: `(muGameFlags >> 10) & 1`.
    bool ServerInterfaceGameParamsBase::IsRankedGame() const
    {
        return ((muGameFlags >> 10) & 1u) != 0u;
    }

    // operator= @ 0x825504C8 -- member-wise copy in the exact X360 store order:
    // the four char buffers (as contiguous byte runs +4/36, +40/20, +60/16,
    // +76/128) followed by the eleven scalar words (+204..+244). Reproduced here
    // as named-member assignments preserving that order.
    ServerInterfaceGameParamsBase&
    ServerInterfaceGameParamsBase::operator=(const ServerInterfaceGameParamsBase& lrOther)
    {
        memcpy(macName, lrOther.macName, sizeof(macName));             // +4, 36
        memcpy(macPassword, lrOther.macPassword, sizeof(macPassword)); // +40, 20
        memcpy(macHostName, lrOther.macHostName, sizeof(macHostName)); // +60, 16
        memcpy(macSession, lrOther.macSession, sizeof(macSession));    // +76, 128

        // +204 .. +244 : the eleven scalar words, in order.
        miGameID          = lrOther.miGameID;
        miRoomID          = lrOther.miRoomID;
        miMinNumPlayers   = lrOther.miMinNumPlayers;
        miMaxNumPlayers   = lrOther.miMaxNumPlayers;
        miNumPlayers      = lrOther.miNumPlayers;
        miNumPublicSlots  = lrOther.miNumPublicSlots;
        miNumPrivateSlots = lrOther.miNumPrivateSlots;
        muCustomFlags     = lrOther.muCustomFlags;
        muGameFlags       = lrOther.muGameFlags;
        mbJoinUserset     = lrOther.mbJoinUserset;
        muRandomSeed      = lrOther.muRandomSeed;

        return *this;
    }

    // The lobby game-flag word conversion (home: the game-flags TU).
    u32 ConvertFlags(u32 luFlags, EConversionFlags leDirection);

    // Reset every field to the empty default: no name / password / session / host, no
    // game or room id, eight public slots.
    bool ServerInterfaceGameParamsBase::Prepare()
    {
        SetName("");
        SetPassword("");
        SetSession("");
        macHostName[0]    = 0;
        miGameID          = -1;
        miRoomID          = -1;
        miMinNumPlayers   = 0;
        miMaxNumPlayers   = 0;
        miNumPlayers      = 0;
        miNumPublicSlots  = 8;
        miNumPrivateSlots = 0;
        muCustomFlags     = 0;
        muGameFlags       = 0;
        muRandomSeed      = 0;
        mbJoinUserset     = false;
        return true;
    }

    // Append the game's fields to a lobby request record; the derived structure (if any)
    // travels packed under "PARAMS".
    void ServerInterfaceGameParamsBase::SerialiseToString(char* lpcString, s32 liLength) const
    {
        const char* const lpcSource =
            "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\"
            "CgsServerInterfaceGameParams.cpp";
        CGS_ASSERT_AT(lpcString, "lpcString", lpcSource, 92);

        if (macName[0] != 0)
        {
            TagFieldSetString(lpcString, liLength, "NAME", macName);
        }
        TagFieldSetString(lpcString, liLength, "PASS", macPassword);
        if (mbJoinUserset)
        {
            TagFieldSetNumber(lpcString, liLength, "SET", 1);
        }

        if (GetData() != 0 && GetDataSize() != 0)
        {
            CGS_ASSERT_AT(strlen(GetPattern()) < static_cast<size_t>(GetPatternLength()),
                "strlen( GetPattern()) < (size_t) GetPatternLength()", lpcSource, 110);
            TagFieldSetStructure(lpcString, liLength, "PARAMS",
                                 GetData(), static_cast<s32>(GetDataSize()), GetPattern());
        }

        TagFieldSetNumber(lpcString, liLength, "MINSIZE", miMinNumPlayers);
        TagFieldSetNumber(lpcString, liLength, "MAXSIZE", miMaxNumPlayers);
        TagFieldSetNumber(lpcString, liLength, "CUSTFLAGS", static_cast<s32>(muCustomFlags));
        TagFieldSetNumber(lpcString, liLength, "SYSFLAGS",
                          static_cast<s32>(ConvertFlags(muGameFlags, E_FLAGS_DIRTYSOCK)));
        if (miRoomID != -1)
        {
            TagFieldSetNumber(lpcString, liLength, "ROOM", miRoomID);
        }
        if (miGameID != -1)
        {
            TagFieldSetNumber(lpcString, liLength, "IDENT", miGameID);
        }
        if (macSession[0] != 0)
        {
            TagFieldSetString(lpcString, liLength, "SESS", macSession);
        }
        TagFieldSetNumber(lpcString, liLength, "PRIV", miNumPrivateSlots);
        TagFieldSetNumber(lpcString, liLength, "SEED", static_cast<s32>(muRandomSeed));
        TagFieldSetNumber(lpcString, liLength, "FORCE_LEAVE", 1);
    }

    // Fill the fields from a lobby game record; the derived structure (if any) is
    // unpacked from the record's params text.
    void ServerInterfaceGameParamsBase::SerialiseFromGame(const void* lpGame)
    {
        CGS_ASSERT_AT(lpGame, "lpGame", CGS_ARTIST_FILE, 155 /* ARTIST 0x82877578 */);
        const DirtySock::LobbyApiPlayT* lpRecord = static_cast<const DirtySock::LobbyApiPlayT*>(lpGame);

        SetName(lpRecord->strName);
        SetPassword("");
        CgsCore::StrnCpy(macHostName, lpRecord->strHost, KI_GAMEPARAMS_HOSTNAME_LENGTH);
        SetSession(lpRecord->strSess);

        miGameID          = lpRecord->iIdent;
        miRoomID          = lpRecord->iRoom;
        miMinNumPlayers   = lpRecord->iMinsize;
        miMaxNumPlayers   = lpRecord->iMaxsize;
        miNumPlayers      = lpRecord->iCount;
        miNumPublicSlots  = lpRecord->iMaxsize - lpRecord->iPrivSlots;
        miNumPrivateSlots = lpRecord->iPrivSlots;
        muCustomFlags     = lpRecord->uCustflags;
        muRandomSeed      = lpRecord->uSeed;
        muGameFlags       = ConvertFlags(lpRecord->uSysflags, E_FLAGS_GAMESHARED);

        if (GetData() != 0 && GetDataSize() != 0)
        {
            CGS_ASSERT_AT(strlen(GetPattern()) < static_cast<size_t>(GetPatternLength()),
                          "strlen( GetPattern()) < (size_t) GetPatternLength()",
                          CGS_ARTIST_FILE, 175 /* ARTIST 0x82877798 */);
            TagFieldGetStructure(lpRecord->strParams, GetData(), GetDataSize(), GetPattern());
        }
    }

    void ServerInterfaceGameParamsBase::SetTotalSlots(s32 liNumPublicSlots, s32 liNumPrivateSlots)
    {
        miNumPublicSlots  = liNumPublicSlots;
        miNumPrivateSlots = liNumPrivateSlots;
    }
}

#undef CGS_ARTIST_FILE
