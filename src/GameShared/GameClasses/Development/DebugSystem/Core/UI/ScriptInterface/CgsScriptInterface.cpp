#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/ScriptInterface/CgsScriptInterface.h"

#include <string.h>   // _stricmp, strstr, strchr, strncpy, memcpy, strlen
#include <stdlib.h>   // atoi

#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                       // CgsCore::SPrintf / StrCpy
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"              // DebugManager (singleton, component API)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"            // DebugComponent::IsActive
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"                // DebugUI accessors (managers, log window, SafeStringCat)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"     // Variable
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"  // VariableManager::FindVariableFromPath
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"     // Function
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h"  // FunctionManager::FindFunctionFromPath
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"      // LogWindow::Print
#include "rw/core/debug/host.h"                                                              // rw::core::debug::host file I/O

// CgsDev::DebugUI::ScriptInterface - script command runner.
//
// NOTE ON SCOPE (updated wave D): the former blockers are RESOLVED -
//   * KA_COMMAND_TABLE (X360 rodata @ 0x820DC198, 12 {handler, name, help} triples + null
//     terminator) was recovered with headless IDA from the local ARTIST database; it is declared
//     in the header and defined alongside ExecuteScriptCommand / ScriptCommand_Help.
//   * MenuManager::FindMenu (DWARF :85, X360 0x82819DB0) and MenuManager::OpenWindowFromPath
//     (DWARF :115) are declared in CgsMenuManager.h.
//   * The window-list walk / component-list walk / menu-item-pool walk the asm inlines are
//     modelled by name through DebugUI::mWindowList, DebugManager::mComponentList and
//     VariableManager::mMenuItemPool via ScriptInterface friend declarations (dwarfdump never
//     surfaces DW_TAG_friend; the access is attested by the inlined asm in ScriptCommand_Window
//     0x828318A8 / SaveState 0x82832660).
// The remaining functions land here: ExecuteScriptCommand, ScriptCommand_Help, ScriptCommand_Save
// (the 0x82833110 tail-call thunk the exporter skipped), ScriptCommand_Window, SaveState.

namespace CgsDev
{
    namespace DebugUI
    {

        // -- the built-in command table ----------------------------------------------------------
        //
        // X360 ARTIST rodata @ 0x820DC198: 12 {handler, name, help} triples (stride 12 = three
        // 4-byte pointers) followed by an all-zero terminator @0x820DC228. Both ExecuteScriptCommand
        // and ScriptCommand_Help walk it until the handler slot is null. Names starting with '*'
        // are hidden from the HELP listing.
        ScriptInterface::ScriptCommand ScriptInterface::KA_COMMAND_TABLE[] =
        {
            { ScriptCommand_Window,            "*WINDOW",   "<PATH> <X> <Y> [PINNED] - Open window position" },
            { ScriptCommand_IncrementVariable, "INCREMENT", "<PATH/VAR> - Increment a variable" },
            { ScriptCommand_DecrementVariable, "DECREMENT", "<PATH/VAR> - Decrement a variable" },
            { ScriptCommand_Help,              "HELP",      " - Displays help" },
            { ScriptCommand_SetVariable,       "SET",       "<PATH/VAR> <VAL> - Set a variable" },
            { ScriptCommand_Function,          "CALL",      "<PATH/FUNC> - Call a function" },
            { ScriptCommand_Save,              "SAVE",      "<FILE> - Save UI state" },
            { ScriptCommand_Exec,              "EXEC",      "<FILE> - Execute the given script file" },
            { ScriptCommand_ActivateComponent, "COMPONENT", "<PATH/NAME> - Activate the debug component" },
            { ScriptCommand_Print,             "PRINT",     "<MESSAGE> - Display a message on the console" },
            { ScriptCommand_Alias,             "ALIAS",     "<ALIAS> <PATH/NAME> - Create a shortcut alias" },
            { ScriptCommand_Bind,              "BIND",      "<Fx> <CMD> - Bind F1-F12 to a command" },
            { nullptr,                         nullptr,     nullptr },
        };

        // -- alias / variable / function lookup -------------------------------------------------

        ScriptInterface::Alias* ScriptInterface::FindAlias(const char* lpcAlias)
        {
            for (s32 liCommandIndex = 0; liCommandIndex < KI_MAX_ALIASES; ++liCommandIndex)
            {
                if (_stricmp(maAliasTable[liCommandIndex].macAlias, lpcAlias) == 0)
                    return &maAliasTable[liCommandIndex];
            }
            return nullptr;
        }

