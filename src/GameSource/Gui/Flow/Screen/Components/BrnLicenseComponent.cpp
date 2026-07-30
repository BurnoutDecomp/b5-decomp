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
#if 0   // see the header note: declaring Construct here makes it an override of the virtual
        // CgsGui::GuiComponent::Construct, and this TU cannot be linked yet, so the vtable
        // hole breaks every TU that embeds a LicenseComponent by value. Recovered body kept
        // verbatim; enable it together with the header declaration when the TU is mounted.
    // 0x8241A610 -- Construct. The X360 body is:
    //     IconComponent::Construct(this, lpacName, lpStateInterface, NULL, lpacParentName);
    //     +1980 = 0.08f;  +168 = 0;  +1988 = 0.08f;  +172 = 0;
    //     +1952 = 0; +1953 = 0; +1984 = 0.0f; +1954 = 0;
    //     +1956 = -1; +1960 = -1; +1964 = 0;
    //     +1976 = 1; +1977 = 1; +1968 = 0; +1969 = 0; +1972 = 0; +1978 = 0;
    //     +148 = 0; +152 = 0; +156 = 0; +160 = 0; +164 = 0;
    //     <the six embedded TextFields Construct with "playerName" / "playerUpgrade" /
    //      "IssuedOnText_cpt" / "MonthText_cpt" / "DateText_cpt" / "YearText_cpt",
    //      each parented on macName>
    //     +1992 = 0;
    //
    // PARTIAL BY DESIGN: this header models the resource tuples, the cache/profile
    // pointers, miRank (+1956), mbRankTransitionActive (+1969) and meCurrentLicenseState
    // (+1972). Those seeds are reproduced exactly. The remaining stores belong to members
    // this header has not carved yet -- the two 0.08f dirt timers (+1980/+1988), the dirt
    // accumulator (+1984), the win counter (+1960 = -1), the presentation booleans
    // (+1952/+1953/+1954/+1964/+1968/+1976/+1977/+1978) and the gamerpic flag (+1992) --
    // plus the six embedded TextFields.
    // FLAG PC-platform leaf: those land with the licence-presentation slice
    // (BrnLicenseComponent.cpp's 15 remaining X360 functions); until then this state's
    // presentation is inert, which is why the seeds that DO exist are the ones that keep
    // EnsureResourcesAreLoaded / SetProfilePointer from reading uninitialised storage.
    void LicenseComponent::Construct(const char* lpacName,
                                     CgsGui::StateInterface* lpStateInterface,
                                     const char* lpacParentName)
    {
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        maResources[0].muId   = 0;                                    // +148
        maResources[0].meType = CgsGui::E_GUI_RESOURCETYPE_START;     // +152
        maResources[1].muId   = 0;                                    // +156
        maResources[1].meType = CgsGui::E_GUI_RESOURCETYPE_START;     // +160
        muNumResources        = 0;                                    // +164
        mpGuiCache            = 0;                                    // +168
        mpProfile             = 0;                                    // +172
        miRank                = -1;                                   // +1956
        mbRankTransitionActive = false;                               // +1969
        meCurrentLicenseState = static_cast<ELicenseState>(0);        // +1972
    }
#endif

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
