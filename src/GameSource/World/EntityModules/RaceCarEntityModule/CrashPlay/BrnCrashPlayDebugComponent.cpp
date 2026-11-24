#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

static const char KAC_ARTIST_ASSERT_FILE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
    "../World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.cpp";

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
        CGS_ASSERT(mpCrashPlayManager != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpCrashPlayManager != NULL", KAC_ARTIST_ASSERT_FILE, 82 /* ARTIST 0x822C3490 */));
    }

    void CrashPlayDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*)
    {
        CGS_ASSERT(mpCrashPlayManager != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpCrashPlayManager != NULL", KAC_ARTIST_ASSERT_FILE, 99 /* ARTIST 0x822A8188 */));
    }

    void CrashPlayDebugComponent::Update()
    {
        CGS_ASSERT(mpCrashPlayManager != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpCrashPlayManager != NULL", KAC_ARTIST_ASSERT_FILE, 111 /* ARTIST 0x822A81D0 */));
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
