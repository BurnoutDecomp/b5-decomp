#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfaceRankings.cpp"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"                 // ServerInterface (GetLobbyAPIRef)
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "lobbyapi.h"                                                                              // LobbyApiMsgT

// The rankings component over the DirtySDK LobbyRank module. The getters are thin
// forwards onto the downloaded rank list; the two download actions run through the
// shared component action bookkeeping (StartActionCore / EndActionCore) and complete in
// the LobbyRank callbacks.
//
// The asserts that the original builds by streaming into the assert buffer (with the
// numeric error code appended) keep the console's message text as a plain literal.

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfaceRankings.cpp".

namespace CgsNetwork
{
    namespace
    {
        // LobbyRank error codes -> server-interface errors (both actions share it).
        const DSErrorToServerInterfaceError KA_RANK_DS_SERVER_INTERFACE_ERROR_MAPPING[13] =
        {
            {  -1, E_SERVER_INTERFACE_RANKINGS_ERROR_INVALID_CATEGORY },
            {  -2, E_SERVER_INTERFACE_RANKINGS_ERROR_INVALID_INDEX },
            {  -3, E_SERVER_INTERFACE_RANKINGS_ERROR_INVALID_VARIATION },
            {  -4, E_SERVER_INTERFACE_RANKINGS_ERROR_INVALID_VIEW },
            {  -5, E_SERVER_INTERFACE_RANKINGS_ERROR_LIST_NOT_DOWNLOADED },
            {  -6, E_SERVER_INTERFACE_RANKINGS_ERROR_RANKINGS_NOT_DOWNLOADED },
            {  -7, E_SERVER_INTERFACE_RANKINGS_ERROR_MISC },
            {  -8, E_SERVER_INTERFACE_RANKINGS_ERROR_TIMEOUT },
            {  -9, E_SERVER_INTERFACE_RANKINGS_ERROR_OUT_OF_MEMORY },
            { -10, E_SERVER_INTERFACE_RANKINGS_ERROR_LOBBY_INVALID },
            { -11, E_SERVER_INTERFACE_RANKINGS_ERROR_DOWNLOAD_IN_PROGRESS },
            { -12, E_SERVER_INTERFACE_RANKINGS_ERROR_INVALID_COL },
            { -13, E_SERVER_INTERFACE_RANKINGS_ERROR_INVALID_ROW },
        };

        // LobbyRankColumnCount's "list not downloaded" result.
        const s32 KI_RANK_ERROR_LIST_NOT_DOWNLOADED = -5;

        // Completion message kinds of the two fetches.
        const s32 KI_MSG_KIND_CATEGORIES = 0x63617465;   // 'cate'
        const s32 KI_MSG_KIND_LIST       = 0x6C697374;   // 'list'

        // Row attribute bit marking the local user's row.
        const s32 KI_ROW_ATTRIB_LOCAL_USER = 1;
    }

    const DSErrorToServerInterfaceErrorTable
    ServerInterfaceRankings::KA_DS_ERROR_TABLE_LOOKUP[ServerInterfaceRankings::E_ACTION_COUNT] =
    {
        { KA_RANK_DS_SERVER_INTERFACE_ERROR_MAPPING, 13 },
        { KA_RANK_DS_SERVER_INTERFACE_ERROR_MAPPING, 13 },
    };

    const char* ServerInterfaceRankings::KAPC_ACTION_NAMES[ServerInterfaceRankings::E_ACTION_COUNT] =
    {
        "Downloading Category Data",
        "Downloading Rank Data",
    };

    // The console object is laid out by Construct; the constructor only installs the vtable.

    void ServerInterfaceRankings::Construct()
    {
        meStatus          = 2;
        mpcCurrentAction  = "";
        miLastError       = 0;
        mpServerInterface = 0;
        mpLobbyRank       = 0;
        miUserType        = 0;
    }

