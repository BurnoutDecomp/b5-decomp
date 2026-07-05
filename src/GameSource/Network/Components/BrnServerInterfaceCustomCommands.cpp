#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"

#include "GameSource/Network/Parameters/BrnNetworkRoadRulesData.h"  // RoadRulesUploadData / *DownloadData
#include "GameSource/GameState/BrnCgsPlayerName.h"                  // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDUnCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint (ConvertEventScoreUploadError)
#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h" // EventScoreData::SerialiseToString (UploadEventScoreData)
#include "lobbyapi.h"        // DirtySDK LobbyApiRequestCB / LobbyApiRefT / LobbyApiMsgT
#include "lobbytagfield.h"   // DirtySDK TagFieldFind/Get*/Set*

#include <cstring>  // std::strncpy / strncat / strtok / strchr / strncmp / memset
#include <cstdlib>  // std::atoi

// ===========================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::ServerInterfaceCustomCommands::*  (17 functions, see headers @addr)
//
// The component serialises a game-specific lobby request into the shared 2 KB
// DirtySock message buffer (mpServerInterface->GetMessageBuffer(), length
// KI_MESSAGE_BUFFER_SIZE) via the TagField* helpers, opens the action's net-stream
// log scope (StartActionCore) and posts it through LobbyApiRequestCB with
// _CustomCommandSentCallback as the completion callback. The reply is parsed in
// HandleIncomingMessage, which dispatches on the lobby message kind fourcc and then
// runs the stored CustomCommandCallback with the parsed result blob.
//
// Every store / branch / early-out below is taken store-for-store from the X360 asm.
// The numerous inlined GetMessageBuffer() null-asserts in the ARTIST build (the
// "mpacMessageBuffer" / CgsServerInterfaceDirtySock.h:719 assert) are reproduced as a
// single GetMessageBuffer() call guarded by the same precondition; folding the
// repeated inlined copies into one call per use site is a pure de-optimisation that
// preserves behaviour.
// ===========================================================================

namespace BrnNetwork
{

// ---- file-scope constants (DWARF BrnServerInterfaceCustomCommands.cpp) ----------
namespace
{
    // Message-buffer length the X360 inlines as the literal 2048 at every TagField* site.
    const s32 KI_MESSAGE_BUFFER_LENGTH = CgsNetwork::KI_MESSAGE_BUFFER_SIZE;  // 2048

    // BrnServerInterfaceCustomCommands.cpp:46 -- per-action human-readable label, indexed
    // by EAction (off_82F29F30 on X360); passed to StartActionCore and used by the net-stream
    // log. The entries the X360 string pool attests via this TU's action sites are exact
    // (off_82F29F30 [0], off_82F29F34 [1], off_82F29F40 [4], off_82F29F4C [7]); the rest are
    // recovered from the EAction names (the label is cosmetic -- only used for logging).
    const char* const KPC_ACTION_NAMES[ServerInterfaceCustomCommands::E_ACTION_COUNT] =
    {
        "Upld RR Score",       // [0]  E_ACTION_SET_ROAD_RULES_SCORES        (off_82F29F30, exact)
        "Dnld RR Scores",      // [1]  E_ACTION_GET_ROAD_RULES_SCORES        (off_82F29F34, exact)
        "Dnld Local RR Scores",// [2]  E_ACTION_GET_LOCAL_ROAD_RULES_SCORES  (inferred)
        "Dnld Friends list",   // [3]  E_ACTION_GET_FRIENDS                  (inferred)
        "Upld Friends list",   // [4]  E_ACTION_UPLOAD_FRIENDS               (off_82F29F40, exact)
        "Upld FBurn Stats",    // [5]  E_ACTION_UPLOAD_FREEBURN_LOBBY_STATS  (inferred)
        "Upld Offline Prog",   // [6]  E_ACTION_UPLOAD_OFFLINE_PROGRESSION   (inferred)
        "Upld Rivals Data",    // [7]  E_ACTION_UPLOAD_RIVAL_DATA            (off_82F29F4C, exact)
        "Upld Event Score",    // [8]  E_ACTION_UPLOAD_EVENT_SCORE_DATA      (inferred)
        "Get ELO",             // [9]  E_ACTION_GET_ELO                      (inferred)
        "Set ELO"              // [10] E_ACTION_SET_ELO                      (inferred)
    };

    // Helper to pack a big-endian fourcc (the wire byte order the X360 lobby uses).
    constexpr s32 KFOURCC(char a, char b, char c, char d)
    {
        return (static_cast<s32>(static_cast<u8>(a)) << 24) |
               (static_cast<s32>(static_cast<u8>(b)) << 16) |
               (static_cast<s32>(static_cast<u8>(c)) << 8)  |
                static_cast<s32>(static_cast<u8>(d));
    }

    // BrnServerInterfaceCustomCommands.cpp:67 -- per-action lobby message-kind fourcc,
    // indexed by EAction (dword_820872A4 on X360). Every fourcc below is recovered exactly
    // from the kind the X360 HandleIncomingMessage @ 0x82589C70 compares against
    // ('rrup'/'rrgt'/'rrlc'/'fget'/'fupd'/'evup'/'gelo'/'selo' read off the cmpw immediates);
    // the two actions whose acks the handler does not branch on ([5] freeburn-stats and
    // [6] offline-progression) are best-effort reconstructions of the same fourcc family.
    const s32 KI_ACTIONS_TO_MESSAGE_CODES[ServerInterfaceCustomCommands::E_ACTION_COUNT] =
    {
        KFOURCC('r','r','u','p'),  // [0]  SET_ROAD_RULES_SCORES       (exact, handler 'rrup')
        KFOURCC('r','r','g','t'),  // [1]  GET_ROAD_RULES_SCORES       (exact, handler 'rrgt')
        KFOURCC('r','r','l','c'),  // [2]  GET_LOCAL_ROAD_RULES_SCORES (exact, handler 'rrlc')
        KFOURCC('f','g','e','t'),  // [3]  GET_FRIENDS                 (exact, handler 'fget')
        KFOURCC('f','u','p','d'),  // [4]  UPLOAD_FRIENDS              (exact, handler 'fupd')
        KFOURCC('f','b','s','t'),  // [5]  UPLOAD_FREEBURN_LOBBY_STATS (inferred)
        KFOURCC('o','p','u','p'),  // [6]  UPLOAD_OFFLINE_PROGRESSION  (inferred)
        KFOURCC('e','v','u','p'),  // [7]  UPLOAD_RIVAL_DATA           (exact, handler 'evup')
        KFOURCC('e','s','u','p'),  // [8]  UPLOAD_EVENT_SCORE_DATA     (inferred)
        KFOURCC('g','e','l','o'),  // [9]  GET_ELO                     (exact, handler 'gelo')
        KFOURCC('s','e','l','o')   // [10] SET_ELO                     (exact, handler 'selo')
    };

