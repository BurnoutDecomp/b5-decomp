#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunctionManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsDev
{
    // ARTIST 82828E28; DecFIGS B24990 declares void(const char*).
    void DebugManager::DeactivateComponent(const char* lpcName)
    {
        DebugComponent* lpComponent = FindComponentByName(lpcName);
        if (lpComponent)
            lpComponent->mbActive = false;
    }

    // ARTIST 8282E308; DecFIGS B3E0D4 declares void(const char*).
    void DebugManager::CallFunction(const char* lpcPath)
    {
        if (!mpUI)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpUI", "..\\..\\..\\GameShared\\GameClasses\\Development/DebugSystem/Core/CgsDebugManager.cpp", 1201);
            CgsDev::Assert::EndAssert();
        }
        mpUI->GetFunctionManager().CallFunction(lpcPath);
    }
}
