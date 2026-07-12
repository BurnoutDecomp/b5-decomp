#pragma once

// ===================================================================================
// GameSource/Gui/Flow/Screen/States/BrnOnlinePreEvent.h
//
// BrnGui::OnlinePreEvent - the online pre-event ("ready up" / fly-by) screen state. A
// CgsGui::State leaf: on entry it constructs the pre-event messages component and drives
// an internal state machine (get-cache -> load-resources -> wait-for-init -> running)
// that plays the "ON_PRE_EVENT" apt movie, then, once the cache/components are up, shows
// timed fly-by messages for each event in the online line-up.
//
// Class shape (virtual layout, member set, internal-state enum) is from the DecFIGS DWARF
// (BrnOnlinePreEvent.h), gated on the X360 ledger. The GetResourcesToLoad accessor is the
// one header-attributed ledger function; the out-of-line virtual/helper bodies live in
// BrnOnlinePreEvent.cpp.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnOnlinePreEventMessages.h" // mMessageComponent (embedded by value)

namespace BrnGui { class GuiCache; }
namespace BrnGui { struct GuiEventControllerInputPressed; }  // declaration-only consumer param
namespace BrnGui { struct GuiEventAptTrigger; }              // declaration-only consumer param

namespace BrnGui
{
    struct OnlinePreEvent : public CgsGui::State
    {
        // DWARF BrnOnlinePreEvent.h:70 -- the entry state's internal load/run machine.
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE      = 0,
            E_INTERNALSTATE_LOADRESOURCES = 1,
            E_INTERNALSTATE_WFINIT        = 2,
            E_INTERNALSTATE_RUNNING       = 3,
            E_INTERNALSTATE_LEFT          = 4,
            E_INTERNALSTATE_COUNT         = 5,
        };

        // @ 0x824872D8 - register for events, construct the messages component, reset the
        // fly-by machine to E_INTERNALSTATE_GETCACHE.
        virtual void OnEnter();

        // @ 0x824A0EB0 - unregister, play the leave apt movie, mark E_INTERNALSTATE_LEFT.
        virtual void OnLeave();

        // Per-frame tick (dispatches the internal-state machine + UpdatePermanent). Body
        // lands with its own ledger slice; declared here for the vtable shape.
        virtual void Update();

        // @ 0x82500698 - hands the online pre-event screen's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- internal-state-machine steps (DWARF BrnOnlinePreEvent.h; private) ----
        // @ 0x824925C0 - scan the in-event queue for the GuiCache event (type 64) and latch it.
        void UpdateGetCache();
        // @ 0x824A0F40 - once the cache's resources are loaded, play "ON_PRE_EVENT" (level 3).
        bool UpdateLoadResources();
        // Wait for the apt components to finish initialising. Body lands with its own slice.
        bool UpdateWFInit();
        // @ 0x82487360 - hide the current fly-by message once its display time elapses.
        void UpdateRunning();
        // @ 0x824A1020 - drain the in-event queue every frame (load-notify / lost-conn /
        // fly-by-show / advance).
        void UpdatePermanent();

        // Declared-only consumers (bodies land with their own slices).
        void HandleControllerInputPressed(const GuiEventControllerInputPressed* lpEvent);
        void HandleAptTriggers(const GuiEventAptTrigger* lpEvent);

        // ---- statics (DWARF cpp:26-46; .rdata) ----
        static const s32                    maiEventToObserve[6];             // @0x8205F978 (6 entries)
        static const s32                    miNumEventsObserved;              // == 6
        static const CgsGui::sResourceTuple maResourcesToLoad[];             // @0x8205F994 (1 entry)
        static const u32                    muNumResourcesToLoad;             // @0x8205F99C == 1
        static const char                   KAC_MESSAGES_COMPONENT_NAME[20];  // @0x8205... "PreEventMessages_mc"
        // The idle sentinel mfTimeToRemove holds when no fly-by is scheduled (X360 rodata
        // flt_82001CC0). It must exceed any GetTime() so UpdateRunning does not re-fire once
        // a message is hidden; FLAG: exact rodata value not recovered, modelled as FLT_MAX.
        static const f32                    KF_FLYBY_IDLE_TIME;

        // ---- members (DWARF h:81-94; X360 offsets) ----
        GuiCache*              mpGuiCache;          // +0x38
        InternalState          meInternalState;     // +0x3C
        OnlinePreEventMessages mMessageComponent;   // +0x40
        f32                    mfTimeToRemove;       // +0x448
        s32                    miCurrentFlyByIndex;  // +0x44C
    };
}