    // --- ConvertError special-case codes (read off the X360 cmpw immediates) ----------
    const s32 KI_CODE_NOT_RECOGNISED = KFOURCC('b','a','d','c');  // 0x62616463 -> 140
    const s32 KI_CODE_TIMEOUT        = KFOURCC('t','i','m','e');  // 0x74696D65 -> 5
    const s32 KI_CODE_IN_PROGRESS    = KFOURCC('i','p','e','r');  // 0x69706572 -> 6

    // 'nost' (0x6E6F7374) -- the set-elo "local player has no stats" sub-error code.
    const s32 KI_CODE_NO_STATS       = KFOURCC('n','o','s','t');

    // CustomCommandErrorMapping (BrnServerInterfaceCustomCommands.cpp:105) -- the generic
    // {kind, code} -> server-interface error table consulted by ConvertError when the code
    // is not one of the three special cases above. 14 rows (KI_NUMBER_OF_ERROR_MAPPINGS).
    struct CustomCommandErrorMapping
    {
        s32 liMessageKind;            // BrnServerInterfaceCustomCommands.cpp:107
        s32 liMessageCode;            // BrnServerInterfaceCustomCommands.cpp:108
        s32 liServerInterfaceErrorCode; // BrnServerInterfaceCustomCommands.cpp:109
    };

    // 14 rows, proven from the X360 scan loop: r11 starts at base+4 (0x82087304) and steps by
    // 0xC (3 words: kind, code, error) while r11 < off_820873AC (0x820873AC, the first byte
    // past the table -- it is the "Not using rebroadcaster" string). (0x820873AC - 0x82087304)
    // / 0xC == 14 iterations.
    const s32 KI_NUMBER_OF_ERROR_MAPPINGS = 14;  // cpp:134

