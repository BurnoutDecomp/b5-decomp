#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)

// BrnGui::ComplexBar - a multi-segment apt bar component (the red car-stats bar,
// boost bars, ...) that runs animated transitions between segment indices, with a
// queued follow-up index and an optional colour override. NOW the full DWARF class
// (BrnComplexBar.h:43/:92-:97); previously a minimal PlayerStatsBar-facing slice.
// This TU bodies Construct/UpdateFlash/SetColour/HandleTransitionComplete;
// SetTo/RunTo/Increment are their own ledger functions (declaration-only).
namespace BrnGui
{
    class ComplexBar : public CgsGui::GuiComponent
    {
    public:
        static const u32 KU_MAX_COLOUR_LEN = 16;   // DWARF h:45

        // @0x824E5890 (this TU, DWARF cpp:42) -- vtable slot 0 (PlayerStatsBar
        // dispatches it with (name, iface, parent)).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // DWARF h:118/:139/:170 -- their own ledger functions (declaration-only).
        void SetTo(s32 liIndex);
        void RunTo(s32 liIndex);
        void Increment(s32 liDelta);

        // @0x824E5988 (this TU) -- format the colour and push the apt colour states.
        void SetColour(u32 luColour);

        // @0x824E5A10 (this TU, cpp:116) -- a bar transition finished at liBarSize.
        void HandleTransitionComplete(s32 liBarSize);

    private:
        // @0x824E58F8 (this TU) -- push the current/target indices as the apt
        // start/end states and latch the in-progress flag.
        void UpdateFlash(bool lbTransitionInProgress);

        // DWARF h:92..:97 (X360 offsets after the 0x8C GuiComponent base).
        s32  miCurrentIndex;             // :92  +0x8C (140)
        s32  miTargetIndex;              // :93  +0x90 (144)
        s32  miQueuedIndex;              // :94  +0x94 (148)
        bool mbTransitionInProgress;     // :95  +0x98 (152)
        char macColour[KU_MAX_COLOUR_LEN]; // :97  +0x99 (153) .. +0xA8
    };
}
