#pragma once

// BrnGuiHudMessagesDebugComponent.h
// Home of BrnGui::GuiHudMessagesDebugComponent -- the in-game debug component that lets a
// developer trigger / step through HUD messages from the debug menu. Derives from
// CgsDev::DebugComponent (the debug-menu plumbing). Layout + method set from the DecFIGS
// DWARF (GameSource/Gui/BrnGuiHudMessagesDebugComponent.h:47/:76..:79).
//
// This slice reconstructs the two out-of-line virtual identity overrides the X360 ARTIST
// build emits for this component:
//
//   GetName @ 0x827DD220 -> "Hud Messages"  (the debug-menu label)
//   GetPath @ 0x824ED348 -> "Gui"           (the debug-menu group path)
//
// The remaining members (Construct/Destruct/TriggerMessage/SelectNext.../the callbacks)
// are declared so the class shape is faithful, but their bodies live in this component's
// .cpp TU and reach the HudMessageDirector/HudMessageController, which are out of scope
// here -- only the two identity overrides are attributed to this slice.

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent

namespace BrnGui
{
    class HudMessageDirector;
    class HudMessageController;

    // BrnGuiHudMessagesDebugComponent.h:47
    struct GuiHudMessagesDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(HudMessageDirector* lpMessageDirector);
        void Destruct();

        // BrnGuiHudMessagesDebugComponent.h:164/:151
        void SetController(const HudMessageController* lpMessageController);
        bool ControllerIsValid() const;

    protected:
        void OnActivate() override;

        // @ 0x827DD220 -- debug-menu label.
        const char* GetName() const override;
        // @ 0x824ED348 -- debug-menu group path.
        const char* GetPath() const override;
        bool        IsSimple() const override;

        void TriggerMessage(bool lbWithTimer);
        void SelectNextMessage();
        void SelectPreviousMessage();
        void TriggerMessageCallback(void* lpUserData);
        void TriggerMessageWithoutTimerCallback(void* lpUserData);
        void SelectNextMessageCallback(void* lpUserData);
        void SelectPreviousMessageCallback(void* lpUserData);

        // BrnGuiHudMessagesDebugComponent.h:76..:79
        HudMessageDirector*         mpMessageDirector;
        const HudMessageController* mpMessageController;
        s32                         miMessageId;
        s32                         miMessageGroup;
    };
}
