#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

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