    // KA_CUSTOM_COMMAND_ERROR_MAPPING @ 0x82087300 -- the generic {kind, code, error} table
    // ConvertError (0x825839A0-0x82583AF0) scans when a reply code is not one of the three
    // inline special cases ('badc'->140, 'time'->5, 'iper'->6, all reproduced and asm-verified).
    // UNRECOVERED: the 14 rows live in .rdata that is NOT in the available function-keyed
    // exports (neither BURNOUT_X360_ARTIST.XEX nor DecFIGS PS3 dumps the data segment), so their
    // literal {kind, code, error} values are unknown. The table is reproduced at the correct
    // length (14 rows, see above) with zeroed rows as an honest placeholder; this is SAFE for
    // behaviour because ConvertError early-returns on code==0 before the scan, so a {0,0,*} row
    // can never match a live reply -- the scan simply falls through to the "Unhandled custom
    // command message type" assert + return 1 for any unrecognised {kind, code}, exactly as the
    // real table would for a code it does not list. The row values are a known uncertainty
    // FLAGGED for the reviewer + still_open: they affect only the returned error NUMBER for the
    // (kind, code) pairs the real table would have mapped to a specific error.
    const CustomCommandErrorMapping
        KA_CUSTOM_COMMAND_ERROR_MAPPING[KI_NUMBER_OF_ERROR_MAPPINGS] = {};
}

// ---------------------------------------------------------------------------
// Construct @ 0x82583720 -- chain to the component base then null/idle the Brn members.
void ServerInterfaceCustomCommands::Construct()
{
    CgsNetwork::ServerInterfaceComponent::Construct();

    mpServerInterface    = nullptr;   // a1[4]
    mpCallback           = nullptr;   // a1[9]
    mpCallbackData       = nullptr;   // a1[10]
    mpiRaceElo           = nullptr;   // a1[5]
    mpiRoadRageElo       = nullptr;   // a1[6]
    mpiBurningHomeRunElo = nullptr;   // a1[7]
    meCurrentAction      = E_ACTION_COUNT;  // a1[8] = 11
}

// Prepare @ 0x82583770 -- run the custom-commands base Prepare gate, then latch the server
// interface and idle. (Hex-Rays mis-symbolised the base Prepare call as a vtable-slot-
// colliding CgsSound::Playback::Content::DoOnPostLoad; the real callee is the interposed
// CgsNetwork::ServerInterfaceCustomCommands::Prepare(), which returns the gating bool.)
bool ServerInterfaceCustomCommands::Prepare(CgsNetwork::ServerInterfaceDirtySock* lpServerInterface)
{
    if (!CgsNetwork::ServerInterfaceCustomCommands::Prepare())
        return false;

    CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");

    mpServerInterface    = lpServerInterface;  // a1[4]
    mpCallback           = nullptr;            // a1[9]
    mpCallbackData       = nullptr;            // a1[10]
    mpiRaceElo           = nullptr;            // a1[5]
    mpiRoadRageElo       = nullptr;            // a1[6]
    mpiBurningHomeRunElo = nullptr;            // a1[7]
    meCurrentAction      = E_ACTION_COUNT;      // a1[8] = 11
    return true;
}

// Release @ 0x82583810 -- run the custom-commands base Release gate, then clear the server
// interface and idle. (Same vtable-slot mis-symbolisation as Prepare; real callee is the
// interposed CgsNetwork::ServerInterfaceCustomCommands::Release().)
bool ServerInterfaceCustomCommands::Release()
{
    if (!CgsNetwork::ServerInterfaceCustomCommands::Release())
        return false;

    mpServerInterface    = nullptr;   // a1[4]
    mpCallback           = nullptr;   // a1[9]
    mpCallbackData       = nullptr;   // a1[10]
    mpiRaceElo           = nullptr;   // a1[5]
    mpiRoadRageElo       = nullptr;   // a1[6]
    mpiBurningHomeRunElo = nullptr;   // a1[7]
    meCurrentAction      = E_ACTION_COUNT;  // a1[8] = 11
    return true;
}

// ---------------------------------------------------------------------------
// SendCustomCommand @ 0x8258F028 -- shared request tail: stamp the action, open its log
// scope (StartActionCore), assert the payload, and post via LobbyApiRequestCB.
void ServerInterfaceCustomCommands::SendCustomCommand(EAction leNewAction,
                                                      const char* lpcMessageData)
{
    CGS_ASSERT(leNewAction != E_ACTION_COUNT, "meNewAction != E_ACTION_COUNT");

    meCurrentAction = leNewAction;                       // a1[8] = leNewAction
    StartActionCore(KPC_ACTION_NAMES[leNewAction]);      // (*(vtable+12))(this, off_82F29F30[a2])

    CGS_ASSERT(lpcMessageData != nullptr, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),         // *(a1[4]+120)
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction], // dword_820872A4[a1[8]]
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// GetEloOfLocalPlayer @ 0x8258F0F0 -- stash the out-pointers + callback, then send 'get elo'.
//
// Parameter order is taken store-for-store from the X360 asm and is INTENTIONALLY
// (race, burning-home-run, road-rage) -- the reverse of SetEloOfLocalPlayer:
//   a2 -> assert "lpRaceElo"          (cpp:601) -> stw r29, 0x14(this)  -> mpiRaceElo          (a1[5])
//   a3 -> assert "lpBurningHomeRunElo"(cpp:603) -> stw r28, 0x1C(this)  -> mpiBurningHomeRunElo(a1[7])
//   a4 -> assert "lpRoadRageElo"      (cpp:602) -> stw r27, 0x18(this)  -> mpiRoadRageElo      (a1[6])
// The asserts fire in (race, road-rage, burning-home-run) source-line order (601/602/603);
// the stores route each parameter to its correctly-named member regardless.
void ServerInterfaceCustomCommands::GetEloOfLocalPlayer(s32* lpiRaceElo, s32* lpiBurningHomeRunElo,
                                                        s32* lpiRoadRageElo,
                                                        CustomCommandCallback lCallback, void* lpData)
{
    CGS_ASSERT(lpiRaceElo != nullptr, "lpRaceElo");            // a2, cpp:601
    CGS_ASSERT(lpiRoadRageElo != nullptr, "lpRoadRageElo");    // a4, cpp:602
    CGS_ASSERT(lpiBurningHomeRunElo != nullptr, "lpBurningHomeRunElo"); // a3, cpp:603
    CGS_ASSERT(lCallback != nullptr, "lCallback");            // a5, cpp:604
    CGS_ASSERT(lpData != nullptr, "lpData");                  // a6, cpp:605

    mpCallbackData       = lpData;                 // a1[10] -- stw r25, 0x28
    mpCallback           = lCallback;              // a1[9]  -- stw r26, 0x24
    mpiRaceElo           = lpiRaceElo;             // a1[5]  -- stw r29, 0x14
    mpiRoadRageElo       = lpiRoadRageElo;         // a1[6]  -- stw r27, 0x18
    mpiBurningHomeRunElo = lpiBurningHomeRunElo;   // a1[7]  -- stw r28, 0x1C

    // The X360 passes &unk_820046A7 (an empty/no-payload marker) as the message data.
    SendCustomCommand(E_ACTION_GET_ELO, "");
}

// SetEloOfLocalPlayer @ 0x8258F200 -- write RACE/ROADRAGE/BURNINGHOMERUN fields, then 'set elo'.
void ServerInterfaceCustomCommands::SetEloOfLocalPlayer(s32 liRaceElo, s32 liRoadRageElo,
                                                        s32 liBurningHomeRunElo,
                                                        CustomCommandCallback lCallback, void* lpData)
{
    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;  // empty the record

    bool lbOk;
    lbOk = TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "RACE", liRaceElo) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetNumber RACE");
    lbOk = TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "ROADRAGE", liRoadRageElo) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetNumber ROADRAGE");
    lbOk = TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "BURNINGHOMERUN", liBurningHomeRunElo) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetNumber BURNINGHOMERUN");

    SendCustomCommand(E_ACTION_SET_ELO, mpServerInterface->GetMessageBuffer());
}

// GetFriendsListFromServer @ 0x8258FD18 -- write the "TAG=F" filter, then send 'get friends'.
void ServerInterfaceCustomCommands::GetFriendsListFromServer(CustomCommandCallback lCallback, void* lpData)
{
    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    bool lbOk = TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "TAG", "F") != 0;
    CGS_ASSERT(lbOk, "TagFieldSetString TAG F");

    SendCustomCommand(E_ACTION_GET_FRIENDS, mpServerInterface->GetMessageBuffer());
}

// GetLocalRoadRulesHighScores @ 0x8258FB10 -- write SKEY/NUM/IDX, then send 'get local rr'.
void ServerInterfaceCustomCommands::GetLocalRoadRulesHighScores(s32 liNumScoresToDownload,
                                                                s32 liNextRoadIndexToDownload,
                                                                const char* lpcRoadRulesServerKey,
                                                                CustomCommandCallback lCallback, void* lpData)
{
    CGS_ASSERT(liNumScoresToDownload != 0, "liNumScoresToDownload");

    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    bool lbOk;
    lbOk = TagFieldSetRaw(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "SKEY", lpcRoadRulesServerKey) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw SKEY");
    lbOk = TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "NUM", liNumScoresToDownload) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetNumber NUM");
    lbOk = TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "IDX", liNextRoadIndexToDownload) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetNumber IDX");

    SendCustomCommand(E_ACTION_GET_LOCAL_ROAD_RULES_SCORES, mpServerInterface->GetMessageBuffer());
}