        Variable* ScriptInterface::FindVariable(const char* lpcPath, bool lbCheckAliases)
        {
            if (lbCheckAliases)
            {
                Alias* lpAlias = FindAlias(lpcPath);
                if (lpAlias && lpAlias->mpVariable)
                    return lpAlias->mpVariable;
            }
            return GetUI().GetVariableManager().FindVariableFromPath(lpcPath);
        }

        Function* ScriptInterface::FindFunction(const char* lpcPath, bool lbCheckAliases)
        {
            if (lbCheckAliases)
            {
                Alias* lpAlias = FindAlias(lpcPath);
                if (lpAlias && lpAlias->mpFunction)
                    return lpAlias->mpFunction;
            }
            return GetUI().GetFunctionManager().FindFunctionFromPath(lpcPath);
        }

        bool ScriptInterface::CreateAlias(const char* lpcAlias, const char* lpcValue)
        {
            if (strlen(lpcAlias) >= KU_MAX_ALIAS_LENGTH)
            {
                char lacBuffer[100];
                CgsCore::SPrintf(lacBuffer, 100, "Alias too long. Max %d", KU_MAX_ALIAS_LENGTH);
                OutputMessage(lacBuffer);
                return false;
            }

            if (strlen(lpcAlias) == 0 || strstr(lpcAlias, "\t \n\r"))
            {
                OutputMessage("Invalid alias name - no spaces");
                return false;
            }

            // Reuse an existing alias of the same name, else claim the first free slot.
            Alias* lpAlias = FindAlias(lpcAlias);
            if (lpAlias)
            {
                lpAlias->mpVariable = nullptr;
                lpAlias->mpFunction = nullptr;
                lpAlias->macAlias[0] = 0;
            }
            else
            {
                s32 liCommandIndex = 0;
                while (maAliasTable[liCommandIndex].macAlias[0])
                {
                    if (++liCommandIndex >= KI_MAX_ALIASES)
                    {
                        OutputMessage("Too many aliases");
                        return false;
                    }
                }
                lpAlias = &maAliasTable[liCommandIndex];
            }

            CgsCore::StrCpy(lpAlias->macAlias, KU_MAX_ALIAS_LENGTH, lpcAlias);

            // Resolve the alias target: a function first, otherwise a variable.
            lpAlias->mpFunction = FindFunction(lpcValue, false);
            if (!lpAlias->mpFunction)
                lpAlias->mpVariable = FindVariable(lpcValue, false);

            if (!lpAlias->mpVariable && !lpAlias->mpFunction)
            {
                OutputMessage("Unknown alias value");
                lpAlias->macAlias[0] = 0;
                return false;
            }
            return true;
        }

        // -- command execution ------------------------------------------------------------------

        void ScriptInterface::Execute(const char* lpcCommandString)
        {
            char lacBuffer[256];
            if (lpcCommandString)
            {
                strncpy(lacBuffer, lpcCommandString, 255);
                lacBuffer[255] = 0;
            }
            else
            {
                lacBuffer[0] = 0;
            }

            // Normalise carriage returns to spaces.
            for (char* lpcCleanup = lacBuffer; *lpcCleanup; ++lpcCleanup)
            {
                if (*lpcCleanup == '\r')
                    *lpcCleanup = ' ';
            }

            // Skip leading whitespace.
            char* lpcRead = lacBuffer;
            while (*lpcRead == ' ' || *lpcRead == '\t')
                ++lpcRead;

            // Ignore comments and empty lines.
            if (*lpcRead == ';' || !*lpcRead)
                return;

            char* lpcScriptCommand = lpcRead;

            // Consume the command word (up to the first space / tab / end).
            while (*lpcRead != ' ')
            {
                if (*lpcRead == '\t' || !*lpcRead)
                    break;
                ++lpcRead;
            }

            const char* lpacScriptParameters[KI_MAX_PARAMETERS];
            s32 liParameterCount = 0;

            if (*lpcRead)
            {
                // Terminate the command word and skip the whitespace after it.
                *lpcRead = 0;
                ++lpcRead;
                while (*lpcRead == ' ' || *lpcRead == '\t')
                    ++lpcRead;

                while (*lpcRead && liParameterCount < KI_MAX_PARAMETERS)
                {
                    bool lbQuote = (*lpcRead == '"');
                    if (lbQuote)
                        ++lpcRead;

                    lpacScriptParameters[liParameterCount++] = lpcRead;

                    if (lbQuote)
                    {
                        while (*lpcRead && *lpcRead != '"')
                            ++lpcRead;
                        if (*lpcRead)
                            *lpcRead++ = 0;
                    }

                    while (*lpcRead && *lpcRead != ' ' && *lpcRead != '\t' && *lpcRead != ',')
                        ++lpcRead;

                    if (*lpcRead)
                    {
                        *lpcRead = 0;
                        ++lpcRead;
                    }

                    while (*lpcRead == ' ' || *lpcRead == '\t')
                        ++lpcRead;
                }
            }

            // Pad the unused parameter slots with the shared empty string.
            for (s32 liCount = liParameterCount; liCount < KI_MAX_PARAMETERS; ++liCount)
                lpacScriptParameters[liCount] = "";

            ExecuteScriptCommand(lpcScriptCommand, lpacScriptParameters, liParameterCount);
        }

