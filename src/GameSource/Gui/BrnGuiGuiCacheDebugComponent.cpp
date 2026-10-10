// BrnGuiGuiCacheDebugComponent.cpp
// BrnGui::GuiCacheDebugComponent -- Construct and Update.

#include "GameSource/Gui/BrnGuiGuiCacheDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"      // CgsGuiModuleIO::InputBuffer
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"   // Lock/UnlockBuffersForIO

namespace BrnGui
{
    void GuiCacheDebugComponent::Construct(GuiModule* lpGuiModule)
    {
        CGS_ASSERT(lpGuiModule != 0, "Invalid Gui module passed to GuiCacheDebugComponent::Construct");

        CgsDev::DebugComponent::Construct();
        mpGuiModule                      = lpGuiModule;
        mbShowGuiCacheStatus             = false;
        miDisplayMode                    = 0;
        mbShowAptComponentGuiCacheStatus = false;
        mInputQueue.Construct();
    }

    // Append the queued events to the GUI module's input events under the buffer's write lock,
    // then clear the local queue.
    void GuiCacheDebugComponent::Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer)
    {
        CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer");
        CgsModule::LockBuffersForIO(lpInputBuffer);
        lpInputBuffer->GetGuiEvents()->Append(mInputQueue);
        CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer");
        CgsModule::UnlockBuffersForIO(lpInputBuffer);

        mInputQueue.Clear();
    }
}
