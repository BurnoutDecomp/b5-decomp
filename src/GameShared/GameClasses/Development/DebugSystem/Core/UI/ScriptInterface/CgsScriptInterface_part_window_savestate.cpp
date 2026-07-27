#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/ScriptInterface/CgsScriptInterface.h"

#include <string.h>   // _stricmp, strlen
#include <stdlib.h>   // atoi

#include "GameShared/GameClasses/Core/CgsAssert.h"                                              // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                         // CgsCore::SPrintf / StrCat
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"                // DebugManager (singleton, component list)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"              // DebugComponent::IsActive / GetComponentPath
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"                  // DebugUI (window list, managers)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"                   // Window
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.h"                // Menu::GetPath
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuManager.h"         // MenuManager::FindMenu / OpenWindowFromPath
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"       // Variable
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"// VariableManager (mMenuItemPool)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsMenuItemVariable.h"// MenuItemVariable::GetVariable
#include "rw/core/debug/host.h"                                                                 // rw::core::debug::host file I/O

// CgsDev::DebugUI::ScriptInterface - the *WINDOW command handler and the whole-state serialiser.
// Split out of CgsScriptInterface.cpp (which holds the other bodies) so the two halves could be
// landed independently; both are members of the same class and share its header.
//
//   ScriptCommand_Window (X360 ARTIST 0x828318A8)
//   SaveState            (X360 ARTIST 0x82832660)

namespace CgsDev
{
    namespace DebugUI
    {

        // *WINDOW <PATH> <X> <Y> [PINNED] - move an already-open debug window to a screen position,
        // opening it from its menu path first if it is not on the window stack yet.
        void ScriptInterface::ScriptCommand_Window(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            char lacBuffer[256];

            // The parameter-count complaint is routed through the singleton's own script interface
            // (the X360 loads DebugManager::mpInstance here rather than using lpThis).
            if (liParameterCount < 3)
            {
                DebugManager::GetInstance()->GetUI().GetScriptInterface().OutputMessage("invalid parameters");
                return;
            }

            const char* lpcPath = lppcParameters[0];
            f32 lfX = (f32)atoi(lppcParameters[1]);
            f32 lfY = (f32)atoi(lppcParameters[2]);

            // Every trailing parameter must be the PINNED keyword.
            bool lbPinned = false;
            for (s32 liParameter = 3; liParameter < liParameterCount; ++liParameter)
            {
                if (_stricmp(lppcParameters[liParameter], "PINNED") != 0)
                {
                    CgsCore::SPrintf(lacBuffer, 256, "%s - unknown parameter", lppcParameters[liParameter]);
                    lpThis->OutputMessage(lacBuffer);
                    return;
                }
                lbPinned = true;
            }

            DebugUI& lUI = DebugManager::GetInstance()->GetUI();

            // Already open: just reposition it (no pin handling on this path).
            for (Window* lpOpenWindow = lUI.mWindowList.GetFirst();
                 lpOpenWindow;
                 lpOpenWindow = lUI.mWindowList.GetNext(lpOpenWindow))
            {
                lpOpenWindow->GetMenuPath(lacBuffer, 256);
                if (_stricmp(lacBuffer, lpcPath) == 0)
                {
                    lpOpenWindow->SetPosition(lfX, lfY);
                    lpOpenWindow->ClampToScreen();
                    return;
                }
            }

            Window* lpWindow = lUI.GetMenuManager().OpenWindowFromPath(lpcPath);
            if (!lpWindow)
            {
                DebugManager::GetInstance()->GetUI().GetScriptInterface().OutputMessage("window not found:");
                DebugManager::GetInstance()->GetUI().GetScriptInterface().OutputMessage(lpcPath);
                return;
            }

            lpWindow->SetPosition(lfX, lfY);
            lpWindow->ClampToScreen();
            if (lbPinned && !lpWindow->IsPinned())
                lpWindow->TogglePin();
        }

        // Serialise the whole debug-UI state back out as a script the EXEC command can replay:
        // active components, every saveable variable, the aliases, the F-key bindings and the open
        // windows. Every record is formatted into lacBuffer and written without its terminator -
        // except the trailing blank line, which writes the NUL too.
        void ScriptInterface::SaveState(const char* lpcFileName)
        {
            char lacBuffer[512];
            char lacString[256];
            char lacPath[256];

            int liFile = rw::core::debug::host::Open(lpcFileName, 50);
            if (liFile < 0)
            {
                OutputMessage("Cannot open file");
                return;
            }

            CgsCore::SPrintf(lacBuffer, 512, ";\n; DebugUI state script. Generated from build %s\n;\n\n\n", __DATE__);
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));