        void ScriptInterface::ExecuteScript(const char* lpcFileName)
        {
            char lacBuffer[512];
            char lacLine[256];

            int liFile = rw::core::debug::host::Open(lpcFileName, 1);
            if (liFile < 0)
            {
                CgsCore::SPrintf(lacBuffer, 512, "Cannot open %s", lpcFileName);
                OutputMessage(lacBuffer);
                return;
            }

            s32 liSize = rw::core::debug::host::Read(liFile, lacBuffer, 511);
            lacBuffer[liSize] = 0;

            while (liSize)
            {
                // Locate the end of the current line.
                char* lpcEnd = lacBuffer;
                if (lacBuffer[0] != '\n')
                {
                    while (*lpcEnd && *lpcEnd != '\n')
                        ++lpcEnd;
                }

                if (*lpcEnd == '\n')
                {
                    *lpcEnd++ = 0;
                }
                else if (*lpcEnd && lpcEnd != lacBuffer)
                {
                    // Buffer filled without a line feed - the line is too long to process.
                    OutputMessage("line too long or missing LF at end of file. aborting!");
                    break;
                }
                // else: a complete final line with no trailing line feed - process it below.

                strncpy(lacLine, lacBuffer, 255);
                lacLine[255] = 0;
                Execute(lacLine);

                // Shift the unconsumed remainder to the front of the buffer.
                s32 liRemaining = (s32)strlen(lpcEnd);
                memcpy(lacBuffer, lpcEnd, liRemaining);
                lacBuffer[liRemaining] = 0;

                // Top the buffer back up.
                s32 liRead = rw::core::debug::host::Read(liFile, &lacBuffer[liRemaining], 511 - liRemaining);
                liSize = liRead + liRemaining;
                lacBuffer[liSize] = 0;
            }

            rw::core::debug::host::Close(liFile);
        }

        void ScriptInterface::ExecuteScriptCommand(const char* lpcCommand, const char** lppcParameters, s32 liParameterCount)
        {
            // Built-in command first: a case-insensitive match on the command word.
            for (s32 liCommandIndex = 0; KA_COMMAND_TABLE[liCommandIndex].mpFunction; ++liCommandIndex)
            {
                if (_stricmp(lpcCommand, KA_COMMAND_TABLE[liCommandIndex].mpcName) == 0)
                {
                    KA_COMMAND_TABLE[liCommandIndex].mpFunction(this, lppcParameters, liParameterCount);
                    return;
                }
            }

            // Otherwise a user alias, then a debug component name.
            if (!ExecuteScriptAlias(lpcCommand, lppcParameters, liParameterCount))
            {
                if (!ExecuteScriptComponent(lpcCommand, lppcParameters, liParameterCount))
                    OutputMessage("Unknown command");
            }
        }

        bool ScriptInterface::ExecuteScriptAlias(const char* lpcCommand, const char** lppcParameters, s32 liParameterCount)
        {
            Alias* lpAlias = FindAlias(lpcCommand);
            if (!lpAlias)
                return false;

            Function* lpFunction = lpAlias->mpFunction;
            if (lpFunction)
            {
                Function::DebugCallbackFunction lpfCallback = lpFunction->GetFunction();
                if (lpfCallback)
                    lpfCallback(lpFunction->GetParameter());
                return true;
            }

            Variable* lpVariable = lpAlias->mpVariable;
            if (!lpVariable)
                return false;

            if (liParameterCount < 1)
            {
                char lacBuffer[100];
                lpVariable->GetValueAsString(lacBuffer, 100);
                OutputMessage(lacBuffer);
            }
            else
            {
                lpVariable->SetValueFromString(*lppcParameters);
            }
            return true;
        }

