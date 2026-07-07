// ============================================================================
// GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.cpp
//
// BrnGui::LicenseComponent -- the driver-licence screen component (see the header).
// Reconstructed from BURNOUT_X360_ARTIST.XEX; behaviour (stores, asserts, callee
// order and signedness) taken from the X360 assembly of each function.
// ============================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"

#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache::EnsureResource(s)...
#include "GameSource/GameState/Progression/BrnProfile.h"                  // BrnProgression::Profile::GetLicenceIssuedDate
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface::GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager::FormatDateString
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"    // CgsSystem::DateAndTime
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT + CgsDev::Assert::Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (SetRank composed message)

namespace BrnGui
{
    // 0x824B31E8
    void LicenseComponent::SetCachePointer(GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "NULL != lpGuiCache");
        mpGuiCache = lpGuiCache;
    }

    // 0x824B3248
    void LicenseComponent::SetProfilePointer(BrnProgression::Profile* lpProfile)
    {
        CGS_ASSERT(lpProfile != 0, "NULL != lpProfile");

        // Only rebuild the date field when the profile actually changes.
        if (mpProfile != lpProfile)
        {
            mpProfile = lpProfile;

            const CgsSystem::DateAndTime lLicenceDate = mpProfile->GetLicenceIssuedDate();
            CgsLanguage::LanguageManager* lpLanguageManager = mpStateInterface->GetLanguageManager();

            const s32 liYear  = lLicenceDate.GetYear();
            const s32 liMonth = lLicenceDate.GetMonth();
            const s32 liDay   = lLicenceDate.GetDay();

            char lacDateString[64];
            lpLanguageManager->FormatDateString(lacDateString, liDay, liMonth, liYear, 64);
            mDateTextField.SetText(lacDateString);
        }
    }

    // 0x8241A4B8
    void LicenseComponent::SetRank(s32 liRank)
    {
        // The X360 range check is SIGNED (blt <0 / ble <=5): valid ranks are 0..5.
        if (liRank < 0 || liRank > 5)
        {
            // The X360 builds the message on the assert message-stream:
            //   "Invalid rank supplied (" << rank << ")".
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream loStream(lacMessage, sizeof(lacMessage));
            loStream << "Invalid rank supplied (" << liRank << ")";

            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lacMessage,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gui\\flow\\screen\\components\\BrnLicenseComponent.h",
                343);
            CgsDev::Assert::EndAssert();
        }

        SetState("idle");
        miRank = liRank;
        mbRankTransitionActive = false;
    }

    // 0x824B3300
    bool LicenseComponent::EnsureResourcesAreLoaded()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");

        CGS_ASSERT(meCurrentLicenseState != E_LICENSE_FIRST_RESOURCE_UNLOADED,
                   "Do not know which resource we need to load at this point! In state E_LICENSE_FIRST_RESOURCE_UNLOADED \n");

        // The "first resource" state group (state in [8,15)) uses tuple slot 1; every
        // other state uses slot 0.
        const s32 liIndex = (meCurrentLicenseState >= E_LICENSE_FIRST_RESOURCE_UNLOADED
                             && meCurrentLicenseState < 15) ? 1 : 0;

        return mpGuiCache->EnsureResourceIsLoaded(maResources[liIndex]);
    }

    // 0x824B33F8
    bool LicenseComponent::EnsureResourcesAreUnloaded()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");

        CGS_ASSERT(meCurrentLicenseState == E_LICENSE_RESOURCES_UNLOADED,
                   "E_LICENSE_RESOURCES_UNLOADED == meCurrentLicenseState");

        return mpGuiCache->EnsureResourcesAreUnloaded(maResources, muNumResources);
    }
}
