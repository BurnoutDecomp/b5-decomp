#ifndef CGS_SERVER_INTERFACE_RANKINGS_H
#define CGS_SERVER_INTERFACE_RANKINGS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

struct LobbyApiMsgT;   // DirtySDK lobby message (vendor SDK, global namespace)

// ===========================================================================
// CgsNetwork::ServerInterfaceRankings
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceRankings.{h,cpp}
//
// The rankings server-interface component (the E_PREPARESTAGE_RANKINGS_COMPONENT /
// E_RELEASESTAGE_RANKINGS_COMPONENT stage owner; embedded by value as
// BrnServerInterfaceBase::mRankings). Like every other DirtySock component it is a
// polymorphic leaf over the shared ServerInterfaceComponent base.
//
// LAYOUT: the ServerInterfaceComponent base (+0x00..+0x0F), then
//   +0x10  mpServerInterface
//   +0x14  meCurrentAction
//   +0x18  mpLobbyRank       (the downloaded rank table; every getter reads it)
//   +0x1C  miUserType
// The leaf appends two virtuals to the component vtable: Suspend and Resume (the
// console Resume slot is the shared empty handler).
//
// The scalar deleting destructor @ 0x827DE238-sibling 0x827DE310 restores the shared
// component vtable slot (off_820CDBF8) at this+0, then conditionally frees -- i.e. the
// component carries no owned heap members of its own beyond the base layout. Any
// rankings-specific data members are not present in the available exports (the leaf's
// only emitted code path is the trivial scalar deleting destructor), so none are
// modelled here; they would GROW this home additively when their stores are recovered.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterface;
    namespace DirtySock { struct LobbyRankT; }

    class ServerInterfaceRankings : public ServerInterfaceComponent
    {
    public:
        enum EAction
        {
            E_ACTION_FETCH_CATEGORIES = 0,
            E_ACTION_FETCH_RANK       = 1,
            E_ACTION_COUNT            = 2,
        };

        ServerInterfaceRankings();

        // Scalar deleting destructor @ 0x827DE310 (restores off_820CDBF8, conditional free).
        virtual ~ServerInterfaceRankings();

        // Declared-only lifecycle virtuals (bodies owned by dedicated component TUs;
        // not bodied here). Mirror the ServerInterfaceComponent vtable order.
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
        virtual void Suspend();
        virtual void Resume();

        bool Prepare(ServerInterface* lpServerInterface);
        void Update();
        bool Release();
        void Destruct();
        void DownloadScoreboardData(const char** lapcUserNames, s32 liNumUsers);

        // === ADDITIVE GROW (flagged by the BrnNetworkScoreboardManager group) ============
        // The downloaded-scoreboard query surface. The bodies live in this component's own
        // dossier (the X360 exports for these are RECOVERED -> CgsServerInterfaceRankings.cpp
        // but not yet committed); declared-only here so the ScoreboardManager call sites gate
        // under cl /c. Signatures are pinned from the BrnNetworkScoreboardManager X360 call
        // sites: PPC Hex-Rays drops the trailing index args, so they are restored from the
        // register usage at each call (e.g. GetColumnType(mpRankings, liColumn)).
        bool        IsBusy();                                        // meStatus != E_STATUS_IDLE (2)
        s32         GetNumberOfCategories();
        s32         GetNumberOfIndexes(s32 liCategory);
        s32         GetNumberOfVariations(s32 liCategory, s32 liIndex);
        // Heading-name getters (pinned from the BrnNetwork::ScoreboardManager::CopyCategories /
        // CopyIndexes X360 call sites @ 0x82562590 / 0x82562638 -- GetCategoryName(liCategory) and
        // GetIndexName(liCategory, liIndex) feed NetworkOutScoreboardHeadingList::AddHeading).
        const char* GetCategoryName(s32 liCategory);
        const char* GetIndexName(s32 liCategory, s32 liIndex);
        // Variation heading name (pinned from the BrnNetwork::ScoreboardManager::CopyVariations X360
        // call site @ 0x825626D8 -- GetVariationName(liCategory, liIndex, liVariation) feeds
        // NetworkOutScoreboardHeadingList::AddHeading). ADDITIVE GROW (BrnNetworkScoreboardManager TU).
        const char* GetVariationName(s32 liCategory, s32 liIndex, s32 liVariation);
        s32         GetNumberOfColumns();
        s32         GetNumberOfRows();
        s32         GetColumnType(s32 liColumn);
        s32         GetColumnStyle(s32 liColumn);
        s32         GetColumnWidth(s32 liColumn);
        const char* GetColumnTitle(s32 liColumn);
        bool        ScoreboardHasParam(s32 liParam);
        // Point the rankings component at the target scoreboard (category / index / variation).
        // Called by BrnNetwork::ScoreboardManager::HandleEvScoreTargetEvent. Declared-only here;
        // body lands with this component's own TU. ADDITIVE GROW (BrnNetworkScoreboardManager TU).
        void        SelectScoreboard(s32 liCategory, s32 liIndex, s32 liVariation);
        s32         GetRowThatContainsLocalUser();
        void        GetCell(s32 liColumn, s32 liRow, char* lpcBuffer, s32 liBufferSize);
        s32         GetUserType(s32 liVariation, const char** lapcUserListNames,
                                s32* lpiUserListCount) const;
        bool        DownloadHeadings(void* lpHeadingType);
        void        CancelCurrentActionAndInvalidateScoreboard();
        // =================================================================================

    private:
        void EndAction(s32 liError);
        static void FetchCategoriesCallback(DirtySock::LobbyRankT* lpLobbyRank, LobbyApiMsgT* lpMsg,
                                            void* lpUserData);
        static void FetchRankCallback(DirtySock::LobbyRankT* lpLobbyRank, LobbyApiMsgT* lpMsg,
                                      void* lpUserData);

        ServerInterface*       mpServerInterface;   // +0x10
        EAction                meCurrentAction;     // +0x14
        DirtySock::LobbyRankT* mpLobbyRank;         // +0x18
        s32                    miUserType;          // +0x1C
    };
}

#endif // CGS_SERVER_INTERFACE_RANKINGS_H
