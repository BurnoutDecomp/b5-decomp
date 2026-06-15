#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::ScreenLoading - the in-flow loading-screen GUI state (distinct from the
// boot loading state). Tracks online-join bookkeeping and whether the loading
// screen is currently playing. Layout/virtual set from the DecFIGS DWARF
// (BrnScreenLoading.h / .cpp).
namespace BrnGui
{
    class GuiCache;

    struct ScreenLoading : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        void ApplyOptionsDataProfileSettings();

        static const s32 maiEventToObserve[2];   // @ 0x820666FC (.rdata)
        static const s32 miNumEventsObserved;    // == 2

        bool       mbInviteInProgress;
        bool       mbStartingGameDueToPlayerJoin;
        GuiCache*  mpGuiCache;
        bool       mbScreenPlaying;
    };
}