        bool ScriptInterface::ExecuteScriptComponent(const char* lpcCommand, const char** lppcParameters, s32 liParameterCount)
        {
            (void)lppcParameters;
            (void)liParameterCount;

            DebugManager& lManager = GetDebugManager();
            DebugComponent* lpComponent = lManager.FindComponentByName(lpcCommand);
            if (!lpComponent || lpComponent->IsActive())
                return false;

            lManager.ActivateComponent(lpComponent);
            return true;
        }

        // -- built-in command handlers ----------------------------------------------------------

        void ScriptInterface::ScriptCommand_ActivateComponent(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            (void)liParameterCount;

            DebugManager* lpManager = DebugManager::GetInstance();
            DebugComponent* lpComponent = lpManager->FindComponentByName(*lppcParameters);
            if (!lpComponent)
            {
                lpThis->OutputMessage("Component not found");
                return;
            }
            if (!lpComponent->IsActive())
                lpManager->ActivateComponent(lpComponent);
        }

        void ScriptInterface::ScriptCommand_Alias(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            if (liParameterCount < 1)
            {
                lpThis->OutputMessage("expecting at least 1 parameter");
                return;
            }

            if (liParameterCount != 1)
            {
                lpThis->CreateAlias(lppcParameters[0], lppcParameters[1]);
                return;
            }

            Alias* lpAlias = lpThis->FindAlias(lppcParameters[0]);
            if (!lpAlias)
            {
                lpThis->OutputMessage("Alias not found");
                return;
            }

            char lacValue[256];
            lpThis->GetAliasString(lpAlias, lacValue, 256);

            char lacMessage[264];
            CgsCore::SPrintf(lacMessage, 256, "%s: %s", lpAlias->macAlias, lacValue);
            lpThis->OutputMessage(lacMessage);
        }

        void ScriptInterface::ScriptCommand_Bind(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            if (liParameterCount < 1)
            {
                lpThis->OutputMessage("expecting at least 1 parameter");
                return;
            }

            const char* lpcFunctionKey = lppcParameters[0];
            char lcFirst = lpcFunctionKey[0];
            if (lcFirst == 'F' || lcFirst == 'f')
            {
                s32 liBindingIndex = atoi(lpcFunctionKey + 1);
                if ((u32)(liBindingIndex - 1) <= (u32)(KI_MAX_BINDINGS - 1))
                {
                    char* lpcBinding = lpThis->maBindingTable[liBindingIndex - 1].macBinding;

                    if (liParameterCount == 1)
                    {
                        char lacMessage[256];
                        CgsCore::SPrintf(lacMessage, 256, "F%s: %s", lpcFunctionKey + 1, lpcBinding);
                        lpThis->OutputMessage(lacMessage);
                    }
                    else
                    {
                        lpcBinding[0] = 0;
                        for (s32 liParameter = 1; liParameter < liParameterCount; ++liParameter)
                        {
                            const char* lpcValue = lppcParameters[liParameter];
                            if (strchr(lpcValue, ' '))
                            {
                                lpThis->GetUI().SafeStringCat(lpcBinding, "\"", KU_MAX_BINDING_LENGTH);
                                lpThis->GetUI().SafeStringCat(lpcBinding, lpcValue, KU_MAX_BINDING_LENGTH);
                                lpThis->GetUI().SafeStringCat(lpcBinding, "\" ", KU_MAX_BINDING_LENGTH);
                            }
                            else
                            {
                                lpThis->GetUI().SafeStringCat(lpcBinding, lpcValue, KU_MAX_BINDING_LENGTH);
                                lpThis->GetUI().SafeStringCat(lpcBinding, " ", KU_MAX_BINDING_LENGTH);
                            }
                        }
                    }
                    return;
                }
            }

            lpThis->OutputMessage("invalid function key: F1-F12");
        }

