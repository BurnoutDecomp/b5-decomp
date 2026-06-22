#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"  // DebugManagerConstructParameters
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (Get2DRenderer guard)

// CgsDev::DebugUI::DebugUI - the manager accessors every DebugComponent / manager reaches through
// GetUI(). The X360 reads them as fixed sub-objects of the UI singleton (MenuManager@+228,
// VariableManager@+272, FunctionManager@+332); here they are the named by-value members. The rest of
// the DebugUI surface (window stack, render, console, ...) is the UI follow-on.

namespace CgsDev
{
    namespace DebugUI
    {
        MenuManager&     DebugUI::GetMenuManager()     { return mMenuManager; }
        VariableManager& DebugUI::GetVariableManager() { return mVariableManager; }
        FunctionManager& DebugUI::GetFunctionManager() { return mFunctionManager; }
        const Metrics&   DebugUI::GetMetrics() const   { return mMetrics; }

        // X360 CgsDebugUI.cpp:101 (bounded). Construct the three managers (each sizes its pools from
        // the construct parameters), reset the window stack + cascade/visibility scalars, and clear the
        // 2D renderer pointer (DebugManager::ConstructRenderer wires it via Set2DRenderer). The X360
        // also lays out the palette/metrics defaults, builds the console / error-window / script /
        // command sub-windows, and registers a built-in UI-visibility variable; those ride on the
        // deferred heavy members and are the UI follow-on (none is needed for the perfmon HUD to draw).
        void DebugUI::Construct(const DebugManagerConstructParameters* lpParameters)
        {
            mMetrics = Metrics::DEFAULT;   // X360 memcpy's the default metrics into the UI here

            mMenuManager.Construct(lpParameters);
            mVariableManager.Construct(lpParameters);
            mFunctionManager.Construct(lpParameters);

            mWindowList.Clear();
            mpActiveWindow = nullptr;
            mfCascadeX     = 0.0f;
            mfCascadeY     = 0.0f;
            mbVisible      = false;
            mbRunAutoExec  = false;
            mp2dRender     = nullptr;
        }

        // X360 CgsDebugUI.cpp:345 is empty (the debug allocator owns the managers' pool backing).
        void DebugUI::Destruct() {}

        // X360 0x828221A8 (CgsDebugUI.h:246). Asserts the 2D immediate renderer has been wired
        // (DebugManager::ConstructRenderer calls Set2DRenderer at boot) then returns it. Every
        // window/menu/log ComputeSize-style measure path reaches the font metrics through here.
        Debug2DImmediateRender* const DebugUI::Get2DRenderer() const
        {
            CGS_ASSERT(mp2dRender != NULL, "mp2dRender != NULL");
            return mp2dRender;
        }

        void DebugUI::Set2DRenderer(Debug2DImmediateRender* lpRender)  { mp2dRender = lpRender; }

        // Bounded string helpers (MakeFullPath/menu-path building use these). Always null-terminate
        // within the buffer.
        void DebugUI::SafeStringCopy(char* lpcBuffer, const char* lpcSource, s32 liBufferLen)
        {
            if (liBufferLen <= 0)
                return;
            s32 liIndex = 0;
            for (; liIndex < liBufferLen - 1 && lpcSource[liIndex]; ++liIndex)
                lpcBuffer[liIndex] = lpcSource[liIndex];
            lpcBuffer[liIndex] = '\0';
        }

        void DebugUI::SafeStringCat(char* lpcBuffer, const char* lpcSource, s32 liBufferLen)
        {
            if (liBufferLen <= 0)
                return;
            s32 liEnd = 0;
            while (liEnd < liBufferLen - 1 && lpcBuffer[liEnd])
                ++liEnd;
            s32 liSource = 0;
            for (; liEnd < liBufferLen - 1 && lpcSource[liSource]; ++liEnd, ++liSource)
                lpcBuffer[liEnd] = lpcSource[liSource];
            lpcBuffer[liEnd] = '\0';
        }
    }
}
