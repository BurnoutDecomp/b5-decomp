#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Console/CgsConsole.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/ScriptInterface/CgsScriptInterface.h"

#include <cstdarg>
#include <cstdio>

// CgsDev::DebugInterface is deliberately thin: every operation asserts/obtains the UI through
// GetUI(), then forwards to the owning manager. ARTIST emits these as the small wrappers recovered
// below; the overload shapes come from the DecFIGS header.

namespace CgsDev
{
    // Faithful port of X360 DebugInterface() @ 0x821F1F20:
    //   *(this + 4) = 1;                                  // mbIsAutomaticClass = true
    //   if (!mpInstance) { Begin/Fire/EndAssert("mpInstance", CgsDebugManager.h:343); }
    //   DebugCriticalSection::Enter(dword_83019264);      // enter the per-manager debug section
    //   *this = mpInstance;                               // mpDebugManager = the singleton
    //
    // DebugManager::ThreadSafeAquire() IS this assert-mpInstance + enter-section + return-singleton
    // step (X360 ThreadSafeAquire 0x821F1E50), so the acquiring ctor forwards to it; the matching
    // ~DebugInterface release (when mbIsAutomaticClass) leaves the section.
    DebugInterface::DebugInterface()
        : mpDebugManager(DebugManager::ThreadSafeAquire())
        , mbIsAutomaticClass(true)
    {
    }

    DebugInterface::~DebugInterface()
    {
        if (mbIsAutomaticClass)
            DebugManager::ThreadSafeRelease(mpDebugManager);
    }