    bool ServerInterfaceRankings::Prepare(ServerInterface* lpServerInterface)
    {
        CGS_ASSERT_AT(mpLobbyRank == 0, "!mpLobbyRank", CGS_ARTIST_FILE, 106 /* ARTIST 0x8287B9FC */);

        mpServerInterface = lpServerInterface;
        meCurrentAction   = E_ACTION_COUNT;
        miUserType        = 0;

        CGS_ASSERT_AT(lpServerInterface != 0, "mpServerInterface", CGS_ARTIST_FILE, 112 /* ARTIST 0x8287BA34 */);

        mpLobbyRank = LobbyRankCreate(mpServerInterface->GetLobbyAPIRef(), 0);
        CGS_ASSERT_AT(mpLobbyRank != 0, "mpLobbyRank", CGS_ARTIST_FILE, 117 /* ARTIST 0x8287BA6C */);
        return true;
    }

    bool ServerInterfaceRankings::Release()
    {
        if (mpLobbyRank != 0)
        {
            LobbyRankDestroy(mpLobbyRank);
            mpLobbyRank = 0;
        }
        miUserType        = 0;
        mpServerInterface = 0;
        return true;
    }

    void ServerInterfaceRankings::Destruct()
    {
        miUserType = 0;
    }

    // The rank module has nothing to pump: the per-frame update is the shared empty body.
    void ServerInterfaceRankings::Update()
    {
    }

    void ServerInterfaceRankings::Suspend()
    {
        LobbyRankFlush(mpLobbyRank, 1);
    }

    void ServerInterfaceRankings::Resume()
    {
    }

