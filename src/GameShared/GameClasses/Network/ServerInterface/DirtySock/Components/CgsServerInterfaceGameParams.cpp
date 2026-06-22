#include "CgsServerInterfaceGameParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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

namespace CgsNetwork
{
    ServerInterfaceGameParamsBase::ServerInterfaceGameParamsBase()
    {
    }

    ServerInterfaceGameParamsBase::~ServerInterfaceGameParamsBase()
    {
    }

    // SetName @ 0x825411E8 -- strncpy(this+4, src, 16); assert if strlen >= 16.
    void ServerInterfaceGameParamsBase::SetName(const char* lpcName)
    {
        CGS_ASSERT(strlen(lpcName) < static_cast<size_t>(KI_GAMEPARAMS_NAME_LENGTH),
                   "String too long");
        strncpy(macName, lpcName, KI_GAMEPARAMS_NAME_LENGTH);
    }

    // SetPassword @ 0x825809F0 -- strncpy(this+40, src, 20); assert if strlen >= 20.
    void ServerInterfaceGameParamsBase::SetPassword(const char* lpcPassword)
    {
        CGS_ASSERT(strlen(lpcPassword) < static_cast<size_t>(KI_GAMEPARAMS_PASSWORD_LENGTH),
                   "String too long");
        strncpy(macPassword, lpcPassword, KI_GAMEPARAMS_PASSWORD_LENGTH);
    }

    // SetSession @ 0x825412E8 -- strncpy(this+76, src, 128); assert if strlen >= 128.
    void ServerInterfaceGameParamsBase::SetSession(const char* lpcSession)
    {
        CGS_ASSERT(strlen(lpcSession) < static_cast<size_t>(KI_GAMEPARAMS_SESSION_LENGTH),
                   "String too long");
        strncpy(macSession, lpcSession, KI_GAMEPARAMS_SESSION_LENGTH);
    }

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
        // +4 / 36-byte run: macName[16] + macHostName[20].
        memcpy(macName, lrOther.macName, sizeof(macName));
        memcpy(macHostName, lrOther.macHostName, sizeof(macHostName));
        // +40 / 20-byte run.
        memcpy(macPassword, lrOther.macPassword, sizeof(macPassword));
        // +60 / 16-byte run.
        memcpy(maPad60, lrOther.maPad60, sizeof(maPad60));
        // +76 / 128-byte run.
        memcpy(macSession, lrOther.macSession, sizeof(macSession));

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
}