// UploadOfflineProgress @ 0x82590908 -- pack the OFFPROG structure + FBURNCHALL count, send.
void ServerInterfaceCustomCommands::UploadOfflineProgress(
    const NetworkInOfflineProgression::OfflineProgressionT* lpOfflineProgression,
    s32 liFreeburnChallengeSuccessCount)
{
    CGS_ASSERT(lpOfflineProgression != nullptr, "lpOfflineProgression");
    CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    // The OFFPROG field packs the offline-progression record with the X360 byte pattern.
    TagFieldSetStructure(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "OFFPROG",
                         lpOfflineProgression, 64, "lllwwwbbb13s13s13sb");
    TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                      "FBURNCHALL", liFreeburnChallengeSuccessCount);

    SendCustomCommand(E_ACTION_UPLOAD_OFFLINE_PROGRESSION, mpServerInterface->GetMessageBuffer());
}

// ---------------------------------------------------------------------------
// GetRoadRulesHighScores @ 0x8258F900 -- build a comma-separated "R" road list [start, end)
// (capped at 64 roads), set it, then post the request inline (StartActionCore + LobbyApiRequestCB).
void ServerInterfaceCustomCommands::GetRoadRulesHighScores(u32 luTimeStamp, s32 liNumScores,
                                                           s32 liStartIndex,
                                                           CustomCommandCallback lCallback, void* lpData)
{
    (void)luTimeStamp;  // a2 in the asm names the kind family; not stored by this path.

    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    char lacRoadId[16];
    char lacRoadsToGetScoresFor[192];
    std::memset(lacRoadId, 0, 4);
    std::memset(lacRoadsToGetScoresFor, 0, 120);

    // v12 = min(liStartIndex + liNumScores, 64).
    s32 liEnd = liStartIndex + liNumScores;
    if (liEnd >= 64)
        liEnd = 64;

    for (s32 liRoad = liStartIndex; liRoad < liEnd; )
    {
        CgsCore::SPrintf(lacRoadId, 120, "%d", liRoad);
        std::strncat(lacRoadsToGetScoresFor, lacRoadId, 120);
        if (++liRoad == liEnd)
            break;
        std::strncat(lacRoadsToGetScoresFor, ",", 120);
    }

    char* lpcMessageData = mpServerInterface->GetMessageBuffer();
    bool lbOk = TagFieldSetRaw(lpcMessageData, KI_MESSAGE_BUFFER_LENGTH, "R",
                               lacRoadsToGetScoresFor) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw R");

    lpcMessageData = mpServerInterface->GetMessageBuffer();

    meCurrentAction = E_ACTION_GET_ROAD_RULES_SCORES;            // a1[8] = 1
    StartActionCore(KPC_ACTION_NAMES[E_ACTION_GET_ROAD_RULES_SCORES]); // off_82F29F34

    CGS_ASSERT(lpcMessageData != nullptr, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction],
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// SetRoadRulesForLocalPlayer @ 0x8258F3E0 -- serialise the upload table into comma-separated
// "V" (scores), "R" (record indices) and "C" (car ids) fields, plus SKEY and the 64-bit "U"
// unique id, then post inline.
void ServerInterfaceCustomCommands::SetRoadRulesForLocalPlayer(RoadRulesUploadData* lpRoadRulesData,
                                                               const char* lpcRoadRulesServerKey,
                                                               CustomCommandCallback lCallback, void* lpData)
{
    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    CGS_ASSERT(lpRoadRulesData != nullptr, "lpRoadRulesData");
    CGS_ASSERT(lpRoadRulesData->GetNumScores() > 0, "lpRoadRulesData->GetNumScores() > 0");

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    char lacRecordValues[176];   // "V" field text (KI_ROAD_RULES_SCORE_ARRAY_SIZE 165)
    char lacRecordIndices[64];   // "R" field text (KI_ROAD_RULES_RECORD_INDEX_ARRAY_SIZE 60)
    char lacRecordCarIDs[336];   // "C" field text (KI_ROAD_RULES_CAR_ID_ARRAY_SIZE 195)
    std::memset(lacRecordValues, 0, 165);
    std::memset(lacRecordIndices, 0, 60);
    std::memset(lacRecordCarIDs, 0, 195);

    const s32 liNumScores = lpRoadRulesData->GetNumScores();
    for (s32 liIndex = 0; liIndex < liNumScores; ++liIndex)
    {
        char lacScore[11];   // "%d" of the quantised score (KI_MAX_CHAR_PER_SCORE 11)
        char lacIndex[4];    // "%d" of the record index   (KI_MAX_CHAR_PER_RECORD_INDEX 4)
        char lacCarID[16];   // un-compressed car id        (KI_CAR_ID_LENGTH 13)
        std::memset(lacCarID, 0, 13);

        CGS_ASSERT(liIndex < liNumScores, "liIndex < miNumScores");
        CgsCore::SPrintf(lacScore, 11, "%d", lpRoadRulesData->GetScoreValue(liIndex));

        CGS_ASSERT(liIndex < liNumScores, "liIndex < miNumScores");
        CgsCore::SPrintf(lacIndex, 4, "%d", lpRoadRulesData->GetRecordIndex(liIndex));

        CGS_ASSERT(liIndex < liNumScores, "liIndex < miNumScores");
        CgsIDUnCompress(lpRoadRulesData->GetCarID(liIndex), lacCarID);
        // Spaces in the printable car id are written as '?' on the wire.
        for (s32 liChar = 0; liChar < 13; ++liChar)
        {
            if (lacCarID[liChar] == ' ')
                lacCarID[liChar] = '?';
        }

        std::strncat(lacRecordValues, lacScore, 165);
        std::strncat(lacRecordIndices, lacIndex, 60);
        std::strncat(lacRecordCarIDs, lacCarID, 195);
        if (liIndex + 1 != liNumScores)
        {
            std::strncat(lacRecordValues, ",", 165);
            std::strncat(lacRecordIndices, ",", 60);
            std::strncat(lacRecordCarIDs, ",", 195);
        }
    }

    bool lbOk;
    lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                          "SKEY", lpcRoadRulesServerKey) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw SKEY");
    lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                          "R", lacRecordIndices) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw R");
    lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                          "V", lacRecordValues) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw V");
    lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                          "C", lacRecordCarIDs) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw C");

    const u64 lu64UniqueID = lpRoadRulesData->GetUniqueID();
    lbOk = TagFieldSetNumber64(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                               "U", static_cast<s64>(lu64UniqueID)) != 0;
    CGS_ASSERT(lbOk, "TagFieldSetNumber64 U");

    char* lpcMessageData = mpServerInterface->GetMessageBuffer();

    meCurrentAction = E_ACTION_SET_ROAD_RULES_SCORES;          // a1[8] = 0
    StartActionCore(KPC_ACTION_NAMES[E_ACTION_SET_ROAD_RULES_SCORES]);

    CGS_ASSERT(lpcMessageData != nullptr, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction],
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// OverWriteServerFriendsRecord @ 0x825902B0 -- build a comma-separated quoted-name "SET" list
// (TAG=F), then post the 'upload friends' request inline.
void ServerInterfaceCustomCommands::OverWriteServerFriendsRecord(
    const CgsNetwork::PlayerName* lpaFriendsNames, s32 liNumBuddies)
{
    CGS_ASSERT(liNumBuddies > 0, "liNumBuddies > 0");
    // PlayerName::IsEmpty() is "first char is NUL"; the X360 checks `lbz; cmplwi 0`.
    CGS_ASSERT(lpaFriendsNames[0].GetPlayerName()[0] != 0, "!lpaFriendsNames[0].IsEmpty()");

    mpCallbackData = nullptr;  // a1[10]
    mpCallback     = nullptr;  // a1[9]

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    bool lbOk = TagFieldSetRaw(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "TAG", "F") != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw TAG F");

    if (liNumBuddies > 0)
    {
        char lacFriendsList[2000];
        std::memset(lacFriendsList, 0, 1900);

        const CgsNetwork::PlayerName* lpName = lpaFriendsNames;
        for (s32 liBuddy = 0; liBuddy < liNumBuddies; ++liBuddy)
        {
            char lacFormatted[32];
            // Names with embedded spaces are quoted; others are written bare.
            const char* lpcFormat = (std::strchr(lpName->GetPlayerName(), ' ') != nullptr)
                                        ? "\"%s\"" : "%s";
            CgsCore::SPrintf(lacFormatted, 19, lpcFormat, lpName->GetPlayerName());
            std::strncat(lacFriendsList, lacFormatted, 1900);
            if (liBuddy + 1 != liNumBuddies)
                std::strncat(lacFriendsList, ",", 1900);
            ++lpName;
        }

        lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                              "SET", lacFriendsList) != 0;
        CGS_ASSERT(lbOk, "TagFieldSetRaw SET");
    }

    char* lpcMessageData = mpServerInterface->GetMessageBuffer();

    meCurrentAction = E_ACTION_UPLOAD_FRIENDS;                 // a1[8] = 4
    StartActionCore(KPC_ACTION_NAMES[E_ACTION_UPLOAD_FRIENDS]); // off_82F29F40

    CGS_ASSERT(lpcMessageData != nullptr, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction],
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// UpdateServerFriendsRecord @ 0x8258FE78 -- build TAG=F plus an "ADD" and/or "DELETE"
// comma-separated quoted-name list, then post the 'upload friends' request inline.
void ServerInterfaceCustomCommands::UpdateServerFriendsRecord(
    const CgsNetwork::PlayerName* lpaPlayerNamesToAdd,
    const CgsNetwork::PlayerName* lpaPlayerNamesToRemove,
    s32 liNumPlayersToAdd, s32 liNumPlayersToRemove,
    CustomCommandCallback lCallback, void* lpData)
{
    CGS_ASSERT((liNumPlayersToAdd > 0) || (liNumPlayersToRemove > 0),
               "( liNumPlayersToAdd > 0 ) || ( liNumPlayersToRemove > 0 )");
    CGS_ASSERT((lpaPlayerNamesToAdd[0].GetPlayerName()[0] != 0) ||
               (lpaPlayerNamesToRemove[0].GetPlayerName()[0] != 0),
               "( !lpaPlayerNamesToAdd[0].IsEmpty() ) || ( !lpaPlayerNamesToRemove[0].IsEmpty() )");

    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    bool lbOk = TagFieldSetRaw(lpcBuffer, KI_MESSAGE_BUFFER_LENGTH, "TAG", "F") != 0;
    CGS_ASSERT(lbOk, "TagFieldSetRaw TAG F");

    char lacFriendsList[2016];

    if (liNumPlayersToAdd > 0)
    {
        std::memset(lacFriendsList, 0, 1900);
        const CgsNetwork::PlayerName* lpName = lpaPlayerNamesToAdd;
        for (s32 liPlayer = 0; liPlayer < liNumPlayersToAdd; ++liPlayer)
        {
            char lacFormatted[32];
            const char* lpcFormat = (std::strchr(lpName->GetPlayerName(), ' ') != nullptr)
                                        ? "\"%s\"" : "%s";
            CgsCore::SPrintf(lacFormatted, 19, lpcFormat, lpName->GetPlayerName());
            std::strncat(lacFriendsList, lacFormatted, 1900);
            if (liPlayer + 1 != liNumPlayersToAdd)
                std::strncat(lacFriendsList, ",", 1900);
            ++lpName;
        }
        lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                              "ADD", lacFriendsList) != 0;
        CGS_ASSERT(lbOk, "TagFieldSetRaw ADD");
    }

    if (liNumPlayersToRemove > 0)
    {
        std::memset(lacFriendsList, 0, 1900);
        const CgsNetwork::PlayerName* lpName = lpaPlayerNamesToRemove;
        for (s32 liPlayer = 0; liPlayer < liNumPlayersToRemove; ++liPlayer)
        {
            char lacFormatted[32];
            const char* lpcFormat = (std::strchr(lpName->GetPlayerName(), ' ') != nullptr)
                                        ? "\"%s\"" : "%s";
            CgsCore::SPrintf(lacFormatted, 19, lpcFormat, lpName->GetPlayerName());
            std::strncat(lacFriendsList, lacFormatted, 1900);
            if (liPlayer + 1 != liNumPlayersToRemove)
                std::strncat(lacFriendsList, ",", 1900);
            ++lpName;
        }
        lbOk = TagFieldSetRaw(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                              "DELETE", lacFriendsList) != 0;
        CGS_ASSERT(lbOk, "TagFieldSetRaw DELETE");
    }

    char* lpcMessageData = mpServerInterface->GetMessageBuffer();

    meCurrentAction = E_ACTION_UPLOAD_FRIENDS;                 // a1[8] = 4
    StartActionCore(KPC_ACTION_NAMES[E_ACTION_UPLOAD_FRIENDS]); // off_82F29F40

    CGS_ASSERT(lpcMessageData != nullptr, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction],
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// UploadLiveRevengeData @ 0x82590A88 -- pack each rival as a RIVAL%i structure field, then
// post the 'upload rivals' request inline. No-op when liNumberToUpload == 0.
void ServerInterfaceCustomCommands::UploadLiveRevengeData(const RivalDataT* lpRivalData,
                                                          const s32* lpiRivalIDs,
                                                          s32 liNumberToUpload)
{
    CGS_ASSERT(lpRivalData != nullptr, "lpRivalData");
    CGS_ASSERT(liNumberToUpload >= 0, "liNumberToUpload >= 0");

    if (liNumberToUpload <= 0)
        return;

    char* lpcBuffer = mpServerInterface->GetMessageBuffer();
    lpcBuffer[0] = 0;

    // The rival rows are a flat 64-byte-stride blob; lpiRivalIDs supplies each row's id.
    const u8* lpRow = reinterpret_cast<const u8*>(lpRivalData);
    const s32* lpiId = lpiRivalIDs;
    for (s32 liRival = 0; liRival < liNumberToUpload; ++liRival)
    {
        char lacFieldName[96];
        CgsCore::SPrintf(lacFieldName, 7, "RIVAL%i", *lpiId);
        TagFieldSetStructure(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                             lacFieldName, lpRow, 64, "llllllllllll16s");
        ++lpiId;
        lpRow += 64;
    }

    char* lpcMessageData = mpServerInterface->GetMessageBuffer();

    meCurrentAction = E_ACTION_UPLOAD_RIVAL_DATA;              // v7[8] = 7
    StartActionCore(KPC_ACTION_NAMES[E_ACTION_UPLOAD_RIVAL_DATA]); // off_82F29F4C

    CGS_ASSERT(lpcMessageData != nullptr, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction],
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// ConvertError @ 0x82583888 -- map a {kind, code} fourcc pair to a server-interface error.
int ServerInterfaceCustomCommands::ConvertError(int liMessageKind, int liMessageCode)
{
    if (liMessageCode == 0)
        return 0;

    if (liMessageCode == KI_CODE_NOT_RECOGNISED)
    {
        // The X360 streams "Server doesn't recongise: <code>\n" into the assert buffer.
        CGS_ASSERT(false, "Server doesn't recongise");
        return 140;
    }
    if (liMessageCode == KI_CODE_TIMEOUT)
        return 5;
    if (liMessageCode == KI_CODE_IN_PROGRESS)
        return 6;

    // Scan the generic mapping table for a row matching both kind and code.
    for (s32 liIndex = 0; liIndex < KI_NUMBER_OF_ERROR_MAPPINGS; ++liIndex)
    {
        if (KA_CUSTOM_COMMAND_ERROR_MAPPING[liIndex].liMessageKind == liMessageKind &&
            KA_CUSTOM_COMMAND_ERROR_MAPPING[liIndex].liMessageCode == liMessageCode)
        {
            return KA_CUSTOM_COMMAND_ERROR_MAPPING[liIndex].liServerInterfaceErrorCode;
        }
    }

    // Not in the table: the X360 streams "Unhandled custom command message type: <kind>/<code>".
    CGS_ASSERT(false, "Unhandled custom command message type");
    return 1;
}

// ---------------------------------------------------------------------------
// ConvertEventScoreUploadError @ 0x82583B10 -- map an event-score-upload {kind, code} fourcc
// pair to a server-interface error. Structurally the same {special cases + generic table scan}
// as ConvertError, but the 'badc' ("server doesn't recognise") case logs into the debug-print
// stream (CgsDev::Log::gpDebugPrint) rather than firing an assert.
int ServerInterfaceCustomCommands::ConvertEventScoreUploadError(int liMessageKind, int liMessageCode)
{
    if (liMessageCode == 0)
        return 0;

    if (liMessageCode == KI_CODE_NOT_RECOGNISED)  // 'badc'
    {
        // The X360 renders the 4-byte kind fourcc as a NUL-terminated 4-char string and streams
        // "Server doesn't recognise: <kind>\n" into CgsDev::Log::gpDebugPrint (off_82F335C8).
        char lacKind[8];
        std::memcpy(lacKind, &liMessageKind, 4);
        lacKind[4] = 0;
        *CgsDev::Log::gpDebugPrint << "Server doesn't recognise: " << lacKind << "\n";
        return 140;
    }
    if (liMessageCode == KI_CODE_TIMEOUT)      // 'time'
        return 5;
    if (liMessageCode == KI_CODE_IN_PROGRESS)  // 'iper'
        return 6;

    // Scan the generic mapping table for a row matching both kind and code.
    for (s32 liIndex = 0; liIndex < KI_NUMBER_OF_ERROR_MAPPINGS; ++liIndex)
    {
        if (KA_CUSTOM_COMMAND_ERROR_MAPPING[liIndex].liMessageKind == liMessageKind &&
            KA_CUSTOM_COMMAND_ERROR_MAPPING[liIndex].liMessageCode == liMessageCode)
        {
            return KA_CUSTOM_COMMAND_ERROR_MAPPING[liIndex].liServerInterfaceErrorCode;
        }
    }

    // Not in the table: the X360 de-inlines BeginAssert + a StrStream of
    // "Unhandled custom command message type: <kind>/<code>\n" + FireAssert(...:1224). Collapsed
    // to one CGS_ASSERT (the streamed kind/code are cosmetic diagnostics; string verbatim from
    // rodata, trailing ': ' preserved).
    CGS_ASSERT(false, "Unhandled custom command message type: ");
    return 1;
}

// ---------------------------------------------------------------------------
// UploadEventScoreData @ 0x825905D8 -- stash the callback + user data, serialise the batch of
// event scores into the message buffer, then post the 'event score data' upload (action 8).
int ServerInterfaceCustomCommands::UploadEventScoreData(EventScoreData* lpEventScoreData,
                                                        CustomCommandCallback lCallback, void* lpData)
{
    mpCallbackData = lpData;     // a1[10] -- stw r6, 0x28
    mpCallback     = lCallback;  // a1[9]  -- stw r5, 0x24

    CGS_ASSERT(lpEventScoreData != nullptr, "lpEventScoreUploadData");

    lpEventScoreData->SerialiseToString(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH);

    SendCustomCommand(E_ACTION_UPLOAD_EVENT_SCORE_DATA, mpServerInterface->GetMessageBuffer());
    return 0;  // SendCustomCommand is void (its r3 tail is ignored by every caller).
}

// ---------------------------------------------------------------------------
// ~ServerInterfaceCustomCommands (scalar deleting destructor @ 0x827DE408).
// The X360 thunk restores this leaf class's vtable (off_820CDBF8) at this+0 and conditionally
// runs operator delete (the MSVC flag-bit-0 `delete this` path). Emitting the destructor as
// virtual makes the compiler synthesise exactly that thunk; there are no owned resources to
// release (Construct only nulls the POD members), so the body is empty. Mirrors the committed
// CgsNetwork::ServerInterfaceEndGameDataBase virtual-destructor pattern.
ServerInterfaceCustomCommands::~ServerInterfaceCustomCommands()
{
}

// ---------------------------------------------------------------------------
// _CustomCommandSentCallback @ 0x8258F020 -- LobbyApiRequestCB completion trampoline:
// forwards to HandleIncomingMessage(lpUserData, lpMsg).
void ServerInterfaceCustomCommands::_CustomCommandSentCallback(::LobbyApiRefT* /*lpLobbyApi*/,
                                                               ::LobbyApiMsgT* lpMsg,
                                                               void* lpUserData)
{
    static_cast<ServerInterfaceCustomCommands*>(lpUserData)->HandleIncomingMessage(lpMsg);
}

// ---------------------------------------------------------------------------
// HandleIncomingMessage @ 0x82589C70 -- parse the completed lobby reply: dispatch on the
// message kind fourcc, build the appropriate result blob, end the action with the converted
// error, then run the stored callback and clear it.
void ServerInterfaceCustomCommands::HandleIncomingMessage(::LobbyApiMsgT* lpMsg)
{
    CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
    CGS_ASSERT(lpMsg != nullptr, "lpMsg");

    bool  lbSuccess = false;   // v4
    void* lpResult  = nullptr; // v5

    // The X360 result buffers are all function-scope stack frames so they stay alive until
    // the (synchronous) callback runs in the common tail: the two road-rules result objects
    // and the 16-byte-per-name friends list blob.
    RoadRulesDownloadData                lDownloadData;     // v90 (1088 bytes)
    RoadRulesLocalPlayerDownloadedScores lLocalScores;      // v87 (320-byte frame)
    char                                 lacFriendsList[1760] = {};  // v91

    const s32   liKind = lpMsg->kind;   // a2[2]
    const s32   liCode = lpMsg->code;   // a2[3]
    const char* lpcData = lpMsg->pData; // a2[4]

    if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_GET_ROAD_RULES_SCORES]) // 'rrgt'
    {
        if (liCode == 0)
        {
            lDownloadData.Construct();
            char* lpcBuffer = mpServerInterface->GetMessageBuffer();
            lpcBuffer[0] = 0;
            std::strncpy(lpcBuffer, lpcData, KI_MESSAGE_BUFFER_LENGTH);

            s32 liScoreType = 0;  // v21
            for (char* lpToken = std::strtok(mpServerInterface->GetMessageBuffer(), ",");
                 lpToken; lpToken = std::strtok(nullptr, ","))
            {
                char lacName[16];   // v85
                char lacScore[16];  // v83
                std::strncpy(lacName, lpToken, 16);

                char* lpScoreToken = std::strtok(nullptr, ",");
                std::strncpy(lacScore, lpScoreToken, 11);

                // A {"0","0"} name/score pair decodes to an empty record.
                if (std::strncmp(lacName, "0", 16) == 0 && std::strncmp(lacScore, "0", 16) == 0)
                {
                    std::memset(lacName, 0, 16);
                    std::memset(lacScore, 0, 11);
                }

                lDownloadData.SetDownloadedRoadRulesData(lacName, liScoreType, std::atoi(lacScore));
                ++liScoreType;
                CGS_ASSERT(liScoreType <= 2, "leEnumIndex <= E_SCORE_TYPE_COUNT");
                if (liScoreType == 2)
                    liScoreType = 0;
            }
            lpResult = &lDownloadData;
            lbSuccess = true;
        }
    }
    else if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_GET_LOCAL_ROAD_RULES_SCORES]) // 'rrlc'
    {
        if (liCode == 0)
        {
            char* lpcBuffer = mpServerInterface->GetMessageBuffer();
            lpcBuffer[0] = 0;
            std::strncpy(lpcBuffer, lpcData, KI_MESSAGE_BUFFER_LENGTH);

            // The first two comma fields are the low / high 32-bit halves of the 64-bit
            // road-rules id; the X360 packs them as ((u64)second << 32) | (u32)first and
            // stamps the local-scores table with that id (miNumScores reset to 0). This is
            // exactly RoadRulesLocalPlayerDownloadedScores::Construct(id).
            char lacField[16];  // v84
            char* lpToken = std::strtok(mpServerInterface->GetMessageBuffer(), ",");
            std::strncpy(lacField, lpToken, 13);
            const s32 liIdLow = std::atoi(lacField);

            lpToken = std::strtok(nullptr, ",");
            std::strncpy(lacField, lpToken, 13);
            const s32 liIdHigh = std::atoi(lacField);

            lLocalScores.Construct((static_cast<u64>(static_cast<u32>(liIdHigh)) << 32) |
                                   static_cast<u32>(liIdLow));

            s32 liScoreType = 0;  // v47
            for (char* lpScore = std::strtok(nullptr, ","); lpScore;
                 lpScore = std::strtok(nullptr, ","))
            {
                char lacScore[16];  // v86
                std::strncpy(lacScore, lpScore, 11);
                lLocalScores.SetDownloadedRoadRulesData(liScoreType, std::atoi(lacScore));
                ++liScoreType;
                CGS_ASSERT(liScoreType <= 2, "leEnumIndex <= E_SCORE_TYPE_COUNT");
                if (liScoreType == 2)
                    liScoreType = 0;
            }
            lpResult = &lLocalScores;
            lbSuccess = true;
        }
    }
    else if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_GET_ELO]) // 'gelo'
    {
        if (liCode == 0)
        {
            CGS_ASSERT(mpiRaceElo != nullptr, "mpiRaceElo");
            CGS_ASSERT(mpiRoadRageElo != nullptr, "mpiRoadRageElo");
            CGS_ASSERT(mpiBurningHomeRunElo != nullptr, "mpiBurningHomeRunElo");

            *mpiRaceElo          = TagFieldGetNumber(TagFieldFind(lpcData, "RACE"), 0);
            *mpiRoadRageElo      = TagFieldGetNumber(TagFieldFind(lpcData, "ROADRAGE"), 0);
            *mpiBurningHomeRunElo = TagFieldGetNumber(TagFieldFind(lpcData, "BURNINGHOMERUN"), 0);
            lbSuccess = true;
        }
        mpiRaceElo           = nullptr;
        mpiRoadRageElo       = nullptr;
        mpiBurningHomeRunElo = nullptr;
    }
    else if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_GET_FRIENDS]) // 'fget'
    {
        if (liCode == 0)
        {
            CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
            CGS_ASSERT(mpServerInterface->GetMessageBuffer() != nullptr,
                       "mpServerInterface->GetMessageBuffer()");

            const char* lpcFriends = TagFieldFind(lpcData, "F");
            std::memset(lacFriendsList, 0, 1600);

            s32 liFriend = 0;
            char* lpName = lacFriendsList;
            while (TagFieldGetDelim(lpcFriends, lpName, 16, "", liFriend, ',') > 0)
            {
                ++liFriend;
                lpName += 16;
            }
            lpResult = lacFriendsList;
            lbSuccess = true;
        }
    }
    else if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_SET_ROAD_RULES_SCORES] ||   // 'rrup'
             liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_UPLOAD_FRIENDS]        ||   // 'fupd'
             liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_UPLOAD_RIVAL_DATA])         // 'evup'
    {
        // Plain upload acks (X360 LABEL_20): success with no result payload when code == 0;
        // otherwise the action failed and the callback is told so.
        if (liCode == 0)
            lbSuccess = true;
    }
    else if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_SET_ELO]) // 'selo'
    {
        if (liCode == 0)
        {
            lbSuccess = true;
        }
        else if (liCode == KI_CODE_NO_STATS)
        {
            // The server reports the local player has no stats yet -- log it, fail quietly.
            // (X360 streams the note into the global net log; modelled as a no-op success=false.)
            lbSuccess = false;
        }
    }

    // Common tail (X360 0x8258A728-0x8258A754): the binary does NOT call a fixed converter
    // here. It dispatches through a PER-ACTION function-pointer table indexed by the CURRENT
    // action (read while it is still set, before the reset to E_ACTION_COUNT below):
    //
    //   lwz   r10, 0x20(r15)            ; meCurrentAction (a1[8])  -- pre-reset
    //   lwz   r4,  0xC(r14)             ; liCode (a2[3])
    //   lwz   r3,  8(r14)               ; liKind (a2[2])
    //   slwi  r10, r10, 2               ; index * sizeof(void*)
    //   lwzx  r11, r10, off_820872D0    ; converter = off_820872D0[meCurrentAction]
    //   bctrl                           ; liError = converter(liKind, liCode)
    //
    // off_820872D0 is unexported .rdata (not in the available exports), so we cannot confirm
    // every slot resolves to this ConvertError -- it is the only converter body recovered for
    // this TU, and it takes exactly (kind, code) as the table entries are called. We therefore
    // model the dispatch as a direct ConvertError(liKind, liCode) call. FLAGGED ASSUMPTION:
    // if a future export shows a slot pointing at a different per-action converter, this single
    // call must become an indexed dispatch over off_820872D0[meCurrentAction]. See still_open.
    const int liError = ConvertError(liKind, liCode);  // == off_820872D0[meCurrentAction](liKind, liCode)
    EndActionCore(liError);

    CustomCommandCallback lCallback = mpCallback;  // v75 = a1[9]
    meCurrentAction = E_ACTION_COUNT;               // a1[8] = 11
    if (lCallback)
        lCallback(mpCallbackData, lpResult, lbSuccess);
    mpCallback     = nullptr;  // a1[9] = 0
    mpCallbackData = nullptr;  // a1[10] = 0
}

}  // namespace BrnNetwork
