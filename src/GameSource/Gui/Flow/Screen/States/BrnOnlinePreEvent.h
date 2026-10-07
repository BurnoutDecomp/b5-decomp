#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnOnlinePreEventMessages.h"   // by value

// BrnGui::OnlinePreEvent - the online pre-event (fly-by messages) screen state
// (ON_PRE_EVENT): each fly-by event shows the next pre-event message for its duration.
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace BrnGui
{
    class GuiCache;   // pointer member only

    struct OnlinePreEvent : public CgsGui::State
    {
        // The screen's internal state machine (console +0x3C).
        enum InternalState
        {
            E_INTERNALSTATE_GETCACHE      = 0,
            E_INTERNALSTATE_LOADRESOURCES = 1,
            E_INTERNALSTATE_WFINIT        = 2,
            E_INTERNALSTATE_RUNNING       = 3,
            E_INTERNALSTATE_LEFT          = 4,
            E_INTERNALSTATE_COUNT         = 5,
        };

        virtual void OnEnter();
        virtual void OnLeave();
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
        void UpdateGetCache();
        bool UpdateLoadResources();
        void UpdateRunning();
        void UpdatePermanent();

        static const s32                    maiEventToObserve[6];
        static const s32                    miNumEventsObserved;   // == 6
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x8205F994 (unk_8205F994, .rdata)
        static const u32                    muNumResourcesToLoad;  // @ 0x8205F99C (dword_8205F99C, .rdata) == 1
        static const char                   KAC_MESSAGES_COMPONENT_NAME[20];   // "PreEventMessages_mc"

        // ---- data members (declaration order; console offsets in the comments) -------
        GuiCache*              mpGuiCache;            // +0x38
        InternalState          meInternalState;       // +0x3C
        OnlinePreEventMessages mMessageComponent;     // +0x40
        f32                    mfTimeToRemove;        // +0x448
        s32                    miCurrentFlyByIndex;   // +0x44C
    };
}
