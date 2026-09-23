#ifndef CGS_SERVER_INTERFACE_GAME_PARAMS_H
#define CGS_SERVER_INTERFACE_GAME_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"
#include "CgsServerInterfaceGameFlags.h"   // KU_GAME_FLAGS_PERSISTENT (SetFixedGame)

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
// LAYOUT (console offsets; member order follows the reference declaration):
//   operator= copies, in this exact order:
//       +4   : 36 bytes  (macName[36])
//       +40  : 20 bytes  (macPassword[20])
//       +60  : 16 bytes  (macHostName[16])
//       +76  : 128 bytes (macSession[128])
//       +204 : 11 words  (the int/u32/bool counters below)
//   SetName      strncpy(this+4 , src, 16)   (the 36-byte buffer is
//                                           only ever set 16 long)
//   SetPassword  strncpy(this+40, src, 20)   -> macPassword[20]
//   SetSession   strncpy(this+76, src, 128)  -> macSession[128]
//   IsRankedGame returns (muGameFlags >> 10) & 1, reads this+236
//   The found-game reader copies GetName() with a 36-byte limit and tests the
//   first byte of the host name at +60 for '@'.
//
// Offset map (this == ServerInterfaceStructureInterface vptr @ +0 on X360):
//   +4   macName[36]
//   +40  macPassword[20]
//   +60  macHostName[16]
//   +76  macSession[128]
//   +204 miGameID
//   +208 miRoomID
//   +212 miMinNumPlayers      (SetMinPlayers)
//   +216 miMaxNumPlayers      (SetMaxPlayers / GetMaxPlayers)
//   +220 miNumPlayers         (GetNumberOfPlayers)
//   +224 miNumPublicSlots
//   +228 miNumPrivateSlots
//   +232 muCustomFlags
//   +236 muGameFlags          (bit 10 / 0x400 == "ranked", bit 2 / 0x4 == fixed)
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
    // Member buffer sizes from the operator= copy runs; the name buffer is 36 bytes but
    // SetName limits the string to 16.
    const s32 KI_GAMEPARAMS_NAME_LENGTH        = 16;
    const s32 KI_GAMEPARAMS_NAME_BUFFER_LENGTH = 36;
    const s32 KI_GAMEPARAMS_HOSTNAME_LENGTH    = 16;
    const s32 KI_GAMEPARAMS_PASSWORD_LENGTH    = 20;
    const s32 KI_GAMEPARAMS_SESSION_LENGTH     = 128;

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

        // Inlined at the console call sites (create game, the found-game list).
        const char* GetName() const            { return macName; }
        const char* GetHostName() const        { return macHostName; }
        s32  GetNumberOfPlayers() const        { return miNumPlayers; }
        s32  GetMaxPlayers() const             { return miMaxNumPlayers; }
        u32  GetRandomSeed() const             { return muRandomSeed; }
        void SetMinPlayers(s32 liMinPlayers)   { miMinNumPlayers = liMinPlayers; }
        void SetMaxPlayers(s32 liMaxPlayers)   { miMaxNumPlayers = liMaxPlayers; }
        // The create-game path sets the fixed (persistent) game flag. FLAG: only the
        // set branch is attested; clearing on false follows SetRankedGame's shape.
        void SetFixedGame(bool lbFixedGame)
        {
            if (lbFixedGame)
                muGameFlags |= KU_GAME_FLAGS_PERSISTENT;
            else
                muGameFlags &= ~KU_GAME_FLAGS_PERSISTENT;
        }

        // CgsServerInterfaceGameParams.h:443 -- true if muGameFlags bit 10 set.
        bool IsRankedGame() const;

        // Store the public and private slot counts (miNumPublicSlots / miNumPrivateSlots).
        void SetTotalSlots(s32 liNumPublicSlots, s32 liNumPrivateSlots);

        // ADDITIVE GROW (flagged by the ServerInterfaceGames group): the lobby-record
        // (de)serialisers, matching the sibling param families (PlayerParams / GameSearch /
        // QuickJoin all expose SerialiseToString). ServerInterfaceGames create/join/update
        // drive SerialiseToString; GetGameParameters / the search-sort comparator drive
        // DeserialiseFromString. Declared here (bodied in the game-params TU).
        // SerialiseToString is virtual: the X360 ServerInterfaceGameParamsX360 leaf @0x828778E0
        // is a genuine vtable-slot override that pure-forwards to this base.
        virtual void SerialiseToString(char* lpcRecord, s32 liRecLen) const;
        bool DeserialiseFromString(const char* lpcRecord);

        // ADDITIVE GROW (ServerInterfaceGameParamsX360 TU): the base virtuals the X360 leaf
        // overrides. Prepare @0x82877810 seeds the ranked-context table; SerialiseFromGame
        // @0x828778E8 pushes the game's live state into the params record. Both are declared
        // here so the leaf's base-qualified calls + vtable overrides bind; base bodies live in
        // the base game-params TU (declared-not-defined at link time here is acceptable).
        virtual bool Prepare();
        virtual void SerialiseFromGame(const void* lpGame);

        // CgsServerInterfaceGameParams.h:339
        virtual void SetRankedGame(bool lbRanked);

        virtual ~ServerInterfaceGameParamsBase();

        // Member-wise copy (asm @ 0x825504C8). Out-of-line: the X360 build emits a
        // hand-rolled byte-copy of the four char buffers followed by the 11 scalar
        // words, in that exact order.
        ServerInterfaceGameParamsBase& operator=(const ServerInterfaceGameParamsBase& lrOther);

    protected:
        char macName[KI_GAMEPARAMS_NAME_BUFFER_LENGTH];   // +4
        char macPassword[KI_GAMEPARAMS_PASSWORD_LENGTH];  // +40
        char macHostName[KI_GAMEPARAMS_HOSTNAME_LENGTH];  // +60
        char macSession[KI_GAMEPARAMS_SESSION_LENGTH];    // +76

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
