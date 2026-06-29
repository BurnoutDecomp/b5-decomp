#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"  // DebugManagerConstructParameters
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"     // Window (mWindowList element - Add/Remove/IsAdded)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (Get2DRenderer guard)

// Complete element types for the three DebugStaticPool<T>::Allocate instantiations below: the
// pool template (DebugStaticPool, in CgsDebugCollections.h, transitively included via CgsDebugUI.h)
// only reaches the elements by pointer, but the explicit instantiations need the full types.
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"  // MenuItemVariable
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuWindow.h"             // MenuWindow
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"          // VariableMetadata

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

        // Window-stack membership (the Console toggle/slide path drives these). AddWindow appends the
        // window to the stack; RemoveWindow unlinks it; IsWindowAdded queries membership. The full X360
        // AddWindow also resets the cascade position + focus bookkeeping (UpdateCascadePosition /
        // SetActiveWindow) - those ride on the DebugUI window-stack follow-on; the membership change is
        // the part the Console depends on.
        void DebugUI::AddWindow(Window* lpWindow)    { mWindowList.Add(lpWindow); }
        void DebugUI::RemoveWindow(Window* lpWindow) { mWindowList.Remove(lpWindow); }
        bool DebugUI::IsWindowAdded(Window* lpWindow){ return mWindowList.IsAdded(lpWindow); }

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

    // ---- DebugStaticPool<T>::Allocate instantiations -----------------------------------------
    // The three pool-allocate entry points the debug-UI managers drive (X360 ARTIST):
    //   * DebugStaticPool<MenuItemVariable>::Allocate @0x82827B10 - called by
    //       VariableManager::RegisterVariable when it pools the rendered menu-item row.
    //   * DebugStaticPool<MenuWindow>::Allocate       @0x82827700 - called by
    //       MenuManager::CreateMenuWindow when it pools a new menu window.
    //   * DebugStaticPool<VariableMetadata>::Allocate @0x82827D18 - called by
    //       VariableManager::SetMetadata when it pools an attribute record.
    // Each asm is the same shape: pop the last free slot (mFree, a LIFO DebugStaticArray at
    // this+8: i16 miCount@+2, T** mppItems@+4), null-guard it, push it onto the active list
    // (mActive.Add - the trailing `_::Add` call), and return it. That body is the single generic
    // DebugStaticPool<T>::Allocate in CgsDebugCollections.h; these are the explicit instantiations
    // for the three element types, NOT a redefinition of the generic. Explicit instantiation must
    // live in the template's home namespace (CgsDev::Internal), not in DebugUI.
    namespace Internal
    {
        template DebugUI::MenuItemVariable* DebugStaticPool<DebugUI::MenuItemVariable>::Allocate();
        template DebugUI::MenuWindow*       DebugStaticPool<DebugUI::MenuWindow>::Allocate();
        template DebugUI::VariableMetadata* DebugStaticPool<DebugUI::VariableMetadata>::Allocate();
    }
}
