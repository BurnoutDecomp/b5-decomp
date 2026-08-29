#pragma once

// ===================================================================================
// BrnGui::RivalMapPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnRivalMapPanel.h
//
// The crash-nav map's rival panel: four rival text fields plus a car-image icon, driven
// either from the local player's own cached stats (offline) or from a named online rival.
//
// DECFIGS DWARF:
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnRivalMapPanel.h
// It supplies `struct BrnGui::RivalMapPanel : public BrnGui::IconComponent`, the ERivalType
// enum (h:51) and the member run mTextfields[4] / mCarImageIcon / mPlayerName /
// meCurrentRivalType / mRivalID / mOnlineRivalID / mPlayerStats / mPlayerStatsReceived /
// mbActive.
//
// ⭐⭐ HEAD CARVE RETIRED 2026-08-29 (main-menu wave G1). The previous revision modelled
// everything before meCurrentRivalType as one opaque `u8 maHeadReserved[0x5D8]`, because no
// bodied function in scope touched it. RivalMapPanel::Construct @0x8243A1C0 now pins the
// whole run, member for member, and it is spelled out below:
//   +0x0000  IconComponent base        (`IconComponent::Construct(this, name, si, 0, parent)`
//                                        @0x8243A1DC -- no state-identifier table)
//   +0x0094  mTextfields[4]            (0x128 stride; the loop at 0x8243A1F4..0x8243A224 walks
//                                        `addi r29,r29,0x128` from this+0x94 while the name
//                                        pointer walks off_82F25170 -> off_82F25180)
//   +0x0534  mCarImageIcon             (IconComponent, "rivalCarIcon_cpt", then SetState
//                                        ("invisible") @0x8243A254)
//   +0x05C8  mPlayerName               (CgsNetwork::PlayerName, 16 bytes; `stb 0, 0x5C8`
//                                        @0x8243A278 blanks it, and SetRivalData's online
//                                        overload LobbyNameCmp's/Constructs it @0x82430D18)
//   +0x05D8  meCurrentRivalType        (`stw 4, 0x5D8` == E_RIVAL_TYPE_COUNT)
//   +0x05E0  mRivalID                  (`std 0, 0x5E0`; SetRivalData(CgsID) ld/std's it whole)
//   +0x05E8  mOnlineRivalID            (`std 0, 0x5E8`; the PlayerName overload writes it)
//   +0x05F0  mPlayerStats              (memset 0, 432 @0x8243A27C; StorePlayerInfo memcpy's here)
//   +0x07A0  mPlayerStatsReceived      (`stb 0, 0x7A0`)
//   +0x07A1  mbActive                  (`stb 0, 0x7A1`)
// (The host layout is name-based and every embedded pointer widens; the guest offsets above
// are documentation + the proof that each member is where the DWARF puts it.)
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // CgsID (== u64)
#include "GameSource/GameState/BrnCgsPlayerName.h"                      // CgsNetwork::PlayerName (by value)
#include "GameSource/Gui/BrnGuiTextField.h"                             // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"              // BrnGui::IconComponent (base + mCarImageIcon)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                         // BrnGui::GuiFlow
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::StateInterface

namespace BrnGui
{
    class GuiCache;

    class RivalMapPanel : public IconComponent
    {
        // CrashNavPanel::ChangeVisiblePanelState @0x8243A548 INLINES the "re-show the offline
        // rival panel" arm as three direct stores into this panel (a byte compare at +0x7A1, a
        // word compare/store at +0x5D8 and a SetState("transInRival")). Friending the one
        // console caller keeps that store-for-store rather than inventing accessor names for
        // members the DWARF gives no accessors for.
        friend class CrashNavPanel;

    public:
        // DWARF BrnRivalMapPanel.h:51 -- which rival the panel is showing. Corroborated by
        // CrashNavPanel: `li r5, 0` on the offline-player arm and `li r5, 3` on the online-rival
        // arm of SetRivalPanelData (@0x8243AAF4 / @0x8243AB0C), `li r5, 1` on the by-id overload
        // (@0x8243AB90), and E_RIVAL_TYPE_COUNT (4) used as the "no rival transition" sentinel.
        // Construct parks the panel on E_RIVAL_TYPE_COUNT.
        enum ERivalType
        {
            E_RIVAL_TYPE_OFFLINE_PLAYER = 0,
            E_RIVAL_TYPE_OFFLINE_RIVAL  = 1,
            E_RIVAL_TYPE_ONLINE_PLAYER  = 2,
            E_RIVAL_TYPE_ONLINE_RIVAL   = 3,
            E_RIVAL_TYPE_COUNT          = 4,
        };

