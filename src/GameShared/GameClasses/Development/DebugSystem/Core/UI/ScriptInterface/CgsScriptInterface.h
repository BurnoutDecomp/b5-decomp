#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Controller/CgsDebugController.h"  // DebugController::SpecialKey (Update)

// CgsDev::DebugUI::ScriptInterface - the debug console's script runner. It parses a command line
// (Execute) or a whole script file (ExecuteScript) into a command word + up to KI_MAX_PARAMETERS
// arguments, then dispatches to one of the built-in script commands (KA_COMMAND_TABLE), a user
// alias, or a debug component. It also owns the alias table (name -> Variable/Function) and the
// F1-F12 key-binding table, and can serialise the whole debug-UI state back out as a script
// (SaveState). Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/UI/ScriptInterface/CgsScriptInterface.h) + the X360 ARTIST asm.
//
// Derives from Internal::DebugInternal (empty base) so it reaches the debug singletons (GetUI(),
// GetDebugManager()) the same way as the other debug-UI subsystems. It is embedded by value in
// DebugUI (mScriptInterface); the alias table is the first data member, so on X360 `this` aliases
// &maAliasTable[0] (the FindAlias walk). We access every field by name (semantic parity), so the
// x64 pointer-width growth of Alias is transparent.

namespace CgsDev
{
    namespace DebugUI
    {
        struct Variable;
        struct Function;

        struct ScriptInterface : public Internal::DebugInternal
        {
            // -- limits (X360 CgsScriptInterface.h) --
            static const s32 KI_MAX_PARAMETERS   = 10;
            static const s32 KI_MAX_ALIASES      = 40;
            static const u32 KU_MAX_ALIAS_LENGTH = 10;
            static const s32 KI_MAX_BINDINGS     = 12;
            static const u32 KU_MAX_BINDING_LENGTH = 128;

            // One built-in script command: its handler + the command word + the help text.
            struct ScriptCommand
            {
                void (*mpFunction)(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
                const char* mpcName;
                const char* mpcHelp;
            };

            // One user alias: a short name mapping to a registered Variable or Function.
            struct Alias
            {
                char      macAlias[KU_MAX_ALIAS_LENGTH];
                Variable* mpVariable;
                Function* mpFunction;
            };

            // One F-key binding: the script text run when the key is pressed.
            struct Binding
            {
                char macBinding[KU_MAX_BINDING_LENGTH];
            };

            // -- public entry points --
            void Construct();
            // Fully qualified: CgsDev::DebugController is the input layer (distinct from the
            // DebugUI-scoped forward declaration CgsDebugUI.h uses for GetController()).
            void Update(CgsDev::DebugController::SpecialKey leSpecialKey);
            void Execute(const char* lpcCommandString);
            void ExecuteScript(const char* lpcFileName);
            void SaveState(const char* lpcFileName);

        protected:
            bool      CreateAlias(const char* lpcAlias, const char* lpcValue);
            void      GetAliasString(Alias* lpAlias, char* lpcBuffer, s32 liBufferLen);
            Alias*    FindAlias(const char* lpcAlias);
            Variable* FindVariable(const char* lpcPath, bool lbCheckAliases);
            Function* FindFunction(const char* lpcPath, bool lbCheckAliases);

            void ExecuteScriptCommand(const char* lpcCommand, const char** lppcParameters, s32 liParameterCount);
            bool ExecuteScriptAlias(const char* lpcCommand, const char** lppcParameters, s32 liParameterCount);
            bool ExecuteScriptComponent(const char* lpcCommand, const char** lppcParameters, s32 liParameterCount);

            // The built-in command handlers (registered by pointer in KA_COMMAND_TABLE).
            static void ScriptCommand_Help(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_SetVariable(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_IncrementVariable(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_DecrementVariable(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Function(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_ActivateComponent(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Save(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Exec(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Print(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Alias(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Window(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);
            static void ScriptCommand_Bind(ScriptInterface* lpThis, const char** lppcParameters, s32 liParameterCount);

            void OutputMessage(const char* lpcMessage);

            // The command dispatch table (DWARF CgsScriptInterface.h:85). 12 built-in commands +
            // a null-handler terminator, RECOVERED from the X360 ARTIST rodata at 0x820DC198
            // (headless-IDA dump, wave D - handler/name/help triples chased through the pointer
            // table; see scratchpad/waveD/ScriptInterface.spec.md in the workflow repo).
            static ScriptCommand KA_COMMAND_TABLE[];

            // -- data (the alias table is first; the binding table follows) --
            Alias   maAliasTable[KI_MAX_ALIASES];
            Binding maBindingTable[KI_MAX_BINDINGS];
        };
    }
}
