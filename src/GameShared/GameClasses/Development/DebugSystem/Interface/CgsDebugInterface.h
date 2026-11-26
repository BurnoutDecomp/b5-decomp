#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h"        // DebugUI::Function::DebugCallbackFunction (RegisterFunction)

// CgsDev::DebugInterface - the lightweight handle the engine passes around to talk to the debug
// system: it wraps a DebugManager pointer (plus an "is automatic" flag marking stack-scoped uses)
// and exposes the complete register/console/render mirror of the DebugComponent API. Recovered from
// the DecFIGS declaration shape and the thin ARTIST forwarding bodies.

namespace CgsDev
{
    namespace DebugUI
    {
        struct DebugUI;
        struct LogWindowStrStream;
        struct MenuItemVariable;
    }
    class DebugRender;

    struct DebugInterface
    {
        // The AUTOMATIC (stack-scoped) handle: set the is-automatic flag, then acquire the singleton
        // (ThreadSafeAquire asserts it and enters the debug section); the destructor releases it.
        // Header-inline with the body statements in this order (LegacySrc CgsDebugInterface.h:136;
        // ARTIST's out-of-line copy 0x821F1F20 stores the flag, stb @0x821F1F40, before the inlined
        // ThreadSafeAquire, and DebugComponent::Register 0x828331E4 inlines it the same way).
        DebugInterface()
        {
            mbIsAutomaticClass = true;
            mpDebugManager = DebugManager::ThreadSafeAquire();
        }

        // LegacySrc CgsDebugInterface.h:143: the flag, then the borrowed manager.
        explicit DebugInterface(DebugManager* lpDebugManager)
        {
            mbIsAutomaticClass = false;
            mpDebugManager = lpDebugManager;
        }

        ~DebugInterface()
        {
            if (mbIsAutomaticClass)
                DebugManager::ThreadSafeRelease(mpDebugManager);
        }

        // Header-inline: assert the manager pointer is set (CgsDebugInterface.h:163, ARTIST copy
        // 0x823A61B0), then return it.
        DebugManager&     GetDebugManager()
        {
            CGS_ASSERT_AT(mpDebugManager, "mpDebugManager",
                "..\\..\\..\\GameShared\\GameClasses\\Development/DebugSystem/Interface/CgsDebugInterface.h", 163);
            return *mpDebugManager;
        }
        DebugUI::DebugUI& GetUI();

        DebugRender& GetRender();
        DebugRender& Get2dRender();

        DebugUI::LogWindowStrStream& GetConsole();
        void ConsolePrint(const char* lpcMessage);
        void ConsolePrintf(const char* lpcFormat, ...);
        void ShowConsole(bool lbShow);

        // Show/hide the on-screen debug console. FLAG: these DebugInterface members
        // are inferred from the ICEWrapper call site (no second in-tree reference),
        // homed in CgsDebugInterface.cpp (DebugInterface::EnableConsole /
        // DisableConsole); the in-game ICE editor toggles the console through these
        // (BrnDirector::ICEWrapper::EditorOn/EditorOff). DECLARATION-ONLY: the bodies
        // belong to the DebugInterface TU and the /c gate does not link.
        void EnableConsole();
        void DisableConsole();
        bool IsConsoleEnabled();
        void ShowErrorMessage(const char* lpcMessage);

        void RegisterVariable(bool* lpbVariable, const char* lpcPath, const char* lpcName);
        void RegisterVariable(s32* lpiVariable, const char* lpcPath, const char* lpcName);
        void RegisterVariable(u32* lpuVariable, const char* lpcPath, const char* lpcName);
        void RegisterVariable(f32* lpfVariable, const char* lpcPath, const char* lpcName);
        void SetRange(f32* lpfVariable, f32 lfMin, f32 lfMax);
        void SetRange(s32* lpiVariable, s32 liMin, s32 liMax);
        void SetRange(u32* lpuVariable, u32 luMin, u32 luMax);
        void SetStep(f32* lpfVariable, f32 lfStep);
        void SetStep(s32* lpiVariable, s32 liStep);
        void SetStep(u32* lpuVariable, u32 luStep);
        void SetReadOnly(void* lpValue, bool lbReadOnly);
        void SetSaveEnabled(void* lpValue, bool lbSaveEnabled);
        void SetVisible(void* lpValue, bool lbVisible);
        void SetCustomMenuItem(void* lpValue, DebugUI::MenuItemVariable* lpCustomMenuItem);
        void SetOptions(s32* lpiVariable, const DebugUI::StringList* lpOptions);
        void SetChangeCallback(void* lpValue,
                               DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback,
                               void* lpUserData);
        void SetSelectCallback(void* lpValue,
                               DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback,
                               void* lpUserData);
        void SetVariableName(void* lpValue, const char* lpcName);
        void UnregisterVariable(void* lpValue);

        void RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback,
                              void* lpUserData, const char* lpcPath, const char* lpcName);
        void UnregisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback,
                                void* lpUserData);
        void SetFunctionName(DebugUI::Function::DebugCallbackFunction lpfCallback,
                             void* lpUserData, const char* lpcName);

        void ExecuteScript(const char* lpcFileName);

    private:
        DebugManager* mpDebugManager;
        bool          mbIsAutomaticClass;
    };
}