        void ScriptInterface::ScriptCommand_DecrementVariable(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            if (liParameterCount != 1)
            {
                lpThis->OutputMessage("Expected 1 parameters");
                return;
            }

            const char* lpcPath = *lppcParameters;
            Alias* lpAlias = lpThis->FindAlias(lpcPath);
            if (lpAlias && lpAlias->mpVariable)
            {
                lpAlias->mpVariable->Decrement();
                return;
            }

            Variable* lpVariable = lpThis->GetUI().GetVariableManager().FindVariableFromPath(lpcPath);
            if (lpVariable)
                lpVariable->Decrement();
            else
                lpThis->OutputMessage("Variable not found");
        }

        void ScriptInterface::ScriptCommand_Exec(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            (void)liParameterCount;
            lpThis->ExecuteScript(*lppcParameters);
        }

        void ScriptInterface::ScriptCommand_Function(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            (void)liParameterCount;

            const char* lpcPath = *lppcParameters;

            Function* lpFunction = nullptr;
            Alias* lpAlias = lpThis->FindAlias(lpcPath);
            if (lpAlias)
                lpFunction = lpAlias->mpFunction;

            if (!lpFunction)
            {
                lpFunction = lpThis->GetUI().GetFunctionManager().FindFunctionFromPath(lpcPath);
                if (!lpFunction)
                {
                    lpThis->OutputMessage("Function not found");
                    return;
                }
            }

            Function::DebugCallbackFunction lpfCallback = lpFunction->GetFunction();
            if (lpfCallback)
                lpfCallback(lpFunction->GetParameter());
        }

        void ScriptInterface::ScriptCommand_Help(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            (void)lppcParameters;
            (void)liParameterCount;

            lpThis->OutputMessage("Available Commands:");

            char lacMessage[256];
            for (s32 liCommandIndex = 0; KA_COMMAND_TABLE[liCommandIndex].mpFunction; ++liCommandIndex)
            {
                // A leading '*' marks a command that is hidden from the help listing.
                if (KA_COMMAND_TABLE[liCommandIndex].mpcName[0] != '*')
                {
                    CgsCore::SPrintf(lacMessage, 256, "  %s %s", KA_COMMAND_TABLE[liCommandIndex].mpcName, KA_COMMAND_TABLE[liCommandIndex].mpcHelp);
                    lpThis->OutputMessage(lacMessage);
                }
            }
        }

        void ScriptInterface::ScriptCommand_IncrementVariable(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            if (liParameterCount != 1)
            {
                lpThis->OutputMessage("Expected 1 parameters");
                return;
            }

            const char* lpcPath = *lppcParameters;
            Alias* lpAlias = lpThis->FindAlias(lpcPath);
            if (lpAlias && lpAlias->mpVariable)
            {
                lpAlias->mpVariable->Increment();
                return;
            }

            Variable* lpVariable = lpThis->GetUI().GetVariableManager().FindVariableFromPath(lpcPath);
            if (lpVariable)
                lpVariable->Increment();
            else
                lpThis->OutputMessage("Variable not found");
        }

        void ScriptInterface::ScriptCommand_Print(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            (void)liParameterCount;
            lpThis->GetUI().GetLogWindow().Print(*lppcParameters);
        }

        void ScriptInterface::ScriptCommand_Save(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            (void)liParameterCount;
            lpThis->SaveState(*lppcParameters);
        }

        void ScriptInterface::ScriptCommand_SetVariable(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount)
        {
            if (liParameterCount != 2)
            {
                lpThis->OutputMessage("Expected 2 parameters");
                return;
            }

            Alias* lpAlias = lpThis->FindAlias(*lppcParameters);
            Variable* lpVariable;
            if (lpAlias && lpAlias->mpVariable)
                lpVariable = lpAlias->mpVariable;
            else
                lpVariable = lpThis->GetUI().GetVariableManager().FindVariableFromPath(*lppcParameters);

            if (!lpVariable)
            {
                char lacMessage[256];
                CgsCore::SPrintf(lacMessage, 256, "Variable %s not found", *lppcParameters);
                lpThis->OutputMessage(lacMessage);
                return;
            }

            if (lpVariable->IsReadOnly())
            {
                char lacMessage[256];
                CgsCore::SPrintf(lacMessage, 256, "Variable %s is read only", *lppcParameters);
                lpThis->OutputMessage(lacMessage);
            }
            else
            {
                lpVariable->SetValueFromString(lppcParameters[1]);
            }
        }

    }  // namespace DebugUI
}  // namespace CgsDev
