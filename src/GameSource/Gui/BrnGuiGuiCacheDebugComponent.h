#pragma once

// BrnGuiGuiCacheDebugComponent.h
// Home of BrnGui::GuiCacheDebugComponent -- the debug component that overlays the GUI cache's
// status (and the Apt components' view of it) on the HUD. Derives from CgsDev::DebugComponent.
// Members in declaration order; the console puts the first one right after the base (+0x0C):
//   mpGuiModule +0x0C  miDisplayMode +0x10  mbShowGuiCacheStatus +0x14
//   mbShowAptComponentGuiCacheStatus +0x15  maStringList[5] +0x18  mInputQueue +0x40
// Only Construct and Update are emitted as standalone functions in the target build.

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
        static const s32 KI_NUM_DISPLAY_MODES = 5;

        // Bind the GUI module, reset the display mode and both status toggles, construct the
        // input queue.
        void Construct(GuiModule* lpGuiModule);

        // Per frame: hand the queued events to the GUI module's input buffer.
        void Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer);

    private:
        GuiModule*                 mpGuiModule;
        s32                        miDisplayMode;
        bool                       mbShowGuiCacheStatus;
        bool                       mbShowAptComponentGuiCacheStatus;
        CgsDev::DebugUI::StringList maStringList[KI_NUM_DISPLAY_MODES];
        CgsModule::VariableEventQueue<18432, 16> mInputQueue;   // InputBuffer::GuiEventQueue
    };
}
