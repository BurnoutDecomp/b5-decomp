#ifndef CGS_SERVER_INTERFACE_RANKINGS_H
#define CGS_SERVER_INTERFACE_RANKINGS_H

#include "types.hpp"

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
// LAYOUT: the leading members mirror the committed ServerInterfaceComponent base
// (CgsServerInterfaceComponent.h) exactly. Modelled here as the leading members --
// rather than via C++ inheritance -- to stay member-by-name without re-forking that
// type's standalone header, matching the existing sibling-component homes
// (CgsServerInterfaceServerInfo.h) in this directory:
//   +0x00  vptr
//   +0x04  mpcCurrentAction  (const char*)
//   +0x08  meStatus          (s32; 2 == "no error")
//   +0x0C  miLastError       (s32)
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
    // Event enum used by the OnEvent vtable slot (full set in the events home).
    enum EServerInterfaceEvent : s32;

    class ServerInterfaceRankings
    {
    public:
        ServerInterfaceRankings();

        // Scalar deleting destructor @ 0x827DE310 (restores off_820CDBF8, conditional free).
        virtual ~ServerInterfaceRankings();

        // Declared-only lifecycle virtuals (bodies owned by dedicated component TUs;
        // not bodied here). Mirror the ServerInterfaceComponent vtable order.
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

        // === ADDITIVE GROW (flagged by the BrnNetworkScoreboardManager group) ============
        // The downloaded-scoreboard query surface. The bodies live in this component's own
        // dossier (the X360 exports for these are RECOVERED -> CgsServerInterfaceRankings.cpp
        // but not yet committed); declared-only here so the ScoreboardManager call sites gate
        // under cl /c. Signatures are pinned from the BrnNetworkScoreboardManager X360 call
        // sites: PPC Hex-Rays drops the trailing index args, so they are restored from the
        // register usage at each call (e.g. GetColumnType(mpRankings, liColumn)).
        bool        IsBusy() const;                                        // meStatus != E_STATUS_IDLE (2)
        s32         GetNumberOfCategories() const;
        s32         GetNumberOfIndexes(s32 liCategory) const;
        s32         GetNumberOfVariations(s32 liCategory, s32 liIndex) const;
        // Heading-name getters (pinned from the BrnNetwork::ScoreboardManager::CopyCategories /
        // CopyIndexes X360 call sites @ 0x82562590 / 0x82562638 -- GetCategoryName(liCategory) and
        // GetIndexName(liCategory, liIndex) feed NetworkOutScoreboardHeadingList::AddHeading).
        const char* GetCategoryName(s32 liCategory) const;
        const char* GetIndexName(s32 liCategory, s32 liIndex) const;
        // Variation heading name (pinned from the BrnNetwork::ScoreboardManager::CopyVariations X360
        // call site @ 0x825626D8 -- GetVariationName(liCategory, liIndex, liVariation) feeds
        // NetworkOutScoreboardHeadingList::AddHeading). ADDITIVE GROW (BrnNetworkScoreboardManager TU).
        const char* GetVariationName(s32 liCategory, s32 liIndex, s32 liVariation) const;
        s32         GetNumberOfColumns() const;
        s32         GetNumberOfRows() const;
        s32         GetColumnType(s32 liColumn) const;
        s32         GetColumnStyle(s32 liColumn) const;
        s32         GetColumnWidth(s32 liColumn) const;
        const char* GetColumnTitle(s32 liColumn) const;
        bool        ScoreboardHasParam(s32 liParam) const;
        // Point the rankings component at the target scoreboard (category / index / variation).
        // Called by BrnNetwork::ScoreboardManager::HandleEvScoreTargetEvent. Declared-only here;
        // body lands with this component's own TU. ADDITIVE GROW (BrnNetworkScoreboardManager TU).
        void        SelectScoreboard(s32 liCategory, s32 liIndex, s32 liVariation);
        s32         GetRowThatContainsLocalUser() const;
        void        GetCell(s32 liColumn, s32 liRow, char* lpcBuffer, s32 liBufferSize) const;
        s32         GetUserType(s32 liVariation, const char** lapcUserListNames,
                                s32* lpiUserListCount) const;
        bool        DownloadHeadings(void* lpHeadingType);
        void        CancelCurrentActionAndInvalidateScoreboard();
        // =================================================================================

    private:
        // --- ServerInterfaceComponent base layout (see header note) ---
        const char* mpcCurrentAction;   // +0x04
        s32         meStatus;           // +0x08
        s32         miLastError;        // +0x0C
    };
}

#endif // CGS_SERVER_INTERFACE_RANKINGS_H
