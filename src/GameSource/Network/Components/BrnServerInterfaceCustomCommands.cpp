#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"

#include "GameSource/Network/Parameters/BrnNetworkRoadRulesData.h"  // RoadRulesUploadData / *DownloadData
#include "GameSource/GameState/BrnCgsPlayerName.h"                  // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDUnCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint (ConvertEventScoreUploadError)
#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h" // EventScoreData::SerialiseToString (UploadEventScoreData)
#include "GameSource/GameState/BrnGameActions.h"                    // GameStateModuleIO::OnlineGameResults (UploadFreeBurnLobbyStats)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // IsLoggedIn
#include "lobbyapi.h"        // DirtySDK LobbyApiRequestCB / LobbyApiRefT / LobbyApiMsgT
#include "lobbytagfield.h"   // DirtySDK TagFieldFind/Get*/Set*

#include "rw/core/stdc/util.h"  // rw::core::stdc::Min (GetRoadRulesHighScores, DWARF)
#include <xtl.h>  // XMemSet
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

// Asserts pass __FILE__; ARTIST's reads "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Network/Components/BrnServerInterfaceCustomCommands.cpp".

namespace BrnNetwork
{

// ---- file-scope constants (DWARF BrnServerInterfaceCustomCommands.cpp) ----------
namespace
{
    // Message-buffer length the X360 inlines as the literal 2048 at every TagField* site.
    const s32 KI_MESSAGE_BUFFER_LENGTH = CgsNetwork::KI_MESSAGE_BUFFER_SIZE;  // 2048

    // BrnServerInterfaceCustomCommands.cpp:46 -- per-action label, indexed by EAction; passed to
    // StartActionCore and the net-stream log. DWARF declares `const char *[N]` (non-const pointers):
    // ARTIST keeps the table in .data at 0x82F29F30 and reloads the entry at each use. Every string
    // is read from the image (the pointers at 0x82F29F30..0x82F29F58 -> 0x82086EC8..0x82086E04).
    const char* KPC_ACTION_NAMES[ServerInterfaceCustomCommands::E_ACTION_COUNT] =
    {
        "Upld RR Score",          // [0]  E_ACTION_SET_ROAD_RULES_SCORES        0x82086EC8
        "Dnld RR Scores",         // [1]  E_ACTION_GET_ROAD_RULES_SCORES        0x82086EB8
        "Dnld Local RR Scores",   // [2]  E_ACTION_GET_LOCAL_ROAD_RULES_SCORES  0x82086EA0
        "Get Friends list",       // [3]  E_ACTION_GET_FRIENDS                  0x82086E8C
        "Upld Friends list",      // [4]  E_ACTION_UPLOAD_FRIENDS               0x82086E78
        "Upld FreeBrn Stat",      // [5]  E_ACTION_UPLOAD_FREEBURN_LOBBY_STATS  0x82086E64
        "Upld Offline Prog",      // [6]  E_ACTION_UPLOAD_OFFLINE_PROGRESSION   0x82086E50
        "Upld Rivals Data",       // [7]  E_ACTION_UPLOAD_RIVAL_DATA            0x82086E3C
        "Upld Event Score Data",  // [8]  E_ACTION_UPLOAD_EVENT_SCORE_DATA      0x82086E24
        "Get Debug ELO",          // [9]  E_ACTION_GET_ELO                      0x82086E14
        "Set Debug ELO"           // [10] E_ACTION_SET_ELO                      0x82086E04
    };

    // The per-action lobby message-kind fourcc, indexed by EAction (read from the image).
    const s32 KI_ACTIONS_TO_MESSAGE_CODES[ServerInterfaceCustomCommands::E_ACTION_COUNT] =
    {
        0x72727570,  // [0]  SET_ROAD_RULES_SCORES
        0x72726774,  // [1]  GET_ROAD_RULES_SCORES
        0x72726C63,  // [2]  GET_LOCAL_ROAD_RULES_SCORES
        0x66676574,  // [3]  GET_FRIENDS
        0x66757064,  // [4]  UPLOAD_FRIENDS
        0x66627374,  // [5]  UPLOAD_FREEBURN_LOBBY_STATS
        0x6F707570,  // [6]  UPLOAD_OFFLINE_PROGRESSION
        0x72767570,  // [7]  UPLOAD_RIVAL_DATA
        0x65767570,  // [8]  UPLOAD_EVENT_SCORE_DATA
        0x67656C6F,  // [9]  GET_ELO
        0x73656C6F   // [10] SET_ELO
    };

