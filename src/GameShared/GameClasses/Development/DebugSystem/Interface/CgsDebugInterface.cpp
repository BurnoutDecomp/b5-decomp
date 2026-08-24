#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"                   // GetUI().GetFunctionManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h" // FunctionManager::RegisterFunction

// CgsDev::DebugInterface - the two functions the component-registration path drives:
// the automatic-acquire constructor and the manager accessor.

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

    // Faithful port of X360 GetDebugManager @ 0x823A61B0:
    //   if (!*this) { Begin/Fire/EndAssert("mpDebugManager", CgsDebugInterface.h:163); }
    //   return *this;
    DebugManager& DebugInterface::GetDebugManager()
    {
        CGS_ASSERT(mpDebugManager, "mpDebugManager");
        return *mpDebugManager;
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
}

// ⭐ 2026-08-17 (boot audit F-P1-14). The forwarder behind DebugInterface::RegisterFunction.
// Argument order is pinned by BrnGameModule::Construct's two call sites @0x823CAF28/48
// (r4 = callback, r5 = context, r6 = path, r7 = name) and matches, argument for argument,
// the FunctionManager::RegisterFunction already bodied at X360 0x8282E7E0.
//
// Defined here rather than inline in the header because the body needs CgsDebugUI.h, whose
// GetNextWindow() collides with the Windows.h macro of the same name -- pulling it through
// the interface header breaks every TU that includes Windows.h first (BrnMain.cpp did).
void CgsDev::DebugInterface::RegisterFunction(DebugUI::Function::DebugCallbackFunction lpfCallback,
                                              void* lpUserData, const char* lpcPath, const char* lpcName)
{
    mpDebugManager->GetUI().GetFunctionManager().RegisterFunction(lpfCallback, lpUserData, lpcPath, lpcName);
}
