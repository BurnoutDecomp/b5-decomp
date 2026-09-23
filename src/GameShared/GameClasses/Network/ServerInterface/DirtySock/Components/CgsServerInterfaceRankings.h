#ifndef CGS_SERVER_INTERFACE_RANKINGS_H
#define CGS_SERVER_INTERFACE_RANKINGS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "lobbyrank.h"   // LobbyRankT, LobbyApiMsgT, the LobbyRank* C API

// ===========================================================================
// CgsNetwork::ServerInterfaceRankings
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceRankings.{h,cpp}
//
// The rankings server-interface component (the E_PREPARESTAGE_RANKINGS_COMPONENT /
// E_RELEASESTAGE_RANKINGS_COMPONENT stage owner; embedded by value as
// BrnServerInterfaceBase::mRankings). It drives the DirtySDK LobbyRank module: the
// category/index/variation heading download, the scoreboard (rank list) download and the
// column/row/cell queries over the downloaded list.
//
// LAYOUT: the ServerInterfaceComponent base (+0x00..+0x0F), then
//   +0x10  mpServerInterface
//   +0x14  meCurrentAction
//   +0x18  mpLobbyRank       (the DirtySDK rank module; every getter reads it)
//   +0x1C  miUserType
// The leaf appends two virtuals to the component vtable: Suspend and Resume.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterface;
    struct DSErrorToServerInterfaceErrorTable;
    namespace DirtySock { using ::LobbyRankT; }

    class ServerInterfaceRankings : public ServerInterfaceComponent
    {
    public:
        enum EAction
        {
            E_ACTION_START                = 0,
            E_ACTION_DOWNLOADING_HEADINGS = 0,
            E_ACTION_DOWNLOADING_RANK     = 1,
            E_ACTION_COUNT                = 2,
        };

        ServerInterfaceRankings();

        virtual ~ServerInterfaceRankings();

        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
        virtual void Suspend();
        virtual void Resume();

        bool Prepare(ServerInterface* lpServerInterface);
        void Update();
        bool Release();
        void Destruct();
        void DownloadScoreboardData(const char** lapcUserNames, s32 liNumUsers);

        // The downloaded-scoreboard query surface (the BrnNetworkScoreboardManager call sites).
        // Busy while an action is in flight (meStatus is not idle).
        bool        IsBusy() { return meStatus != 2; }
        s32         GetNumberOfCategories();
        s32         GetNumberOfIndexes(s32 liCategory);
        s32         GetNumberOfVariations(s32 liCategory, s32 liIndex);
        const char* GetCategoryName(s32 liCategory);
        const char* GetIndexName(s32 liCategory, s32 liIndex);
        const char* GetVariationName(s32 liCategory, s32 liIndex, s32 liVariation);
        s32         GetNumberOfColumns();
        s32         GetNumberOfRows();
        s32         GetColumnType(s32 liColumn);
        s32         GetColumnStyle(s32 liColumn);
        s32         GetColumnWidth(s32 liColumn);
        const char* GetColumnTitle(s32 liColumn);
        bool        ScoreboardHasParam(s32 liParam);
        // Point the rankings component at the target scoreboard (category / index / variation)
        // and latch the selected variation's user type.
        void        SelectScoreboard(s32 liCategory, s32 liIndex, s32 liVariation);
        s32         GetRowThatContainsLocalUser();
        void        GetCell(s32 liColumn, s32 liRow, char* lpcBuffer, s32 liBufferSize);
        // The user type SelectScoreboard latched for the selected variation.
        s32         GetUserType() const { return miUserType; }
        // Start the category/heading download of the named rank view.
        void        DownloadHeadings(const char* lpcView);
        void        CancelCurrentActionAndInvalidateScoreboard();

    private:
        void EndAction(s32 liError);
        static void FetchCategoriesCallback(DirtySock::LobbyRankT* lpLobbyRank, LobbyApiMsgT* lpMsg,
                                            void* lpUserData);
        static void FetchRankCallback(DirtySock::LobbyRankT* lpLobbyRank, LobbyApiMsgT* lpMsg,
                                      void* lpUserData);

        static const DSErrorToServerInterfaceErrorTable KA_DS_ERROR_TABLE_LOOKUP[E_ACTION_COUNT];
        static const char* KAPC_ACTION_NAMES[E_ACTION_COUNT];

        ServerInterface*       mpServerInterface;   // +0x10
        EAction                meCurrentAction;     // +0x14
        DirtySock::LobbyRankT* mpLobbyRank;         // +0x18
        s32                    miUserType;          // +0x1C
    };
}

#endif // CGS_SERVER_INTERFACE_RANKINGS_H