    // --- ConvertError special-case codes (read off the X360 cmpw immediates) ----------
    const s32 KI_CODE_NOT_RECOGNISED = 0x62616463;  // 0x62616463 -> 140
    const s32 KI_CODE_TIMEOUT        = 0x74696D65;  // 0x74696D65 -> 5
    const s32 KI_CODE_IN_PROGRESS    = 0x69706572;  // 0x69706572 -> 6

    // 'nost' (0x6E6F7374) -- the set-elo "local player has no stats" sub-error code.
    const s32 KI_CODE_NO_STATS       = 0x6E6F7374;

    // CustomCommandErrorMapping (BrnServerInterfaceCustomCommands.cpp:105) -- the generic
    // {kind, code} -> server-interface error table consulted by ConvertError when the code
    // is not one of the three special cases above. 14 rows (KI_NUMBER_OF_ERROR_MAPPINGS).
    struct CustomCommandErrorMapping
    {
        s32 liMessageKind;            // BrnServerInterfaceCustomCommands.cpp:107
        s32 liMessageCode;            // BrnServerInterfaceCustomCommands.cpp:108
        s32 liServerInterfaceErrorCode; // BrnServerInterfaceCustomCommands.cpp:109
    };

    // The 14 {kind, code, error} rows ConvertError scans (read from the image).
    const s32 KI_NUMBER_OF_ERROR_MAPPINGS = 14;

