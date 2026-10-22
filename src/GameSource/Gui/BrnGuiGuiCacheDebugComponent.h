#pragma once

// BrnGuiGuiCacheDebugComponent.h
// Home of BrnGui::GuiCacheDebugComponent -- the debug component that overlays the GUI cache's
// status (and the Apt components' view of it) on the HUD. Derives from CgsDev::DebugComponent.
// Members in declaration order; the console puts the first one right after the base (+0x0C):
//   mpGuiModule +0x0C  miDisplayMode +0x10  mbShowGuiCacheStatus +0x14
//   mbShowAptComponentGuiCacheStatus +0x15  maStringList[5] +0x18  mInputQueue +0x40
// This provider proposal restores the original virtuals and both real HUD status helpers.
// Module construction and phase selection remain a separate owner decision.

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"       // CgsDev::DebugUI::StringList
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue

namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer; } }

namespace BrnGui
{
    class GuiModule;

    struct GuiCacheDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // ARTIST827E5D68/6C: derived vtable and actual queue-unconstructed byte only.
        GuiCacheDebugComponent();

        static const s32 KI_NUM_DISPLAY_MODES = 5;

        // Bind the GUI module, reset the display mode and both status toggles, construct the
        // input queue.
        void Construct(GuiModule* lpGuiModule);
        void Destruct();

        // Per frame: hand the queued events to the GUI module's input buffer.
        void Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer);

        // ARTIST vtable 0x820CE850 slot 2 -> 0x82505800.
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

    protected:
        // ARTIST vtable 0x820CE850 slots 3/4/6 -> 0x824ED358, ICF 0x824ED348, 0x824F8060.
        const char* GetName() const override;
        const char* GetPath() const override;
        void OnActivate() override;

        // ARTIST 0x824FD6B0 and 0x824F8148. Both are called only by RenderHUD.
        void ShowGuiCacheStatus(CgsDev::Debug2DImmediateRender* lpRender);
        void ShowAptComponentGuiCacheStatus(CgsDev::Debug2DImmediateRender* lpRender);

    private:
        // The debug-menu callback API takes `void(void*)`; ARTIST passes this object as user data.
        static void ShowGuiCacheStatusCallback(void* lpData);
        static void ShowAptComponentGuiCacheStatusCallback(void* lpData);
        GuiModule*                 mpGuiModule;
        s32                        miDisplayMode;
        bool                       mbShowGuiCacheStatus;
        bool                       mbShowAptComponentGuiCacheStatus;
        CgsDev::DebugUI::StringList maStringList[KI_NUM_DISPLAY_MODES];
        CgsModule::VariableEventQueue<18432, 16> mInputQueue;   // InputBuffer::GuiEventQueue
    };
}
