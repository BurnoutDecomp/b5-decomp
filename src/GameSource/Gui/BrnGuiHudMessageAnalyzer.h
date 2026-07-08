#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                // CgsID
#include "GameShared/GameClasses/Numeric/CgsRandom.h"      // CgsNumeric::Random (embedded)
#include "GameSource/BurnoutConstants.h"                   // EActiveRaceCarIndex
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"            // GuiHudMessage / GuiLiveRevengeUpdateEvent

// ============================================================================
// GameSource/Gui/BrnGuiHudMessageAnalyzer.h
//
// BrnGui::HudMessageAnalyzer - watches the GUI event stream and decides which
// on-screen HUD messages to fire (takedowns, revenge updates, skill thresholds,
// eliminations, ...), forwarding them to the HudMessageDirector. Class shape /
// member names / enums from the DecFIGS DWARF (BrnGuiHudMessageAnalyzer.h:48),
// gated on the X360 ledger. This TU bodies TriggerMessage (both overloads),
// HandlePlayerEliminated and HandleLiveRevengeUpdate; the rest of the large DWARF
// surface (Update + the ~40 other Handle*/analysis methods and threshold statics)
// is owned by its remaining ledger functions and grows here additively.
//
// FLAG: only the members up to mbDelayedForAfterCrashHudMessagePending are
// declared (everything the four bodied functions touch, in DWARF order); the
// remaining DWARF tail (wreck/takedown/dirty-trick bookkeeping) lands with the
// analysis TUs. The class is used by-pointer by those consumers, so the partial
// tail does not shift any declared member.
// ============================================================================

namespace CgsGui { struct GuiAccessPointers; }

namespace BrnGui
{
    class GuiCache;
    struct HudMessageDirector;

    struct HudMessageAnalyzer
    {
        // DWARF h:62.
        enum EBoostEarningStatus
        {
            E_BOOST_EARNING_STATUS_OFF     = -2,
            E_BOOST_EARNING_STATUS_ON      = -1,
            E_BOOST_EARNING_STATUS_GOOD    = 0,
            E_BOOST_EARNING_STATUS_GREAT   = 1,
            E_BOOST_EARNING_STATUS_AWESOME = 2,
            E_BOOST_EARNING_STATUS_COUNT   = 3,
        };

        // DWARF h:83.
        enum EWreckedMessageState
        {
            E_WRECKED_STATE_NOT_WRECKED   = 0,
            E_WRECKED_STATE_WRECKING      = 1,
            E_WRECKED_STATE_WRECKED       = 2,
            E_WRECKED_STATE_WRECKED_STUNT = 3,
            E_WRECKED_STATE_WRECK_HANDLED = 4,
            E_WRECKED_STATE_WRECK_ABORTED = 5,
            E_WRECKED_STATE_WRECK_COUNT   = 6,
        };

        // @0x825179E8 (this TU, DWARF h:~737) -- build a parameterless message from
        // its id and hand it to the director.
        void TriggerMessage(const char* lpcMessageId);

        // @0x82517AF8 -- the prebuilt-message overload (assert the message + the
        // director, then AddMessage(message, false)). Its ledger row is unnamed;
        // recovered here alongside its callers.
        void TriggerMessage(const GuiHudMessage* lpMessage);

        // @0x8251B058 (this TU, DWARF cpp) -- "player eliminated" road-rage message
        // with the eliminated player's online name.
        void HandlePlayerEliminated(const s32* lpiEventPayload);

        // @0x8251E1F0 (this TU, DWARF cpp:3590s) -- live-revenge status-change
        // messages ("taken down X", "X got revenge", ...), aggressor/victim keyed
        // against the local player; delayed until after the crash cam when a wreck
        // is in progress.
        void HandleLiveRevengeUpdate(const GuiLiveRevengeUpdateEvent* lpEvent);

        // @0x8251FA90 -- online-stunt-run "rival/team-mate eliminated" HUD messages (keyed on
        // whether the local player was eliminated, the team-mate flag and the "last player
        // standing" flag; the remote case tags the message with the player's online name).
        void HandleOnlineStuntRunElimination(const GuiOnlineStuntRunEliminationEvent* lpEliminationEvent);

        // @0x8251FC08 -- online-stunt-run "now leading" HUD messages (same keying as elimination;
        // the remote-rival case tags the message with the player's online name).
        void HandleOnlineStuntRunLeading(const GuiOnlineStuntRunLeadingEvent* lpLeadingEvent);

        // @0x8251FD68 -- online-stunt-run "victory" HUD messages (same keying as leading).
        void HandleOnlineStuntRunVictory(const GuiOnlineStuntRunVictoryEvent* lpVictoryEvent);

        // @0x8251FEC8 -- online-stunt-run "last run" HUD message (parameterless "OnlSRLastRun").
        void HandleOnlineStuntRunLastRun(const GuiOnlineStuntRunLastRunEvent* lpLastRunEvent);

        // @0x8251FF38 -- online-stunt-run score/time notification HUD message (a team-scored /
        // rival-scored score line, or a "time" line, plus the score/time value as an int param).
        void HandleOnlineStuntRunMessage(const GuiOnlineStuntRunMessageEvent* lpMessageEvent);

    private:
        // DWARF h:300/h:303 -- their own ledger functions (declaration-only here).
        const char* GetOnlineName(EActiveRaceCarIndex leActiveRaceCarIndex, bool* lpbValid) const;

        // ---- members (DWARF h:97-112 order; the tail is the analysis TUs' concern) ----
        CgsGui::GuiAccessPointers* mpAccessPointers;              // DWARF h:97
        GuiCache*                  mpGuiCache;                    // DWARF h:98
        void*                      mpViewOutputQueue;             // DWARF h:102 (GuiEventQueueLarge*; opaque here)
        HudMessageDirector*        mpHudMessageDirector;          // DWARF h:103 (X360 +12)
        CgsNumeric::Random         mRandom;                       // DWARF h:105
        bool                       mbCrashBoundaryMessagePending; // DWARF h:107
        s32                        meCrashEntryState;             // DWARF h:108 (GuiPlayerCrashingStateChangeEvent::CrashBarState; raw s32 -- enum home pending)
        bool                       mbShowDrivableMessage;         // DWARF h:109
        GuiHudMessage              mDelayedForAfterCrashHudMessage;          // DWARF h:111 (X360 +80, 840B)
        bool                       mbDelayedForAfterCrashHudMessagePending;  // DWARF h:112 (X360 +920)
    };
}