    const CustomCommandErrorMapping
        KA_CUSTOM_COMMAND_ERROR_MAPPING[KI_NUMBER_OF_ERROR_MAPPINGS] =
    {
        { 0x72727570, 0x696E7670, 3 },
        { 0x72727570, 0x69736B79, 3 },
        { 0x72727570, 0x74696D65, 5 },
        { 0x72726774, 0x6E6F6672, 0 },
        { 0x72726774, 0x696E7670, 3 },
        { 0x72726774, 0x74696D65, 5 },
        { 0x72726C63, 0x74696D65, 5 },
        { 0x72726C63, 0x696E7670, 3 },
        { 0x66676574, 0x6D617574, 4 },
        { 0x66676574, 0x6D697373, 2 },
        { 0x66676574, 0x696E7670, 3 },
        { 0x66757064, 0x6D617574, 4 },
        { 0x66757064, 0x6D697373, 2 },
        { 0x73656C6F, 0x6E6F7374, 0 },
    };
}

// ---------------------------------------------------------------------------
// Construct @ 0x82583720 -- chain to the component base then null/idle the Brn members.
void ServerInterfaceCustomCommands::Construct()
{
    CgsNetwork::ServerInterfaceComponent::Construct();

    mpServerInterface    = 0;   // a1[4]
    mpCallback           = 0;   // a1[9]
    mpCallbackData       = 0;   // a1[10]
    mpiRaceElo           = 0;   // a1[5]
    mpiRoadRageElo       = 0;   // a1[6]
    mpiBurningHomeRunElo = 0;   // a1[7]
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

    CGS_ASSERT_AT(lpServerInterface != 0,
        "lpServerInterface", __FILE__, 209 /* ARTIST 0x825837B4 */);

    mpServerInterface    = lpServerInterface;  // a1[4]
    mpCallback           = 0;            // a1[9]
    mpCallbackData       = 0;            // a1[10]
    mpiRaceElo           = 0;            // a1[5]
    mpiRoadRageElo       = 0;            // a1[6]
    mpiBurningHomeRunElo = 0;            // a1[7]
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

    mpServerInterface    = 0;   // a1[4]
    mpCallback           = 0;   // a1[9]
    mpCallbackData       = 0;   // a1[10]
    mpiRaceElo           = 0;   // a1[5]
    mpiRoadRageElo       = 0;   // a1[6]
    mpiBurningHomeRunElo = 0;   // a1[7]
    meCurrentAction      = E_ACTION_COUNT;  // a1[8] = 11
    return true;
}

// Destruct -- no standalone console body; recovered from its inlined copy in
// BrnServerInterfaceBase::Destruct: null / idle the members, then the base Destruct.
void ServerInterfaceCustomCommands::Destruct()
{
    mpServerInterface    = 0;
    mpiRaceElo           = 0;
    mpiRoadRageElo       = 0;
    mpiBurningHomeRunElo = 0;
    meCurrentAction      = E_ACTION_COUNT;
    mpCallback           = 0;
    mpCallbackData       = 0;

    CgsNetwork::ServerInterfaceCustomCommands::Destruct();
}

// Update -- no standalone console body; BrnServerInterfaceBase::Update brackets it with its
// perf monitor and the bracket is empty.
void ServerInterfaceCustomCommands::Update()
{
}

// ---------------------------------------------------------------------------
// SendCustomCommand @ 0x8258F028 -- shared request tail: stamp the action, open its log
// scope (StartActionCore), assert the payload, and post via LobbyApiRequestCB.
void ServerInterfaceCustomCommands::SendCustomCommand(EAction leNewAction,
                                                      const char* lpcMessageData)
{
    CGS_ASSERT_AT(leNewAction != E_ACTION_COUNT,
        "meNewAction != E_ACTION_COUNT",
        __FILE__, 571 /* ARTIST 0x8258F064 */);

    meCurrentAction = leNewAction;                       // a1[8] = leNewAction
    StartActionCore(KPC_ACTION_NAMES[leNewAction]);      // (*(vtable+12))(this, off_82F29F30[a2])

    CGS_ASSERT_AT(lpcMessageData != 0,
        "lpcMessageData",
        __FILE__, 578 /* ARTIST 0x8258F0B0 */);

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
    CGS_ASSERT_AT(lpiRaceElo != 0,
        "lpRaceElo", __FILE__, 601 /* ARTIST 0x8258F12C */);            // a2, cpp:601
    CGS_ASSERT_AT(lpiRoadRageElo != 0,
        "lpRoadRageElo", __FILE__, 602 /* ARTIST 0x8258F150 */);    // a4, cpp:602
    CGS_ASSERT_AT(lpiBurningHomeRunElo != 0,
        "lpBurningHomeRunElo", __FILE__, 603 /* ARTIST 0x8258F174 */); // a3, cpp:603
    CGS_ASSERT_AT(lCallback != 0,
        "lCallback", __FILE__, 604 /* ARTIST 0x8258F198 */);            // a5, cpp:604
    CGS_ASSERT_AT(lpData != 0,
        "lpData", __FILE__, 605 /* ARTIST 0x8258F1BC */);                  // a6, cpp:605

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

    // Every buffer use goes through GetMessageBuffer, each field written inside its assert as the
    // assert texts quote them (ARTIST 0x8258F228..0x8258F3BC, lines 638..640).
    mpServerInterface->GetMessageBuffer()[0] = 0;  // empty the record
    CGS_ASSERT_AT(TagFieldSetNumber(mpServerInterface->GetMessageBuffer(),
                                    mpServerInterface->GetMessageBufferLength(), "RACE", liRaceElo),
                  "CgsNetwork::DirtySock::TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"RACE\", liRaceElo)",
                  __FILE__, 638);
    CGS_ASSERT_AT(TagFieldSetNumber(mpServerInterface->GetMessageBuffer(),
                                    mpServerInterface->GetMessageBufferLength(), "ROADRAGE", liRoadRageElo),
                  "CgsNetwork::DirtySock::TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"ROADRAGE\", liRoadRageElo)",
                  __FILE__, 639);
    CGS_ASSERT_AT(TagFieldSetNumber(mpServerInterface->GetMessageBuffer(),
                                    mpServerInterface->GetMessageBufferLength(), "BURNINGHOMERUN",
                                    liBurningHomeRunElo),
                  "CgsNetwork::DirtySock::TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"BURNINGHOMERUN\", liBurningHomeRunElo)",
                  __FILE__, 640);

    SendCustomCommand(E_ACTION_SET_ELO, mpServerInterface->GetMessageBuffer());
}

// GetFriendsListFromServer @ 0x8258FD18 -- write the "TAG=F" filter, then send 'get friends'.
void ServerInterfaceCustomCommands::GetFriendsListFromServer(CustomCommandCallback lCallback, void* lpData)
{
    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    // Every buffer use goes through GetMessageBuffer (DWARF lists four calls; ARTIST 0x8258FD34..
    // 0x8258FE60), the field write inside its assert, as the assert text quotes it.
    CGS_ASSERT_AT(mpServerInterface->GetMessageBuffer(), "mpServerInterface->GetMessageBuffer()",
                  __FILE__, 851);
    mpServerInterface->GetMessageBuffer()[0] = 0;
    CGS_ASSERT_AT(TagFieldSetString(mpServerInterface->GetMessageBuffer(),
                                    mpServerInterface->GetMessageBufferLength(), "TAG", "F"),
                  "CgsNetwork::DirtySock::TagFieldSetString( mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"TAG\", \"F\" )",
                  __FILE__, 861);

    SendCustomCommand(E_ACTION_GET_FRIENDS, mpServerInterface->GetMessageBuffer());
}

// GetLocalRoadRulesHighScores @ 0x8258FB10 -- write SKEY/NUM/IDX, then send 'get local rr'.
void ServerInterfaceCustomCommands::GetLocalRoadRulesHighScores(s32 liNumScoresToDownload,
                                                                s32 liNextRoadIndexToDownload,
                                                                const char* lpcRoadRulesServerKey,
                                                                CustomCommandCallback lCallback, void* lpData)
{
    CGS_ASSERT_AT(liNumScoresToDownload != 0, "liNumScoresToDownload", __FILE__, 808);

    mpCallbackData = lpData;     // a1[10]
    mpCallback     = lCallback;  // a1[9]

    // ARTIST 0x8258FB98..0x8258FCF8: GetMessageBuffer at each use, the fields inside their asserts.
    mpServerInterface->GetMessageBuffer()[0] = 0;
    CGS_ASSERT_AT(TagFieldSetRaw(mpServerInterface->GetMessageBuffer(),
                                 mpServerInterface->GetMessageBufferLength(), "SKEY", lpcRoadRulesServerKey),
                  "CgsNetwork::DirtySock::TagFieldSetRaw( mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"SKEY\", lpcRoadRulesServerKey )",
                  __FILE__, 820);
    CGS_ASSERT_AT(TagFieldSetNumber(mpServerInterface->GetMessageBuffer(),
                                    mpServerInterface->GetMessageBufferLength(), "NUM", liNumScoresToDownload),
                  "CgsNetwork::DirtySock::TagFieldSetNumber( mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"NUM\", liNumScoresToDownload )",
                  __FILE__, 826);
    CGS_ASSERT_AT(TagFieldSetNumber(mpServerInterface->GetMessageBuffer(),
                                    mpServerInterface->GetMessageBufferLength(), "IDX", liNextRoadIndexToDownload),
                  "CgsNetwork::DirtySock::TagFieldSetNumber( mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"IDX\", liNextRoadIndexToDownload )",
                  __FILE__, 830);

    SendCustomCommand(E_ACTION_GET_LOCAL_ROAD_RULES_SCORES, mpServerInterface->GetMessageBuffer());
}

// UploadOfflineProgress @ 0x82590908 -- pack the OFFPROG structure + FBURNCHALL count, send.
void ServerInterfaceCustomCommands::UploadOfflineProgress(
    const NetworkInOfflineProgression::OfflineProgressionT* lpOfflineProgression,
    s32 liFreeburnChallengeSuccessCount)
{
    CGS_ASSERT_AT(lpOfflineProgression != 0, "lpOfflineProgression", __FILE__, 1317);
    CGS_ASSERT_AT(mpServerInterface != 0, "mpServerInterface", __FILE__, 1318);

    // DWARF: no locals, three GetMessageBuffer calls (ARTIST 0x825909AC / 0x825909BC: the clear
    // and the OFFPROG field each read the buffer through it).
    mpServerInterface->GetMessageBuffer()[0] = 0;

    // The OFFPROG field packs the offline-progression record with the X360 byte pattern.
    TagFieldSetStructure(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "OFFPROG",
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

    mpServerInterface->GetMessageBuffer()[0] = 0;

    // DWARF BrnServerInterfaceCustomCommands.cpp:722..725. The two clears are CgsCore::MemSet in
    // DWARF (no home in this tree); ARTIST calls XMemSet (0x8258F970 / 0x8258F980).
    char lacRoadIndex[4];
    char lacRoadsToGetScoresFor[120];
    XMemSet(lacRoadIndex, 0, sizeof(lacRoadIndex));
    XMemSet(lacRoadsToGetScoresFor, 0, sizeof(lacRoadsToGetScoresFor));

    s32 liEndRoadIndex = rw::core::stdc::Min(liStartIndex + liNumScores, 64);

    for (s32 liIndex = liStartIndex; liIndex < liEndRoadIndex; )
    {
        CgsCore::SPrintf(lacRoadIndex, 120, "%d", liIndex);
        std::strncat(lacRoadsToGetScoresFor, lacRoadIndex, 120);
        if (++liIndex == liEndRoadIndex)
            break;
        std::strncat(lacRoadsToGetScoresFor, ",", 120);
    }

    CGS_ASSERT_AT(TagFieldSetRaw(mpServerInterface->GetMessageBuffer(),
                                 mpServerInterface->GetMessageBufferLength(), "R", lacRoadsToGetScoresFor),
                  "CgsNetwork::DirtySock::TagFieldSetRaw( mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"R\", lacRoadsToGetScoresFor )",
                  __FILE__, 787);

    // ARTIST inlines SendCustomCommand here (its lpcMessageData assert, line 578 at 0x8258FAD0).
    SendCustomCommand(E_ACTION_GET_ROAD_RULES_SCORES, mpServerInterface->GetMessageBuffer());
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

    CGS_ASSERT(lpRoadRulesData != 0, "lpRoadRulesData");
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

    CGS_ASSERT(lpcMessageData != 0, "lpcMessageData");

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
    CGS_ASSERT_AT(liNumBuddies > 0, "liNumBuddies > 0", __FILE__, 998);
    CGS_ASSERT_AT(!lpaFriendsNames[0].IsEmpty(), "!lpaFriendsNames[0].IsEmpty()", __FILE__, 999);

    mpCallbackData = 0;  // a1[10]
    mpCallback     = 0;  // a1[9]

    // ARTIST 0x8259032C..0x82590594: GetMessageBuffer at each use, the fields inside their asserts
    // (texts quoted from the image), SendCustomCommand inlined after the loop (its line 578).
    CGS_ASSERT_AT(mpServerInterface->GetMessageBuffer(), "mpServerInterface->GetMessageBuffer()",
                  __FILE__, 1004);
    mpServerInterface->GetMessageBuffer()[0] = 0;
    CGS_ASSERT_AT(TagFieldSetRaw(mpServerInterface->GetMessageBuffer(),
                                 mpServerInterface->GetMessageBufferLength(), "TAG", "F"),
                  "CgsNetwork::DirtySock::TagFieldSetRaw( mpServerInterface->GetMessageBuffer(), "
                  "mpServerInterface->GetMessageBufferLength(), \"TAG\", \"F\" )",
                  __FILE__, 1014);

    if (liNumBuddies > 0)
    {
        // DWARF BrnServerInterfaceCustomCommands.cpp:961..963 (the PS3 list is 2300 bytes; ARTIST's
        // is the 1900 it clears and appends to: frame 0x840, list at +0x70). The clear is
        // CgsCore::MemSet in DWARF (no home in this tree), XMemSet in ARTIST (0x82590434).
        char lacFriendsList[1900];
        char lacAppendPlayerName[23];
        XMemSet(lacFriendsList, 0, 1900);

        for (s32 liFriendIndex = 0; liFriendIndex < liNumBuddies; )
        {
            // Names with embedded spaces are quoted; others are written bare.
            const char* lpcFormat = (std::strchr(lpaFriendsNames[liFriendIndex].GetPlayerName(), ' ') != 0)
                                        ? "\"%s\"" : "%s";
            CgsCore::SPrintf(lacAppendPlayerName, 19, lpcFormat, lpaFriendsNames[liFriendIndex].GetPlayerName());
            std::strncat(lacFriendsList, lacAppendPlayerName, 1900);
            if (++liFriendIndex != liNumBuddies)
            {
                std::strncat(lacFriendsList, ",", 1900);
            }
        }

        CGS_ASSERT_AT(TagFieldSetRaw(mpServerInterface->GetMessageBuffer(),
                                     mpServerInterface->GetMessageBufferLength(), "SET", lacFriendsList),
                      "CgsNetwork::DirtySock::TagFieldSetRaw( mpServerInterface->GetMessageBuffer(), "
                      "mpServerInterface->GetMessageBufferLength(), \"SET\", lacFriendsList )",
                      __FILE__, 1048);
    }

    SendCustomCommand(E_ACTION_UPLOAD_FRIENDS, mpServerInterface->GetMessageBuffer());
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
            const char* lpcFormat = (std::strchr(lpName->GetPlayerName(), ' ') != 0)
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
            const char* lpcFormat = (std::strchr(lpName->GetPlayerName(), ' ') != 0)
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

    CGS_ASSERT(lpcMessageData != 0, "lpcMessageData");

    LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                      KI_ACTIONS_TO_MESSAGE_CODES[meCurrentAction],
                      lpcMessageData,
                      &ServerInterfaceCustomCommands::_CustomCommandSentCallback,
                      this);
}

// ---------------------------------------------------------------------------
// UploadLiveRevengeData @ 0x82590A88 -- pack each rival as a RIVAL%i structure field, then
// post the 'upload rivals' request inline. No-op when liNumberToUpload == 0.
void ServerInterfaceCustomCommands::UploadLiveRevengeData(const ServerGeneratedTypes::RivalDataT* lpRivalData,
                                                          const s32* lpiRivalIDs,
                                                          s32 liNumberToUpload)
{
    CGS_ASSERT_AT(lpRivalData != 0, "lpRivalData", __FILE__, 1352);
    CGS_ASSERT_AT(liNumberToUpload >= 0, "liNumberToUpload >= 0", __FILE__, 1353);

    if (liNumberToUpload > 0)
    {
        mpServerInterface->GetMessageBuffer()[0] = 0;

        // One 64-byte rival record per row; lpiRivalIDs supplies each row's id. DWARF locals
        // liRivalIndex and the loop's char lacBuffer[7].
        for (s32 liRivalIndex = 0; liRivalIndex < liNumberToUpload; ++liRivalIndex)
        {
            char lacBuffer[7];
            CgsCore::SPrintf(lacBuffer, 7, "RIVAL%i", lpiRivalIDs[liRivalIndex]);
            TagFieldSetStructure(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH,
                                 lacBuffer, &lpRivalData[liRivalIndex],
                                 static_cast<s32>(sizeof(ServerGeneratedTypes::RivalDataT)),
                                 "llllllllllll16s");
        }

        // ARTIST inlines SendCustomCommand here (its lpcMessageData assert, line 578 at
        // 0x82590C28); the action-count assert folds away for the constant action.
        SendCustomCommand(E_ACTION_UPLOAD_RIVAL_DATA, mpServerInterface->GetMessageBuffer());
    }
}

// ---------------------------------------------------------------------------
// UploadFreeBurnLobbyStats -- the finished free-burn lobby game's takedowns, crashes, completed
// challenges (each only when positive), distance, time and rival count. Without a login the
// stats are dropped with a log line.
void ServerInterfaceCustomCommands::UploadFreeBurnLobbyStats(
    const BrnGameState::GameStateModuleIO::OnlineGameResults* lpResults,
    s32 liNumberOfRivals, s32 liNumberOfChallengesCompleted)
{
    // The logged-in arm first, the warning in the else (ARTIST 0x825906D8 branches to the warning
    // placed at the end, 0x825908E0).
    if (static_cast<CgsNetwork::ServerInterfaceConnection*>(mpServerInterface->GetConnectionComponent())->IsLoggedIn())
    {
        mpServerInterface->GetMessageBuffer()[0] = 0;

        if (lpResults->miTakedownsFor > 0)
        {
            TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "TKDNF",
                              lpResults->miTakedownsFor);
        }
        if (lpResults->miTakedownsAgainst > 0)
        {
            TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "TKDNA",
                              lpResults->miTakedownsAgainst);
        }
        if (lpResults->miMarkedManTakedownsFor > 0)
        {
            TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "CRSH",
                              lpResults->miMarkedManTakedownsFor);
        }
        if (liNumberOfChallengesCompleted > 0)
        {
            TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "FBCHAL",
                              liNumberOfChallengesCompleted);
        }

        TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "DIST",
                          static_cast<s32>(lpResults->mfMetersDriven));
        TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "TIME",
                          lpResults->mSecondsInEvent.GetSeconds());
        TagFieldSetNumber(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH, "RIVALS",
                          liNumberOfRivals);

        SendCustomCommand(E_ACTION_UPLOAD_FREEBURN_LOBBY_STATS, mpServerInterface->GetMessageBuffer());
    }
    else
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput
            << "WARNING: Trying to upload freeburn stats whilst not connect, these have now been lost. ";
    }
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
        CGS_FAIL("Server doesn't recongise");
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
    CGS_FAIL("Unhandled custom command message type");
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
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Server doesn't recognise: " << lacKind << "\n";
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
    CGS_FAIL("Unhandled custom command message type: ");
    return 1;
}

