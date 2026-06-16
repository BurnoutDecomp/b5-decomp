#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // DebugComponent (mbActive/OnRegister/DebugUISectionCallback)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"      // GetUI().GetVariableManager()/GetFunctionManager()
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"        // Variant
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT

// CgsDev::DebugManager - the in-game debug systems owner.
//
// Boot/loading path: the per-frame tick is a no-op so the game-module update spine links and runs;
// the full Update (driving the UI) is the manager-construction follow-on.
//
// Registration/lifecycle (what DebugComponent::Register drives): ThreadSafeAquire/Release bracket
// the singleton behind the debug critical section (X360 0x821F1E50 / ThreadSafeRelease);
// RegisterComponent[Simple] thread the component onto the list and surface it in the menu - Simple
// as an mbActive bool toggle (X360 0x8282E1E0), the full form as a section callback row
// (X360 0x82832008). Both clear mbActive and fire the component's OnRegister.
//
// The debug critical section is the same single-threaded-boot no-op as the assert mutex; the real
// lock is deferred to the threading wiring. mpUI is wired by DebugManager::Construct (the
// manager-construction follow-on); GetUI hands it back.

namespace CgsDev
{
    DebugManager* DebugManager::mpInstance = nullptr;

    DebugManager::DebugManager()
        : mpUI(nullptr)
    {
        mComponentList.Clear();
    }

    DebugManager::~DebugManager() {}

    void DebugManager::Update(f32) {}

    DebugManager* DebugManager::ThreadSafeAquire()
    {
        CGS_ASSERT(mpInstance, "mpInstance");
        // X360 enters the debug-manager critical section here (no-op on the single-threaded boot).
        return mpInstance;
    }

    void DebugManager::ThreadSafeRelease(DebugManager* lpDebugManager)
    {
        // X360 leaves the debug-manager critical section here (no-op on the single-threaded boot).
        CGS_ASSERT(lpDebugManager == mpInstance, "lpDebugManager == mpInstance");
    }

    DebugUI::DebugUI& DebugManager::GetUI()
    {
        return *mpUI;
    }

    bool DebugManager::IsComponentRegistered(DebugComponent* lpComponent)
    {
        return mComponentList.IsAdded(lpComponent);
    }

    // X360 0x82832008. Register a component as a full menu section: a function row that, when
    // selected, fires DebugComponent::DebugUISectionCallback for it.
    void DebugManager::RegisterComponent(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName)
    {
        CGS_ASSERT(!IsComponentRegistered(lpComponent), "!IsComponentRegistered(lpDebugComponent)");

        mComponentList.Add(lpComponent);
        lpComponent->mbActive = false;

        if (lpcName)
            GetUI().GetFunctionManager().RegisterFunction(&DebugComponent::DebugUISectionCallback, lpComponent, lpcPath, lpcName);

        lpComponent->OnRegister();
    }

    // X360 0x8282E1E0. Register a "simple" component: a single bool row wired to its mbActive flag.
    void DebugManager::RegisterComponentSimple(DebugComponent* lpComponent, const char* lpcPath, const char* lpcName)
    {
        CGS_ASSERT(!IsComponentRegistered(lpComponent), "!IsComponentRegistered(lpDebugComponent)");
        CGS_ASSERT(lpcName, "lpcName");

        if (!lpcPath)
            lpcPath = "";

        mComponentList.Add(lpComponent);
        lpComponent->mbActive = false;

        GetUI().GetVariableManager().RegisterVariable(DebugUI::Variant(&lpComponent->mbActive), lpcPath, lpcName);

        lpComponent->OnRegister();
    }
}
