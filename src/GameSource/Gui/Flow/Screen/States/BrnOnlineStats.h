#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiCache.h"                                          // BrnGui::GuiCache / GuiFlow
#include "GameSource/Gui/BrnGuiTextField.h"                                      // BrnGui::TextField (embedded stat-field bank)
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                      // GuiEventControllerInputPressed

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlineStats.h
//
// BrnGui::OnlineStats - the online-stats screen state (DWARF home BrnOnlineStats.h:43,
// base CgsGui::State). A load/init flow state that streams its one resource tuple, waits
// for its apt components, then renders six online-stat counters (total games / win rate /
// takedowns / rivals / mugshots / disconnect rate) into an embedded text-field bank.
//
// Class shape (virtual layout + members + byte offsets) and the EInternalState enum are
// from the DecFIGS DWARF (BrnOnlineStats.h), gated on the X360 ledger. mpStateInterface is
// INHERITED from CgsGui::State (base +0x1C); class-local members begin at mpGuiCache.
// ============================================================================
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;

    // The online-stats response payload: six s32 counters read at event+0..+0x14 by
    // HandleStatsData. Field names INFERRED from the SetText target order; the six-word span
    // (+0..+0x14) is X360-attested.
    struct GuiEventOnlineStatsResponse
    {
        s32 miTotalGames;     // +0x00
        s32 miWinRate;        // +0x04
        s32 miTakedowns;      // +0x08
        s32 miRivals;         // +0x0C
        s32 miMugshots;       // +0x10
        s32 miDisconnectRate; // +0x14
    };

    // The "please fetch my online stats" request the RUNNING state posts on the output
    // queue (channel 40) when it receives the trigger event id 0xF1. X360-attested by
    // OnlineStats::UpdateRunning @0x824A19C8: the queued record is a GuiEvent<242> header
    // { muHeader0 = 24, muEventType = 242, muHeader2 = 12 } followed by six stat-id words
    // { 37, 42, 67, 41, 15, 7 }, 36 bytes total. (muHeader0 = 24 = payload byte count,
    // muHeader2 = 12 = payload offset -- the same header idiom as the other GuiEvent<N>s.)
    struct GuiEventOnlineStatsRequest : public CgsGui::GuiEvent<242>
    {
        u32 mauStatIds[6];   // +0x0C .. +0x23

        GuiEventOnlineStatsRequest()
            : CgsGui::GuiEvent<242>(24, 12)
        {
            mauStatIds[0] = 37;
            mauStatIds[1] = 42;
            mauStatIds[2] = 67;
            mauStatIds[3] = 41;
            mauStatIds[4] = 15;
            mauStatIds[5] = 7;
        }
    };

    struct OnlineStats : public CgsGui::State
    {
        // DWARF BrnOnlineStats.h:70.
        enum EInternalState
        {
            E_INTERNALSTATE_GETCACHE        = 0,
            E_INTERNALSTATE_LOADRESOURCES   = 1,
            E_INTERNALSTATE_WFDATA          = 2,
            E_INTERNALSTATE_PLAYSWF         = 3,
            E_INTERNALSTATE_WFINIT          = 4,
            E_INTERNALSTATE_SETUPCOMPONENTS = 5,
            E_INTERNALSTATE_RUNNING         = 6,
            E_INTERNALSTATE_LEFT            = 7,
            E_INTERNALSTATE_COUNT           = 8,
        };

        // DWARF h:93 -- the expected-component list bound.
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 4;

        // @ 0x82487470 / 0x824A18E8 -- the FSM virtuals.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825004C0 - hands the online-stats screen's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- out-of-line ledger functions (BrnOnlineStats.cpp) ----
        void HandleControllerInputPressed(const CgsGui::GuiEventControllerInputPressed* lpEvent);  // @0x82487670
        void HandleStatsData(const GuiEventOnlineStatsResponse* lpEvent);                  // @0x82487728
        void ClearExpectedComponent();                                                     // @0x82487590 (SKIPPED; declared-only)
        void UpdateGetCache();                                                             // @0x82492820
        bool UpdateWFInit();                                                               // @0x82487608
        void UpdateRunning();                                                              // @0x824A19C8
        void UpdatePermanent();                                                            // @0x82492908

        // ---- statics (DWARF cpp:28-58) ----
        static const s32                    maiEventToObserve[];   // @0x8205F9FC (7 entries; contents not decoded)
        static const s32                    miNumEventsObserved;   // == 7
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @0x8205FA1C (.rdata)
        static const u32                    muNumResourcesToLoad;  // @0x8205FA24 == 1

        // The apt-clip names bound in OnEnter (DWARF cpp:53-58).
        static const char* const KAC_TEXTFIELD_NAME_TOTAL_GAMES;     // "TotalGames_mc"
        static const char* const KAC_TEXTFIELD_NAME_WIN_RATE;        // "WinRate_mc"
        static const char* const KAC_TEXTFIELD_NAME_TAKEDOWNS;       // "Takedowns_mc"
        static const char* const KAC_TEXTFIELD_NAME_RIVALS;          // "Rivals_mc"
        static const char* const KAC_TEXTFIELD_NAME_MUGSHOTS;        // "Mugshots_mc"
        static const char* const KAC_TEXTFIELD_NAME_DISCONNECT_RATE; // "DisconnectRate_mc"

        // The controller-action sub-id that backs out of the screen (asm cmpwi 0x32).
        static const s32 KI_ACTION_START = 50;
        // The permanently-observed "go back" event id drained by UpdatePermanent.
        static const s32 KI_EVENT_GO_BACK = 6;
        // In-queue event ids handled by the RUNNING state (asm cmpwi 6 / 0xF1 / 0xF2) and
        // the cache-ready event scanned for by UpdateGetCache (asm cmpwi 0x40).
        static const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;
        static const s32 KI_EVENT_REQUEST_STATS            = 241;   // 0xF1
        static const s32 KI_EVENT_STATS_RESPONSE           = 242;   // 0xF2
        static const s32 KI_EVENT_CACHE_READY              = 64;    // 0x40

        // ---- members (DWARF; X360 byte offsets) ----
        GuiCache*      mpGuiCache;                                 // X360 +0x38
        EInternalState meInternalState;                           // X360 +0x3C
        u32            mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM]; // X360 +0x40 (x4)
        u32            muNumExpectedComponents;                   // X360 +0x50
        TextField      mTotalGames;      // X360 +0x54
        TextField      mWinRate;         // X360 +0x17C
        TextField      mTakedowns;       // X360 +0x2A4
        TextField      mRivals;          // X360 +0x3CC
        TextField      mMugshots;        // X360 +0x4F4
        TextField      mDisconnectRate;  // X360 +0x61C
    };
}