// ---------------------------------------------------------------------------
// UploadEventScoreData @ 0x825905D8 -- stash the callback + user data, serialise the batch of
// event scores into the message buffer, then post the 'event score data' upload (action 8).
void ServerInterfaceCustomCommands::UploadEventScoreData(EventScoreUploadData* lpEventScoreUploadData,
                                                         CustomCommandCallback lCallback, void* lpData)
{
    mpCallbackData = lpData;     // a1[10] -- stw r6, 0x28
    mpCallback     = lCallback;  // a1[9]  -- stw r5, 0x24

    CGS_ASSERT_AT(lpEventScoreUploadData != 0, "lpEventScoreUploadData",
        __FILE__, 1071 /* ARTIST 0x82590604 */);

    lpEventScoreUploadData->SerialiseToString(mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_LENGTH);

    // void: ARTIST 0x82590698 leaves SendCustomCommand's r3 (both callers ignore the result).
    SendCustomCommand(E_ACTION_UPLOAD_EVENT_SCORE_DATA, mpServerInterface->GetMessageBuffer());
}

// ---------------------------------------------------------------------------
// ~ServerInterfaceCustomCommands (scalar deleting destructor @ 0x827DE408).
// The X360 thunk restores this leaf class's vtable (off_820CDBF8) at this+0 and conditionally
// runs operator delete (the MSVC flag-bit-0 `delete this` path). Emitting the destructor as
// virtual makes the compiler synthesise exactly that thunk; there are no owned resources to
// release (Construct only nulls the POD members), so the body is empty. Mirrors the committed
// CgsNetwork::ServerInterfaceEndGameDataBase virtual-destructor pattern.

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
    CGS_ASSERT(mpServerInterface != 0, "mpServerInterface");
    CGS_ASSERT(lpMsg != 0, "lpMsg");

    bool  lbSuccess = false;   // v4
    void* lpResult  = 0; // v5

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
                 lpToken; lpToken = std::strtok(0, ","))
            {
                char lacName[16];   // v85
                char lacScore[16];  // v83
                std::strncpy(lacName, lpToken, 16);

                char* lpScoreToken = std::strtok(0, ",");
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

            lpToken = std::strtok(0, ",");
            std::strncpy(lacField, lpToken, 13);
            const s32 liIdHigh = std::atoi(lacField);

            lLocalScores.Construct((static_cast<u64>(static_cast<u32>(liIdHigh)) << 32) |
                                   static_cast<u32>(liIdLow));

            s32 liScoreType = 0;  // v47
            for (char* lpScore = std::strtok(0, ","); lpScore;
                 lpScore = std::strtok(0, ","))
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
            CGS_ASSERT(mpiRaceElo != 0, "mpiRaceElo");
            CGS_ASSERT(mpiRoadRageElo != 0, "mpiRoadRageElo");
            CGS_ASSERT(mpiBurningHomeRunElo != 0, "mpiBurningHomeRunElo");

            *mpiRaceElo          = TagFieldGetNumber(TagFieldFind(lpcData, "RACE"), 0);
            *mpiRoadRageElo      = TagFieldGetNumber(TagFieldFind(lpcData, "ROADRAGE"), 0);
            *mpiBurningHomeRunElo = TagFieldGetNumber(TagFieldFind(lpcData, "BURNINGHOMERUN"), 0);
            lbSuccess = true;
        }
        mpiRaceElo           = 0;
        mpiRoadRageElo       = 0;
        mpiBurningHomeRunElo = 0;
    }
    else if (liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_GET_FRIENDS]) // 'fget'
    {
        if (liCode == 0)
        {
            CGS_ASSERT(mpServerInterface != 0, "mpServerInterface");
            CGS_ASSERT(mpServerInterface->GetMessageBuffer() != 0,
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
             liKind == KI_ACTIONS_TO_MESSAGE_CODES[E_ACTION_UPLOAD_EVENT_SCORE_DATA])   // 'evup'
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

    // Common tail: the reply is converted by the CURRENT action's converter (read before the
    // action is reset below): the event-score upload has its own, every other action uses
    // ConvertError.
    typedef int (ServerInterfaceCustomCommands::*ErrorConverter)(int, int);
    static const ErrorConverter KA_ACTION_ERROR_CONVERTERS[E_ACTION_COUNT] =
    {
        &ServerInterfaceCustomCommands::ConvertError,                  // [0]
        &ServerInterfaceCustomCommands::ConvertError,                  // [1]
        &ServerInterfaceCustomCommands::ConvertError,                  // [2]
        &ServerInterfaceCustomCommands::ConvertError,                  // [3]
        &ServerInterfaceCustomCommands::ConvertError,                  // [4]
        &ServerInterfaceCustomCommands::ConvertError,                  // [5]
        &ServerInterfaceCustomCommands::ConvertError,                  // [6]
        &ServerInterfaceCustomCommands::ConvertError,                  // [7]
        &ServerInterfaceCustomCommands::ConvertEventScoreUploadError,  // [8]
        &ServerInterfaceCustomCommands::ConvertError,                  // [9]
        &ServerInterfaceCustomCommands::ConvertError,                  // [10]
    };
    const int liError = (this->*KA_ACTION_ERROR_CONVERTERS[meCurrentAction])(liKind, liCode);
    EndActionCore(liError);

    CustomCommandCallback lCallback = mpCallback;  // v75 = a1[9]
    meCurrentAction = E_ACTION_COUNT;               // a1[8] = 11
    if (lCallback)
        lCallback(mpCallbackData, lpResult, lbSuccess);
    mpCallback     = 0;  // a1[9] = 0
    mpCallbackData = 0;  // a1[10] = 0
}

}  // namespace BrnNetwork