    // Faithful port of X360 GetDebugManager @ 0x823A61B0:
    //   if (!*this) { Begin/Fire/EndAssert("mpDebugManager", CgsDebugInterface.h:163); }
    //   return *this;
    DebugManager& DebugInterface::GetDebugManager()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return *mpDebugManager;
    }

    DebugUI::DebugUI& DebugInterface::GetUI()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return mpDebugManager->GetUI();
    }

    DebugRender& DebugInterface::GetRender()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return mpDebugManager->mBufferedRenderer;
    }

    // Faithful port of X360 Get2dRender @0x82822750 (console home CgsDebugInterface.cpp:190):
    // assert the manager pointer, then return its buffered renderer by reference (the console's
    // `mpDebugManager + 0x14C`; DebugInterface is a friend of the manager). This is what
    // BrnDirector::DebugPrinter::ActualPrint @0x821F71D8 and Camera::Utils::Tweaker's on-screen
    // readout both draw through.
    DebugRender& DebugInterface::Get2dRender()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return mpDebugManager->mBufferedRenderer;
    }

    DebugUI::LogWindowStrStream& DebugInterface::GetConsole()
    {
        return GetUI().GetConsole().mLog;
    }

    void DebugInterface::ConsolePrint(const char* lpcMessage)
    {
        GetConsole() << lpcMessage;
    }

    void DebugInterface::ConsolePrintf(const char* lpcFormat, ...)
    {
        char lacBuffer[256];
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        const int liResult = std::vsnprintf(lacBuffer, sizeof(lacBuffer), lpcFormat, lArgs);
        va_end(lArgs);
        if (liResult < 0 || liResult >= static_cast<int>(sizeof(lacBuffer)))
            lacBuffer[sizeof(lacBuffer) - 1] = '\0';
        ConsolePrint(lacBuffer);
    }

    void DebugInterface::ShowConsole(bool lbShow)
    {
        DebugUI::Console& lrConsole = GetUI().GetConsole();
        if (lrConsole.IsVisible() != lbShow)
            lrConsole.ToggleShow();
    }

    void DebugInterface::EnableConsole()  { GetUI().GetConsole().Enable(); }
    void DebugInterface::DisableConsole() { GetUI().GetConsole().Disable(); }
    bool DebugInterface::IsConsoleEnabled() { return GetUI().GetConsole().IsEnabled(); }

    void DebugInterface::ShowErrorMessage(const char* lpcMessage)
    {
        GetUI().ShowErrorMessage(lpcMessage);
    }

    void DebugInterface::RegisterVariable(f32* lpfValue, const char* lpcPath, const char* lpcName)
    { GetUI().GetVariableManager().RegisterVariable(lpfValue, lpcPath, lpcName); }
    void DebugInterface::RegisterVariable(s32* lpiValue, const char* lpcPath, const char* lpcName)
    { GetUI().GetVariableManager().RegisterVariable(lpiValue, lpcPath, lpcName); }
    void DebugInterface::RegisterVariable(u32* lpuValue, const char* lpcPath, const char* lpcName)
    { GetUI().GetVariableManager().RegisterVariable(lpuValue, lpcPath, lpcName); }
    void DebugInterface::RegisterVariable(bool* lpbValue, const char* lpcPath, const char* lpcName)
    { GetUI().GetVariableManager().RegisterVariable(lpbValue, lpcPath, lpcName); }

    void DebugInterface::SetRange(f32* lpfValue, f32 lfMin, f32 lfMax)
    { GetUI().GetVariableManager().SetRange(lpfValue, lfMin, lfMax); }
    void DebugInterface::SetRange(s32* lpiValue, s32 liMin, s32 liMax)
    { GetUI().GetVariableManager().SetRange(lpiValue, liMin, liMax); }
    void DebugInterface::SetRange(u32* lpuValue, u32 luMin, u32 luMax)
    { GetUI().GetVariableManager().SetRange(lpuValue, luMin, luMax); }
    void DebugInterface::SetStep(f32* lpfValue, f32 lfStep)
    { GetUI().GetVariableManager().SetStep(lpfValue, lfStep); }
    void DebugInterface::SetStep(s32* lpiValue, s32 liStep)
    { GetUI().GetVariableManager().SetStep(lpiValue, liStep); }
    void DebugInterface::SetStep(u32* lpuValue, u32 luStep)
    { GetUI().GetVariableManager().SetStep(lpuValue, luStep); }

    void DebugInterface::SetReadOnly(void* lpValue, bool lbReadOnly)
    { GetUI().GetVariableManager().SetReadOnly(lpValue, lbReadOnly); }
    void DebugInterface::SetSaveEnabled(void* lpValue, bool lbSaveEnabled)
    { GetUI().GetVariableManager().SetSaveEnabled(lpValue, lbSaveEnabled); }
    void DebugInterface::SetVisible(void* lpValue, bool lbVisible)
    { GetUI().GetVariableManager().SetVisible(lpValue, lbVisible); }
    void DebugInterface::SetCustomMenuItem(void* lpValue, DebugUI::MenuItemVariable* lpCustomMenuItem)
    { GetUI().GetVariableManager().SetCustomMenuItem(lpValue, lpCustomMenuItem); }
    void DebugInterface::SetOptions(s32* lpiValue, const DebugUI::StringList* lpOptions)
    { GetUI().GetVariableManager().SetOptions(lpiValue, lpOptions); }
    void DebugInterface::SetChangeCallback(void* lpValue,
                                           DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback,
                                           void* lpUserData)
    { GetUI().GetVariableManager().SetChangeCallback(lpValue, lpfCallback, lpUserData); }
    void DebugInterface::SetSelectCallback(void* lpValue,
                                           DebugUI::Variant::UValue::VariableCallbackFunction lpfCallback,
                                           void* lpUserData)
    { GetUI().GetVariableManager().SetSelectCallback(lpValue, lpfCallback, lpUserData); }
    void DebugInterface::SetVariableName(void* lpValue, const char* lpcName)
    { GetUI().GetVariableManager().SetVariableName(lpValue, lpcName); }
    void DebugInterface::UnregisterVariable(void* lpValue)
    { GetUI().GetVariableManager().UnregisterVariable(lpValue); }

    void DebugInterface::RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback,
                                          void* lpUserData, const char* lpcPath, const char* lpcName)
    { GetUI().GetFunctionManager().RegisterFunction(lpfCallback, lpUserData, lpcPath, lpcName); }
    void DebugInterface::UnregisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback,
                                            void* lpUserData)
    { GetUI().GetFunctionManager().UnregisterFunction(lpfCallback, lpUserData); }
    void DebugInterface::SetFunctionName(DebugUI::Function::DebugCallbackFunction lpfCallback,
                                         void* lpUserData, const char* lpcName)
    { GetUI().GetFunctionManager().SetFunctionName(lpfCallback, lpUserData, lpcName); }

    void DebugInterface::ExecuteScript(const char* lpcFileName)
    {
        GetUI().GetScriptInterface().ExecuteScript(lpcFileName);
    }
}