    void ServerInterfaceRankings::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        if (leEvent == E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED)
        {
            if (mpLobbyRank == 0)
            {
                mpLobbyRank = LobbyRankCreate(mpServerInterface->GetLobbyAPIRef(), 0);
                CGS_ASSERT_AT(mpLobbyRank != 0,
                    "mpLobbyRank",
                    __FILE__, 225 /* ARTIST 0x8287BB84 */);
            }
        }
        else if (leEvent == E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_DESTROYING)
        {
            if (mpLobbyRank != 0)
            {
                LobbyRankDestroy(mpLobbyRank);
                mpLobbyRank = 0;
            }
        }
    }

    s32 ServerInterfaceRankings::GetNumberOfColumns()
    {
        CGS_ASSERT(mpLobbyRank != 0, "mpLobbyRank");

        const s32 liNoOfColumns = LobbyRankColumnCount(mpLobbyRank);
        CGS_ASSERT(liNoOfColumns != KI_RANK_ERROR_LIST_NOT_DOWNLOADED, "List not downloaded");
        CGS_ASSERT(liNoOfColumns >= 0, "liNoOfColumns >= 0");
        return liNoOfColumns;
    }

    s32 ServerInterfaceRankings::GetNumberOfRows()
    {
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank", __FILE__, 307 /* ARTIST 0x8287BCD4 */);

        const s32 liNoOfRows = LobbyRankRowCount(mpLobbyRank);
        CGS_ASSERT_AT(liNoOfRows >= 0,
            "liNoOfRows >= 0", __FILE__, 310 /* ARTIST 0x8287BD04 */);
        return liNoOfRows;
    }

    s32 ServerInterfaceRankings::GetNumberOfCategories()
    {
        CGS_ASSERT(mpLobbyRank != 0, "mpLobbyRank");

        const s32 liNoOfCategories = LobbyRankCategoryCount(mpLobbyRank);
        CGS_ASSERT(liNoOfCategories >= 0,
                   "Error getting categories, likely not connected to server. Error code: ");
        return (liNoOfCategories < 0) ? 0 : liNoOfCategories;
    }

    s32 ServerInterfaceRankings::GetNumberOfIndexes(s32 liCategory)
    {
        CGS_ASSERT(mpLobbyRank != 0, "mpLobbyRank");

        const s32 liNoOfIndexes = LobbyRankIndexCount(mpLobbyRank, liCategory);
        CGS_ASSERT(liCategory >= 0, "Error getting indexes");
        return liNoOfIndexes;
    }

    s32 ServerInterfaceRankings::GetNumberOfVariations(s32 liCategory, s32 liIndex)
    {
        CGS_ASSERT(mpLobbyRank != 0, "mpLobbyRank");

        const s32 liNoOfVariations = LobbyRankVariationCount(mpLobbyRank, liCategory, liIndex);
        CGS_ASSERT(liCategory >= 0, "Error getting variation");
        return liNoOfVariations;
    }

    const char* ServerInterfaceRankings::GetCategoryName(s32 liCategory)
    {
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank", __FILE__, 544 /* ARTIST 0x8287C038 */);
        return LobbyRankCategoryName(mpLobbyRank, liCategory, 0, 0);
    }

    const char* ServerInterfaceRankings::GetIndexName(s32 liCategory, s32 liIndex)
    {
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank", __FILE__, 560 /* ARTIST 0x8287C0AC */);
        return LobbyRankIndexName(mpLobbyRank, liCategory, liIndex, 0, 0);
    }

    const char* ServerInterfaceRankings::GetVariationName(s32 liCategory, s32 liIndex, s32 liVariation)
    {
        CGS_ASSERT_AT(mpLobbyRank != 0, "mpLobbyRank", CGS_ARTIST_FILE, 577 /* ARTIST 0x8287C128 */);
        return LobbyRankVariationName(mpLobbyRank, liCategory, liIndex, liVariation, 0, 0);
    }

    void ServerInterfaceRankings::SelectScoreboard(s32 liCategory, s32 liIndex, s32 liVariation)
    {
        LobbyRankSelect(mpLobbyRank, liCategory, liIndex, liVariation);
        miUserType = LobbyRankVariationUserType(mpLobbyRank, liCategory, liIndex, liVariation);
    }

    const char* ServerInterfaceRankings::GetColumnTitle(s32 liColumn)
    {
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank", __FILE__, 615 /* ARTIST 0x8287C1D0 */);
        return LobbyRankColumnName(mpLobbyRank, liColumn, 0, 0);
    }

    void ServerInterfaceRankings::GetCell(s32 liColumn, s32 liRow, char* lpcBuffer, s32 liBufferSize)
    {
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank", __FILE__, 633 /* ARTIST 0x8287C24C */);
        LobbyRankData(mpLobbyRank, liColumn, liRow, lpcBuffer, liBufferSize);
    }

    s32 ServerInterfaceRankings::GetColumnWidth(s32 liColumn)
    {
        return LobbyRankColumnWidth(mpLobbyRank, liColumn);
    }

    s32 ServerInterfaceRankings::GetColumnStyle(s32 liColumn)
    {
        return LobbyRankColumnStyle(mpLobbyRank, liColumn);
    }

    s32 ServerInterfaceRankings::GetColumnType(s32 liColumn)
    {
        return LobbyRankColumnType(mpLobbyRank, liColumn);
    }

    bool ServerInterfaceRankings::ScoreboardHasParam(s32 liParam)
    {
        s32 liIndex = 0;
        s32 liParm;
        while (LobbyRankParm(mpLobbyRank, liIndex, &liParm) == 0)
        {
            ++liIndex;
            if (liParm == liParam)
            {
                return true;
            }
        }
        return false;
    }

    s32 ServerInterfaceRankings::GetRowThatContainsLocalUser()
    {
        for (s32 liRow = 0; liRow < GetNumberOfRows(); ++liRow)
        {
            s32 liAttrib;
            if (LobbyRankRowAttrib(mpLobbyRank, liRow, &liAttrib) == 0 &&
                (liAttrib & KI_ROW_ATTRIB_LOCAL_USER) == KI_ROW_ATTRIB_LOCAL_USER)
            {
                return liRow;
            }
        }
        return -1;
    }

    void ServerInterfaceRankings::DownloadHeadings(const char* lpcView)
    {
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank",
            CGS_ARTIST_FILE, 391 /* ARTIST 0x8289184C */);
        CGS_ASSERT_AT(!IsBusy(),
            "!IsBusy()",
            CGS_ARTIST_FILE, 392 /* ARTIST 0x82891874 */);

        meCurrentAction = E_ACTION_DOWNLOADING_HEADINGS;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_DOWNLOADING_HEADINGS]);

        const s32 liResult = LobbyRankCategoryFetch(mpLobbyRank, lpcView,
                                                    FetchCategoriesCallback, this);
        if (liResult < 0)
        {
            EndAction(liResult);
        }
    }

    void ServerInterfaceRankings::DownloadScoreboardData(const char** lapcUserNames, s32 liNumUsers)
    {
        CGS_ASSERT_AT(!IsBusy(),
            "!IsBusy()",
            CGS_ARTIST_FILE, 482 /* ARTIST 0x82891948 */);
        CGS_ASSERT_AT(mpLobbyRank != 0,
            "mpLobbyRank",
            CGS_ARTIST_FILE, 483 /* ARTIST 0x82891970 */);

        for (s32 liPlayerIndex = 0; liPlayerIndex < liNumUsers; ++liPlayerIndex)
        {
            CGS_ASSERT_AT(lapcUserNames[liPlayerIndex] != 0,
                "lppcUserListNames[ liPlayerIndex ] != NULL",
                CGS_ARTIST_FILE, 491 /* ARTIST 0x828919AC */);
        }

        LobbyRankFlush(mpLobbyRank, 0);

        meCurrentAction = E_ACTION_DOWNLOADING_RANK;
        StartActionCore(KAPC_ACTION_NAMES[E_ACTION_DOWNLOADING_RANK]);

        const s32 liResult = LobbyRankFetch(mpLobbyRank, 0, 0, -1, lapcUserNames, liNumUsers,
                                            FetchRankCallback, this);
        if (liResult < 0)
        {
            EndAction(liResult);
        }
    }

    void ServerInterfaceRankings::CancelCurrentActionAndInvalidateScoreboard()
    {
        if (LobbyRankCancelCB(mpLobbyRank) == 1 && IsBusy())
        {
            EndAction(0);
        }
    }

    void ServerInterfaceRankings::EndAction(s32 liError)
    {
        ServerInterfaceComponent::EndActionCore(ConvertError(liError, meCurrentAction));
        meCurrentAction = E_ACTION_COUNT;
    }

    EServerInterfaceError ServerInterfaceRankings::ConvertError(s32 liError, EAction leAction)
    {
        // One call expression: the count (last argument) is loaded first (ARTIST EndAction 0x82888FD0).
        return ServerInterfaceComponent::ConvertError(liError, KA_DS_ERROR_TABLE_LOOKUP[leAction].mpMappingTable,
                                                      KA_DS_ERROR_TABLE_LOOKUP[leAction].miNumMappings);
    }

    void ServerInterfaceRankings::FetchCategoriesCallback(DirtySock::LobbyRankT* /*lpLobbyRank*/,
                                                          LobbyApiMsgT* lpMsg, void* lpUserData)
    {
        ServerInterfaceRankings* lpThis = static_cast<ServerInterfaceRankings*>(lpUserData);
        CGS_ASSERT_STREAM_AT(lpMsg->kind == KI_MSG_KIND_CATEGORIES,
                             "Oh dear, ServerInterfaceRankings got a message that wasn't for me!",
                             CGS_ARTIST_FILE, 440);
        lpThis->EndAction(lpMsg->code);
    }

    void ServerInterfaceRankings::FetchRankCallback(DirtySock::LobbyRankT* /*lpLobbyRank*/,
                                                    LobbyApiMsgT* lpMsg, void* lpUserData)
    {
        ServerInterfaceRankings* lpThis = static_cast<ServerInterfaceRankings*>(lpUserData);
        CGS_ASSERT_STREAM_AT(lpMsg->kind == KI_MSG_KIND_LIST,
                             "Wrong messge sent to ServerInterfaceRankings::FetchRankCallback",
                             CGS_ARTIST_FILE, 526);
        lpThis->EndAction(lpMsg->code);
    }
}

#undef CGS_ARTIST_FILE