            // -- the registered components that are currently active --
            CgsCore::SPrintf(lacBuffer, 512, "; active components\n");
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));

            DebugManager* lpDebugManager = DebugManager::GetInstance();
            for (DebugComponent* lpComponent = lpDebugManager->mComponentList.GetFirst();
                 lpComponent;
                 lpComponent = lpDebugManager->mComponentList.GetNext(lpComponent))
            {
                if (lpComponent->IsActive())
                {
                    lpComponent->GetComponentPath(lacString, 256);
                    CgsCore::SPrintf(lacBuffer, 512, "COMPONENT \"%s\"\n", lacString);
                    rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));
                }
            }

            // -- every registered variable that is neither read-only nor save-disabled --
            CgsCore::SPrintf(lacBuffer, 512, "\n; all variables\n");
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));

            DebugUI& lUI = DebugManager::GetInstance()->GetUI();
            VariableManager& lVariableManager = lUI.GetVariableManager();

            for (s32 liIndex = 0; liIndex < lVariableManager.mMenuItemPool.GetActiveCount(); ++liIndex)
            {
                MenuItemVariable* lpMenuItemVariable = lVariableManager.mMenuItemPool.GetActiveAt(liIndex);
                CGS_ASSERT(lpMenuItemVariable, "lpMenuItemVariable");

                Variable* lpVariable = lpMenuItemVariable->GetVariable();
                if (lpVariable->IsReadOnly() || !lpVariable->IsSaveEnabled())
                    continue;

                // The variable's full script path is its menu's path plus its own name.
                Menu* lpMenu = lUI.GetMenuManager().FindMenu(lpMenuItemVariable);
                if (lpMenu)
                    lpMenu->GetPath(lacPath, 256);
                else
                    lacPath[0] = 0;

                CgsCore::StrCat(lacPath, 256, "/");
                CgsCore::StrCat(lacPath, 256, lpVariable->GetName());

                lpVariable->GetValueAsString(lacString, 256);
                CgsCore::SPrintf(lacBuffer, 512, "SET \"%s\" \"%s\"\n", lacPath, lacString);
                rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));
            }

            // -- the alias table --
            CgsCore::SPrintf(lacBuffer, 512, "\n; all aliases\n");
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));

            for (s32 liAlias = 0; liAlias < KI_MAX_ALIASES; ++liAlias)
            {
                Alias* lpAlias = &maAliasTable[liAlias];
                if (lpAlias->macAlias[0])
                {
                    GetAliasString(lpAlias, lacString, 256);
                    if (strlen(lacString) != 0)
                    {
                        CgsCore::SPrintf(lacBuffer, 512, "ALIAS %s \"%s\"\n", lpAlias->macAlias, lacString);
                        rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));
                    }
                }
            }

            // -- the F1-F12 key bindings --
            CgsCore::SPrintf(lacBuffer, 512, "\n; all bindings\n");
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));

            for (s32 liBinding = 0; liBinding < KI_MAX_BINDINGS; ++liBinding)
            {
                if (maBindingTable[liBinding].macBinding[0])
                {
                    CgsCore::SPrintf(lacBuffer, 512, "BIND F%d %s\n", liBinding + 1, maBindingTable[liBinding].macBinding);
                    rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));
                }
            }

            // -- the open windows, as *WINDOW commands --
            CgsCore::SPrintf(lacBuffer, 512, "\n; all windows\n");
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));

            for (Window* lpWindow = lUI.mWindowList.GetFirst();
                 lpWindow;
                 lpWindow = lUI.mWindowList.GetNext(lpWindow))
            {
                lpWindow->GetMenuPath(lacString, 256);
                if (lacString[0])
                {
                    CgsCore::SPrintf(lacBuffer, 512, "*WINDOW \"%s\" %d %d", lacString,
                                     (s32)lpWindow->GetX(), (s32)lpWindow->GetY());
                    if (lpWindow->IsPinned())
                        CgsCore::StrCat(lacBuffer, 512, " PINNED");
                    CgsCore::StrCat(lacBuffer, 512, "\n");
                    rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer));
                }
            }

            // The trailing blank line is written WITH its terminator (the X360 passes strlen + 1).
            CgsCore::SPrintf(lacBuffer, 512, "\n\n");
            rw::core::debug::host::Write(liFile, lacBuffer, (u32)strlen(lacBuffer) + 1);

            rw::core::debug::host::Close(liFile);
        }

    }  // namespace DebugUI
}  // namespace CgsDev
