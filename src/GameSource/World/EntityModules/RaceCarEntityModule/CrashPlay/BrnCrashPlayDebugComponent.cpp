#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{
    void CrashPlayDebugComponent::Construct(CrashPlayManager* lpCrashPlayManager)
    {
        mpCrashPlayManager = lpCrashPlayManager;
        mbDisplayCrashBreaker = false;
    }

    void CrashPlayDebugComponent::Destruct()
    {
        CGS_ASSERT(mpCrashPlayManager != 0, "mpCrashPlayManager != NULL");
        mpCrashPlayManager = 0;
        CgsDev::DebugComponent::Destruct();
    }

    void CrashPlayDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender*)
    {
        CGS_ASSERT(mpCrashPlayManager != 0, "mpCrashPlayManager != NULL");
    }

    void CrashPlayDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*)
    {
        CGS_ASSERT(mpCrashPlayManager != 0, "mpCrashPlayManager != NULL");
    }

    void CrashPlayDebugComponent::Update()
    {
        CGS_ASSERT(mpCrashPlayManager != 0, "mpCrashPlayManager != NULL");
    }

    const char* CrashPlayDebugComponent::GetName() const
    {
        return "CrashPlay";
    }

    const char* CrashPlayDebugComponent::GetPath() const
    {
        return "Gameplay";
    }

    void CrashPlayDebugComponent::OnActivate()
    {
        RegisterVariable(&mbDisplayCrashBreaker, "Display crashbreaker");
        RegisterVariable(&mpCrashPlayManager->mfAftertouchPower, "", "Aftertouch power");
        RegisterVariable(&mpCrashPlayManager->mbInfiniteAftertouch, "", "Infinite Aftertouch");
        RegisterVariable(&mpCrashPlayManager->mbInfiniteBoost, "", "Infinite Boost");
    }
}
