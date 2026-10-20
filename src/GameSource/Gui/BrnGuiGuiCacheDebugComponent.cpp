// BrnGuiGuiCacheDebugComponent.cpp
// BrnGui::GuiCacheDebugComponent -- Construct and Update.

#include "GameSource/Gui/BrnGuiGuiCacheDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h" // HUD status text
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"      // CgsGuiModuleIO::InputBuffer
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"   // Lock/UnlockBuffersForIO
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiModule.h"

// X360 0x82824048: the shared debug-HUD text helper already lives in
// CgsDebug2DImmediateRender.cpp; keep its original no-screen-check behavior.
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCheckScreen);

namespace BrnGui
{
    // Original C++ initialization writes no module/toggle/status/base data.
    GuiCacheDebugComponent::GuiCacheDebugComponent()
        : CgsDev::DebugComponent(CgsDev::DebugComponent::GuiConstructorTag{})
    {
        mInputQueue.MarkUnconstructed();
    }

    // Actual enclosing module Destruct82507720..A4 inlines this derived body.
    void GuiCacheDebugComponent::Destruct()
    {
        if (!mpGuiModule)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpGuiModule != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiGuiCacheDebugComponent.cpp", 80);
            CgsDev::Assert::EndAssert();
        }
        mpGuiModule = 0;
        mInputQueue.Destruct();
        // The original following base call is folded to empty8284CB38.
    }

    // ARTIST 0x824ED358; vtable 0x820CE850 slot 3.
    const char* GuiCacheDebugComponent::GetName() const
    {
        return "Gui Cache";
    }

    // The owner table points to the shared ICF body at 0x824ED348, which returns "Gui".
    const char* GuiCacheDebugComponent::GetPath() const
    {
        return "Gui";
    }

    // ARTIST 0x824F8060. Preserve the menu registration order and the five original choices.
    void GuiCacheDebugComponent::OnActivate()
    {
        RegisterFunction(&GuiCacheDebugComponent::ShowGuiCacheStatusCallback, this,
                         "Show Gui Cache Status");
        RegisterVariable(&miDisplayMode, "Display Mode");

        maStringList[0].miValue = 0;
        maStringList[0].mpcName = "All";
        maStringList[1].miValue = 1;
        maStringList[1].mpcName = "Loading+Unloading";
        maStringList[2].miValue = 2;
        maStringList[2].mpcName = "Loaded+Unloaded";
        maStringList[3].miValue = 3;
        maStringList[3].mpcName = "Loaded+Loading";
        maStringList[4].miValue = 4;
        maStringList[4].mpcName = "Unloaded+Unloading";
        SetRange(&miDisplayMode, 0, 4);
        SetOptions(&miDisplayMode, maStringList);

        RegisterFunction(&GuiCacheDebugComponent::ShowAptComponentGuiCacheStatusCallback, this,
                         "Show Apt Component Gui Cache Status");
    }

    // ARTIST 0x824ED368; the original callback asserts, then toggles this+0x14.
    void GuiCacheDebugComponent::ShowGuiCacheStatusCallback(void* lpData)
    {
        CGS_ASSERT(lpData != 0, "ShowGuiCacheStatusCallback: Invalid data in the callback");
        GuiCacheDebugComponent* lpThis = static_cast<GuiCacheDebugComponent*>(lpData);
        lpThis->mbShowGuiCacheStatus = !lpThis->mbShowGuiCacheStatus;
    }

    // ARTIST 0x824ED410; the original callback asserts, then toggles this+0x15.
    void GuiCacheDebugComponent::ShowAptComponentGuiCacheStatusCallback(void* lpData)
    {
        CGS_ASSERT(lpData != 0,
                   "ShowAptComponentGuiCacheStatusCallback: Invalid data in the callback");
        GuiCacheDebugComponent* lpThis = static_cast<GuiCacheDebugComponent*>(lpData);
        lpThis->mbShowAptComponentGuiCacheStatus = !lpThis->mbShowAptComponentGuiCacheStatus;
    }

    // ARTIST 0x82505800. Draw the two independent status panels in their original order.
    void GuiCacheDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (mbShowGuiCacheStatus)
            ShowGuiCacheStatus(lpRender);
        if (mbShowAptComponentGuiCacheStatus)
            ShowAptComponentGuiCacheStatus(lpRender);
    }

    // ARTIST 0x824FD6B0. The console snapshots 16,460 bytes beginning at the embedded
    // StateLoadingHelper and reads it through AppendResourceInfo. That method is read-only, so the
    // host uses the same named owner directly and does not copy guest-sized object bytes.
    void GuiCacheDebugComponent::ShowGuiCacheStatus(CgsDev::Debug2DImmediateRender* lpRender)
    {
        char lacBuffer[1024];
        CgsDev::StrStream lStrStream(lacBuffer, sizeof(lacBuffer));
        // ARTIST snapshots the complete helper before the row walk. Its fields are plain inline storage (resource records contain raw pointers, and queues/arrays own no handles), so a typed host copy preserves the snapshot without copying guest-sized bytes.
        StateLoadingHelper lStateLoadingHelperSnapshot =
            mpGuiModule->mGuiCache.mStateLoadingHelper;

        // ARTIST calls the scalar DrawBox wrapper with RGBA 0xC8000000.
        lpRender->DrawBox(50.0f, 30.0f, 1250.0f, 2695.5999f,
                          static_cast<CgsDev::RGBA>(0xC8000000u));

        u32 luLoadedCount = 0;
        u32 luDisplayedCount = 0;
        f32 lfX = 50.0f;
        f32 lfY = 41.2f;
        for (u32 luIndex = 0; luIndex < 0xED; ++luIndex)
        {
            lStrStream.Reset();
            lStateLoadingHelperSnapshot.AppendResourceInfo(luIndex, lStrStream);
            const char* lpcEntry = lStrStream.GetBuffer();

            bool lbDisplayEntry = false;
            CgsDev::RGBA luColour = static_cast<CgsDev::RGBA>(0xFF00B9F5u);
            if (lpcEntry[0] == 'L')
            {
                if (lpcEntry[4] == 'E')
                {
                    ++luLoadedCount;
                    lbDisplayEntry = miDisplayMode == 0 || miDisplayMode == 2 || miDisplayMode == 3;
                }
                else
                {
                    luColour = static_cast<CgsDev::RGBA>(0xFF0000F5u);
                    lbDisplayEntry = miDisplayMode < 2 || miDisplayMode == 3;
                }
            }
            else if (lpcEntry[6] == 'E')
            {
                luColour = static_cast<CgsDev::RGBA>(0xFFF500B9u);
                lbDisplayEntry = miDisplayMode == 0 || miDisplayMode == 2;
            }
            else
            {
                luColour = static_cast<CgsDev::RGBA>(0xFF00F500u);
                lbDisplayEntry = miDisplayMode == 0 || miDisplayMode == 1 || miDisplayMode == 4;
            }

            if (lbDisplayEntry)
            {
                MaybeDrawText(lpRender, lpcEntry, lfX, lfY, 12.0f, luColour, false);
                ++luDisplayedCount;
                lfX += 300.0f;
                if ((luDisplayedCount & 3) == 0)
                {
                    lfY += 11.2f;
                    lfX = 50.0f;
                }
            }
        }

        lStrStream.Reset();
        lStrStream << "Loadcount: " << luLoadedCount;
        MaybeDrawText(lpRender, lStrStream.GetBuffer(), 50.0f, 15.0f, 12.0f,
                      static_cast<CgsDev::RGBA>(0xFF00B9F5u), false);
        lStrStream.Reset();
    }

    // ARTIST 0x824F8148. Render all expected components for the Screen, Hud and Overlay flows.
    void GuiCacheDebugComponent::ShowAptComponentGuiCacheStatus(
        CgsDev::Debug2DImmediateRender* lpRender)
    {
        char lacBuffer[1024];
        CgsDev::StrStream lStrStream(lacBuffer, sizeof(lacBuffer));
        // ARTIST snapshots the complete helper before the row walk. Its fields are plain inline storage (resource records contain raw pointers, and queues/arrays own no handles), so a typed host copy preserves the snapshot without copying guest-sized bytes.
        StateLoadingHelper lStateLoadingHelperSnapshot =
            mpGuiModule->mGuiCache.mStateLoadingHelper;
        CgsGui::AptCommunicator* lpAptCommunicator =
            mpGuiModule->mpGuiView->GetAptAux()->GetAptCommunicatorInstance();

        // ARTIST calls the scalar DrawBox wrapper with RGBA 0xC8000000.
        lpRender->DrawBox(50.0f, 50.0f, 1250.0f, 629.0f,
                          static_cast<CgsDev::RGBA>(0xC8000000u));

        u32 luLoadedCount = 0;
        u32 luGridRow = 0;
        for (s32 liFlow = E_GUIFLOW_FIRST; liFlow < E_GUIFLOW_COUNT; ++liFlow)
        {
            const char* lpcFlowName = "Unknown";
            switch (liFlow)
            {
                case E_GUIFLOW_SCREEN:  lpcFlowName = "Screen";  break;
                case E_GUIFLOW_HUD:     lpcFlowName = "Hud";     break;
                case E_GUIFLOW_OVERLAY: lpcFlowName = "Overlay"; break;
                default: break;
            }

            lStrStream.Reset();
            lStrStream << lpcFlowName;
            MaybeDrawText(lpRender, lStrStream.GetBuffer(), 50.0f,
                          static_cast<f32>(luGridRow) * 3.0f + 50.0f, 12.0f,
                          static_cast<CgsDev::RGBA>(0xFFF5B9F5u), false);
            lStrStream.Reset();

            const StateLoadingHelper::ComponentsToWatch& lrComponents =
                lStateLoadingHelperSnapshot.maComponentsToWatch[liFlow];
            s32 liComponentY = static_cast<s32>(luGridRow) + 2;
            u32 luComponentIndex = 0;
            f32 lfComponentY = 0.0f;
            while (true)
            {
                ++liComponentY;
                const u32 luColumn = luComponentIndex & 3;
                if (luColumn == 0)
                    lfComponentY = static_cast<f32>(liComponentY) * 3.0f + 50.0f;

                const f32 lfX = static_cast<f32>(luColumn) * 300.0f + 50.0f;
                if (luComponentIndex >= lrComponents.muNumberOfComponentsToWatch)
                {
                    // The original do/while advances this cursor once for its terminating test.
                    ++luComponentIndex;
                    break;
                }

                const bool lbLoaded = lrComponents.mabComponentsLoaded[luComponentIndex];
                const u32 luComponentNameHash = lrComponents.mauComponentsToWatchIds[luComponentIndex];
                const CgsDev::RGBA luColour = lbLoaded
                    ? static_cast<CgsDev::RGBA>(0xFF00B9F5u)
                    : static_cast<CgsDev::RGBA>(0xFFF500B9u);
                if (lbLoaded)
                    ++luLoadedCount;

                lStrStream << (lbLoaded ? "L" : "U") << "\t-\t" << luComponentNameHash << "\t-\t"
                           << lpAptCommunicator->GetComponentNameForHash(luComponentNameHash);
                MaybeDrawText(lpRender, lStrStream.GetBuffer(), lfX, lfComponentY, 12.0f,
                              luColour, false);
                lStrStream.Reset();

                ++luComponentIndex;
                if (luComponentIndex >= 0xC0)
                    break;
            }

            luGridRow = static_cast<u32>(liComponentY + 2);
            const s32 liEnumIndex = liFlow + 1;
            if (liEnumIndex > E_GUIFLOW_COUNT)
                CGS_ASSERT(liEnumIndex <= E_GUIFLOW_COUNT, "leEnumIndex <= E_GUIFLOW_COUNT");
        }

        lStrStream.Reset();
        lStrStream << "Loadcount: " << luLoadedCount << "    L = LOADED    U = UNLOADED";
        MaybeDrawText(lpRender, lStrStream.GetBuffer(), 50.0f, 25.0f, 12.0f,
                      static_cast<CgsDev::RGBA>(0xFF00B9F5u), false);
        lStrStream.Reset();
    }

    void GuiCacheDebugComponent::Construct(GuiModule* lpGuiModule)
    {
        CGS_ASSERT(lpGuiModule != 0, "Invalid Gui module passed to GuiCacheDebugComponent::Construct");

        // Original base Construct resolves to empty8284CB38; no base data reset.
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
