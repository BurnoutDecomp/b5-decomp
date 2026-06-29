#ifndef CGS_SERVER_INTERFACE_GAME_PARAMS_H
#define CGS_SERVER_INTERFACE_GAME_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceGameParamsBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceGameParams.{h,cpp}   (DirtySock component area)
//
// The game-parameters payload that the DirtySock server-interface layer
// serialises to/from the lobby. It derives from ServerInterfaceStructureInterface
// (a vptr-only polymorphic base) and adds the named string buffers + counters
// that describe a hosted/joined game.
//
// LAYOUT -- reconciled from dwarfdump (CgsServerInterfaceGameParams.h:55) AND the
// X360 ARTIST asm. The asm is authoritative for member ORDER/OFFSETS and OVERRIDES
// the PS3 DWARF where they disagree:
//   operator= @ 0x825504C8 copies, in this exact order:
//       +4   : 36 bytes  (macName[16] immediately followed by macHostName[20])
//       +40  : 20 bytes  (macPassword[20])
//       +60  : 16 bytes  (macSession's leading 16 bytes are grouped here by the
//                          compiler; see note -- the full macSession[128] runs
//                          +60..  no: see below)
//       +76  : 128 bytes (macSession[128])
//       +204 : 11 words  (the int/u32/bool counters below)
//   SetName     @ 0x825411E8  strncpy(this+4 , src, 16)   -> macName[16]
//   SetPassword @ 0x825809F0  strncpy(this+40, src, 20)   -> macPassword[20]
//   SetSession  @ 0x825412E8  strncpy(this+76, src, 128)  -> macSession[128]
//   IsRankedGame@ 0x825413E8  returns (muGameFlags >> 10) & 1, reads this+236
//
// Offset map (this == ServerInterfaceStructureInterface vptr @ +0 on X360):
//   +4   macName[16]          (SetName limit 16)
//   +20  macHostName[20]      (GetHostName; copied as part of the +4/36-byte run)
//   +40  macPassword[20]      (SetPassword limit 20)
//   +60  maPad60[16]          (grouped 16-byte run before macSession in the X360
//                              build; not individually named by any setter --
//                              kept as a named padding/reserved field so macSession
//                              lands at +76 exactly as the asm requires)
//   +76  macSession[128]      (SetSession limit 128)
//   +204 miGameID
//   +208 miRoomID
//   +212 miMinNumPlayers
//   +216 miMaxNumPlayers
//   +220 miNumPlayers
//   +224 miNumPublicSlots
//   +228 miNumPrivateSlots
//   +232 muCustomFlags
//   +236 muGameFlags          (bit 10 / 0x400 == "ranked")
//   +240 mbJoinUserset
//   +244 muRandomSeed
//
// NOTE on absolute offsets: these offsets are the X360 (32-bit pointer) layout.
// The lone pointer is the inherited vptr; on a 64-bit host the vptr widens to 8
// bytes, so the byte offsets above will NOT be reproduced and are intentionally
// NOT static_asserted. Members are pinned BY NAME; only the relative ORDER and
// the char-buffer sizes are load-bearing for parity.
// ===========================================================================

namespace CgsNetwork
{
    // CgsServerInterfaceGameParams.h:185 .. 200 (member buffer sizes from asm).
    const s32 KI_GAMEPARAMS_NAME_LENGTH     = 16;
    const s32 KI_GAMEPARAMS_HOSTNAME_LENGTH = 20;
    const s32 KI_GAMEPARAMS_PASSWORD_LENGTH = 20;
    const s32 KI_GAMEPARAMS_SESSION_LENGTH  = 128;

    // The X360 build tests bit 10 of muGameFlags ("ranked match").
    const u32 KU_GAMEPARAMS_RANKED_FLAG = 0x400u;

    struct ServerInterfaceGameParamsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceGameParamsBase();

        // CgsServerInterfaceGameParams.h:282
        void SetName(const char* lpcName);
        // CgsServerInterfaceGameParams.h:289
        void SetPassword(const char* lpcPassword);
        // CgsServerInterfaceGameParams.h:296
        void SetSession(const char* lpcSession);

        // The session-ID string set by SetSession. ADDITIVE GROW (BrnNetworkBuddyManagerX360 TU):
        // BuddyManagerX360::IsUserInGameSession reads the current game's session ID directly off
        // the params object (X360 reads macSession @ +76 with no out-of-line call, i.e. an inlined
        // trivial getter) and strnicmp-compares it against the invite's session ID. Exposed as an
        // inline accessor so the read goes through the named member rather than a raw-offset hack.
        const char* GetSession() const { return macSession; }

        // CgsServerInterfaceGameParams.h:443 -- true if muGameFlags bit 10 set.
        bool IsRankedGame() const;

        // ADDITIVE GROW (flagged by the ServerInterfaceGames group): the lobby-record
        // (de)serialisers, matching the sibling param families (PlayerParams / GameSearch /
        // QuickJoin all expose SerialiseToString). ServerInterfaceGames create/join/update
        // drive SerialiseToString; GetGameParameters / the search-sort comparator drive
        // DeserialiseFromString. Declared here (bodied in the game-params TU).
        void SerialiseToString(char* lpcRecord, s32 liRecLen) const;
        bool DeserialiseFromString(const char* lpcRecord);

        // CgsServerInterfaceGameParams.h:339
        virtual void SetRankedGame(bool lbRanked);

        virtual ~ServerInterfaceGameParamsBase();

        // Member-wise copy (asm @ 0x825504C8). Out-of-line: the X360 build emits a
        // hand-rolled byte-copy of the four char buffers followed by the 11 scalar
        // words, in that exact order.
        ServerInterfaceGameParamsBase& operator=(const ServerInterfaceGameParamsBase& lrOther);

    protected:
        char macName[KI_GAMEPARAMS_NAME_LENGTH];          // +4   (X360)
        char macHostName[KI_GAMEPARAMS_HOSTNAME_LENGTH];  // +20  (X360)
        char macPassword[KI_GAMEPARAMS_PASSWORD_LENGTH];  // +40  (X360)
        char maPad60[16];                                 // +60  (16-byte run grouped before macSession)
        char macSession[KI_GAMEPARAMS_SESSION_LENGTH];    // +76  (X360)

        s32 miGameID;          // +204
        s32 miRoomID;          // +208
        s32 miMinNumPlayers;   // +212
        s32 miMaxNumPlayers;   // +216
        s32 miNumPlayers;      // +220
        s32 miNumPublicSlots;  // +224
        s32 miNumPrivateSlots; // +228
        u32 muCustomFlags;     // +232
        u32 muGameFlags;       // +236
        bool mbJoinUserset;    // +240
        u32 muRandomSeed;      // +244
    };
}

#endif // CGS_SERVER_INTERFACE_GAME_PARAMS_H