        // The four text fields the panel drives, in construction order (their apt clip names
        // come from the file-static table in the .cpp).
        enum ETextField
        {
            E_TEXTFIELD_RIVAL_NAME = 0,
            E_TEXTFIELD_TEXT1      = 1,
            E_TEXTFIELD_TEXT2      = 2,
            E_TEXTFIELD_TEXT3      = 3,
            E_TEXTFIELD_COUNT      = 4,
        };

        // Size of the cached player-stats response event blob (X360 memcpy/memset size 0x1B0).
        static const s32 KI_STATS_RESPONSE_SIZE = 0x1B0;   // 432 bytes

        // @0x8243A1C0 (DWARF cpp:48) -- IconComponent virtual at vtable slot 0;
        // CrashNavPanel::Construct reaches it through the slot (`(**(this + 18480))
        // (this + 18480, "rivalPanel_mc", ..)` @0x82425F54).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82417738 (DWARF cpp:89).
        void AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache);

        // @0x824177C8 (DWARF cpp:115) -- redraw the panel from the CACHED player-stats
        // response. Offline-player mode only.
        void SetPlayerData(GuiCache* lpGuiCache);

        // @0x82430430 (DWARF cpp:168) -- the offline rival: show the rival's car by id.
        void SetRivalData(CgsID lRivalId);

        // @0x82430888 (DWARF cpp:240; the ledger carries the address un-named) -- the ONLINE
        // rival: the rival's car id plus the rival's display name.
        void SetRivalData(const CgsNetwork::PlayerName* lpName, CgsID lRivalId,
                          GuiCache* lpGuiCache);

        // @0x82417900 / @0x824179B0 (DWARF cpp:323/:355).
        void TransitionIn(ERivalType leRivalType);
        void TransitionOut();

        // @0x824176C0 -- copy the incoming player-stats response event into the panel's cache
        // and mark it valid. DWARF h:151 declares it
        // `void StorePlayerInfo(const GuiEventStatsResponse*)`; kept as `const void*` because
        // the committed caller (CrashNavPanel::RecEvent) forwards the raw queue record.
        void StorePlayerInfo(const void* lpStatsResponseEvent);

        // The panel's own face onto IconComponent::SetState(const char*), which is how the
        // console reaches it here (sub_824E2B90 @0x8243A79C / @0x8243A6F8 / this TU's own
        // "player" / "rival" / "transIn*" / "transOut*" pushes). Kept as a declared member --
        // and now BODIED as a one-line forwarder in the RivalMapPanel TU -- because the
        // measured link closure names `BrnGui::RivalMapPanel::SetState`, i.e. the committed
        // consumers were compiled against this face rather than the inherited one.
        void SetState(const char* lpacStateIdentifier);

    private:
        // ---- DWARF member run (X360 offsets are documentation only; access is BY NAME) ----
        TextField          maTextfields[E_TEXTFIELD_COUNT];  // +0x0094 (DWARF h:126)
        IconComponent      mCarImageIcon;                    // +0x0534 (DWARF h:127)
        CgsNetwork::PlayerName mPlayerName;                  // +0x05C8 (DWARF h:129)
        s32                meCurrentRivalType;               // +0x05D8 (DWARF h:131)
        CgsID              mRivalID;                         // +0x05E0 (DWARF h:132)
        CgsID              mOnlineRivalID;                   // +0x05E8 (DWARF h:133)
        u8                 maStatsResponse[KI_STATS_RESPONSE_SIZE];  // +0x05F0 (DWARF h:135)
        bool               mbHasPlayerInfo;                  // +0x07A0 (DWARF h:136)
        bool               mbActive;                         // +0x07A1 (DWARF h:138)
    };
}
