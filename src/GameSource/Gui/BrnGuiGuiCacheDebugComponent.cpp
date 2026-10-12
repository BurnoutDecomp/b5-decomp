// BrnGuiGuiCacheDebugComponent.cpp
// BrnGui::GuiCacheDebugComponent -- Construct and Update.

#include "GameSource/Gui/BrnGuiGuiCacheDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"      // CgsGuiModuleIO::InputBuffer
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"   // Lock/UnlockBuffersForIO

namespace BrnGui
{
    // ARTIST 0x824F8060: register the two status actions, then the five-option display-mode
    // selector. The option values are the original All / Loading+Unloading / Loaded+Unloaded /
    // Loaded+Loading / Unloaded+Unloading modes, in that order.
    void GuiCacheDebugComponent::OnActivate()
    {
        RegisterFunction(&ShowGuiCacheStatusCallback, this, "Show Gui Cache Status");

        RegisterVariable(&miDisplayMode, "Display Mode");
        miDisplayMode = 0;
        maStringList[0] = {0, "All"};
        maStringList[1] = {1, "Loading+Unloading"};
        maStringList[2] = {2, "Loaded+Unloaded"};
        maStringList[3] = {3, "Loaded+Loading"};
        maStringList[4] = {4, "Unloaded+Unloading"};
        SetRange(&miDisplayMode, 0, 4);
        SetOptions(&miDisplayMode, maStringList);

        RegisterFunction(&ShowAptComponentGuiCacheStatusCallback, this,
                         "Show Apt Component Gui Cache Status");
    }

    // ARTIST 0x824ED368 / 0x824ED410. These callbacks only flip their component-owned
    // presentation flags; the Apt reader/rendering path remains a separate dependency.
    void GuiCacheDebugComponent::ShowGuiCacheStatusCallback(void* lpUserData)
    {
        CGS_ASSERT(lpUserData != 0,
                   "ShowGuiCacheStatusCallback: Invalid data in the callback");
        GuiCacheDebugComponent* lpThis = static_cast<GuiCacheDebugComponent*>(lpUserData);
        lpThis->mbShowGuiCacheStatus = !lpThis->mbShowGuiCacheStatus;
    }

    void GuiCacheDebugComponent::ShowAptComponentGuiCacheStatusCallback(void* lpUserData)
    {
        CGS_ASSERT(lpUserData != 0,
                   "ShowAptComponentGuiCacheStatusCallback: Invalid data in the callback");
        GuiCacheDebugComponent* lpThis = static_cast<GuiCacheDebugComponent*>(lpUserData);
        lpThis->mbShowAptComponentGuiCacheStatus = !lpThis->mbShowAptComponentGuiCacheStatus;
    }

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
