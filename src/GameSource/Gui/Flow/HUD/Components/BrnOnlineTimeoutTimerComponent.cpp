// ===================================================================================
// BrnGui::OnlineTimeoutComponent -- implementation
//   class:BrnGui::OnlineTimeoutComponent
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// component's virtual apt-view-state setter (vtable byte offset +0xC == slot index 3) is
// dispatched through the modelled vtable, matching the X360 `(*(*this + 12))(this, <state>)`.
// ===================================================================================
#include "GameSource/Gui/Flow/Hud/Components/BrnOnlineTimeoutTimerComponent.h"

namespace BrnGui
{
    namespace
    {
        // The virtual apt-view-state setter: `void (*)(OnlineTimeoutComponent*, const char*)`.
        // X360 dispatch: load vtable from *this, call the entry at byte offset +0xC (slot
        // index 3 of the pointer table), passing `this` and the state string.
        typedef void (*SetAptViewStateFn)(OnlineTimeoutComponent*, const char*);

        inline void SetAptViewState(OnlineTimeoutComponent* lpThis, const char* lpacState)
        {
            SetAptViewStateFn lpfSet =
                reinterpret_cast<SetAptViewStateFn>(lpThis->mppVTable[3]);  // +0xC == slot 3
            lpfSet(lpThis, lpacState);
        }
    }

    // @ 0x824735F0 -- publish "visible", then mark the component shown (stb 1, 0xA5).
    void OnlineTimeoutComponent::Show()
    {
        SetAptViewState(this, "visible");
        mbActive = true;
    }

    // @ 0x82410A18 -- publish "transin", then mark the component shown (stb 1, 0xA5).
    void OnlineTimeoutComponent::Transin()
    {
        SetAptViewState(this, "transin");
        mbActive = true;
    }

    // @ 0x82410A60 -- publish "invisible", then clear the shown flag (stb 0, 0xA5).
    void OnlineTimeoutComponent::Transout()
    {
        SetAptViewState(this, "invisible");
        mbActive = false;
    }
}
