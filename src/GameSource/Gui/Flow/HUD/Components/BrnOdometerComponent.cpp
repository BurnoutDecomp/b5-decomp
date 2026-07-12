#include "GameSource/Gui/Flow/HUD/Components/BrnOdometerComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT + CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                            // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                        // CgsDev::StrStream (streamed assert message)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"           // CgsGui::StateInterface::GetAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                               // CgsGui::GuiAccessPointers::GetGuiCache
#include "GameSource/Gui/BrnGuiCache.h"                                            // BrnGui::GuiCache (GetTime / GetProfile / GetDistanceDriven)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                    // BrnGui::GuiEventJunctionInfo
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                              // BrnGui::GuiEventDriveThruDiscovered
#include "GameSource/GameState/Progression/BrnProfile.h"                           // BrnProgression::Profile::GetGameModeType*
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"  // BrnGui::AttachToTextFieldComponent

// BrnGui::OdometerComponent -- the in-race odometer HUD readout, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This slice homes all seven ledger functions: Construct /
// Prepare / HandleJunctionChange / HandleDriveThruDiscovered / Update / TransIn /
// TransOutActiveText. The two localisation-format integers passed to TextFieldRef::
// SetLocalisedText are the raw CgsLanguage::LanguageManager::ParameterFormatType values
// the X360 uses (9 == id lookup, 11 == integer, 19 == the distance format) -- passed as
// raw integers to match the committed TextFieldRef mutator house style.

namespace BrnGui
{
    // The "hold" durations (seconds) for the two transient found-messages before the
    // readout returns to the mileage state. DWARF file-scope constants
    // (BrnOdometerComponent.h:141/142); the X360 folds both to the same rodata literal
    // (flt_8204C54C) so the compare in Update reads one shared constant per state.
    const f32 KF_SHOW_NEWEVENTFOUND_TIME    = 5.0f;
    const f32 KF_SHOW_NEWDRIVETHRUFOUND_TIME = 5.0f;

    // @0x82F27868 -- per-gamemode "plural" event-name localisation string-id table, indexed
    // by GuiEventJunctionInfo::meGameModeType. Entry 0 (E_MODE_OFFLINE_RACE) is X360-attested
    // "GAMEMODE_RACE_PLURAL"; the remaining entries are serialised label data not present in
    // this function export, so the table is declaration-only here (defined by this class's
    // data TU once every gamemode label is recovered). Not a DWARF class member -- it is the
    // .cpp's file-scope name table (mirrors the sibling JunctionInfoComponent's
    // gGameModeNameStringIds).
    extern const char* const gGameModeNamePluralStringIds[];

    // @0x82415088 -- adopt the state interface (base Construct: the h:113 lpStateInterface
    // tripwire, store the channel, invalidate the clip), construct the three animator children
    // and invalidate the three text fields under this state interface, then cache the GuiCache
    // and the player profile through the state interface's access pointers. The X360 inlines
    // every base/child Construct and each ref invalidation; liParentAptLayer is unused by the
    // body (the child animators adopt lacParentName, not the layer).
    void OdometerComponent::Construct(const char* lacName,
                                      CgsGui::StateInterface* lpStateInterface,
                                      const char* lacParentName,
                                      s32 liParentAptLayer)
    {
        (void)lacName;
        (void)liParentAptLayer;

        BrnFlaptComponent::Construct(lpStateInterface);   // asserts lpStateInterface, stores it, invalidates mAptRef

        mMileageAnimator.Construct("MileageAnim_cpt", lpStateInterface, lacParentName);
        mMileageTextField.SetInvalid();
        mEventsFoundAnimator.Construct("EventsFoundAnim_cpt", lpStateInterface, lacParentName);
        mEventsFoundTextField.SetInvalid();
        mDriveThrusFoundAnimator.Construct("DriveThrusAnim_cpt", lpStateInterface, lacParentName);
        mDriveThrusFoundTextField.SetInvalid();

        // The GuiCache and player profile the readout renders from (the X360 inlines the
        // GetAccessPointers/GetGuiCache asserts -- they live in those accessors' bodies).
        CgsGui::GuiAccessPointers* lpAccessPointers = lpStateInterface->GetAccessPointers();
        mpGuiCache = lpAccessPointers->GetGuiCache();
        mpProfile  = mpGuiCache->GetProfile();

        meOdometerState    = E_ODOMETERSTATE_NONE;
        mfEnteredStateTime = 0.0f;
    }

    // @0x82423F88 -- bind this readout's root apt clip out of lFile (inlined base Prepare:
    // resolve+bind mAptRef and reset its timeline), prepare the three animators, resolve the
    // three found-message text fields under this readout's own parent, and seed the state to
    // NONE with a zero entered-state time.
    void OdometerComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        BrnFlaptComponent::Prepare(lacName, lFile, 0);   // inlined on the X360

        mMileageAnimator.Prepare("MileageAnim_cpt", lFile, lacName);
        mEventsFoundAnimator.Prepare("EventsFoundAnim_cpt", lFile, lacName);
        mDriveThrusFoundAnimator.Prepare("DriveThrusAnim_cpt", lFile, lacName);

        BrnFlapt::TextFieldRef lTextField;
        mMileageTextField = *AttachToTextFieldComponent(
            &lTextField, "MileageText_Txt", "MileageText_cpt", lacName, lFile);
        mEventsFoundTextField = *AttachToTextFieldComponent(
            &lTextField, "EventsFoundText_Txt", "EventsFoundText_cpt", lacName, lFile);
        mDriveThrusFoundTextField = *AttachToTextFieldComponent(
            &lTextField, "DriveThrusFoundText_Txt", "DriveThrusFoundText_cpt", lacName, lFile);

        meOdometerState    = E_ODOMETERSTATE_NONE;
        mfEnteredStateTime = 0.0f;
    }

    // @0x8242BEB0 -- a newly-discovered junction/event of a game mode: format the player's
    // "X of Y" event tally for that mode into the events-found field, transition out whatever
    // text is currently showing, animate the events-found message in, and enter the
    // EVENTSFOUND state. Only fires for a newly-discovered event.
    void OdometerComponent::HandleJunctionChange(const GuiEventJunctionInfo* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");

        if (lpEvent->mbIsNewlyDiscovered)
        {
            CGS_ASSERT(mpProfile != 0, "mpProfile");

            const s32 liNumDiscovered = mpProfile->GetGameModeTypeDiscovered(lpEvent->meGameModeType);
            const s32 liNumTotal      = mpProfile->GetGameModeTypeAmount(lpEvent->meGameModeType);

            const s32 KI_STRING_LENGTH = 8;
            char lacDiscoveredString[KI_STRING_LENGTH];
            CgsCore::SnPrintf(lacDiscoveredString, KI_STRING_LENGTH - 1, "%d", liNumDiscovered);
            lacDiscoveredString[KI_STRING_LENGTH - 1] = 0;
            char lacTotalString[KI_STRING_LENGTH];
            CgsCore::SnPrintf(lacTotalString, KI_STRING_LENGTH - 1, "%d", liNumTotal);
            lacTotalString[KI_STRING_LENGTH - 1] = 0;

            mEventsFoundTextField.SetLocalisedText(
                "HUDMESSAGE_X_OF_Y_Z_FOUND", 9, 3,
                lacDiscoveredString, 11,
                lacTotalString, 11,
                gGameModeNamePluralStringIds[lpEvent->meGameModeType], 9);

            TransOutActiveText();
            mEventsFoundAnimator.Run("transin");
            meOdometerState    = E_ODOMETERSTATE_EVENTSFOUND;
            mfEnteredStateTime = mpGuiCache->GetTime();
        }
    }

    // @0x8242C000 -- a newly-discovered drive-thru of a given type: pick the type's "plural"
    // localised name, then either flash "all of type found" (once the discovered count reaches
    // the total) or the "X of Y <type> found" tally; transition out the current text, animate
    // the drive-thrus-found message in, and enter the DRIVETHRUFOUND state.
    void OdometerComponent::HandleDriveThruDiscovered(const GuiEventDriveThruDiscovered* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");

        const s32 liNumDiscovered = lpEvent->miNumDiscovered;
        const s32 liNumTotal      = lpEvent->miNumTotal;

        const char* lpDriveThruStringID = 0;
        switch (lpEvent->meDriveThruType)
        {
        case 0:
            lpDriveThruStringID = "DRIVETHRU_JUNK_YARD_PLURAL";
            break;
        case 1:
            lpDriveThruStringID = "DRIVETHRU_GAS_STATION_PLURAL";
            break;
        case 2:
            lpDriveThruStringID = "DRIVETHRU_BODY_SHOP_PLURAL";
            break;
        case 3:
            lpDriveThruStringID = "DRIVETHRU_PAINT_SHOP_PLURAL";
            break;
        case 4:
            lpDriveThruStringID = "DRIVETHRU_CAR_PARK_PLURAL";
            break;
        default:
            CGS_ASSERT(false, "Unknown drive thru type");
            break;
        }

        if (liNumDiscovered >= liNumTotal)
        {
            mDriveThrusFoundTextField.SetLocalisedText(
                "ALL_DRIVE_THRUS_OF_TYPE_FOUND", 9, 1,
                lpDriveThruStringID, 9);
        }
        else
        {
            const s32 KI_STRING_LENGTH = 8;
            char lacDiscoveredString[KI_STRING_LENGTH];
            CgsCore::SnPrintf(lacDiscoveredString, KI_STRING_LENGTH - 1, "%d", liNumDiscovered);
            lacDiscoveredString[KI_STRING_LENGTH - 1] = 0;
            char lacTotalString[KI_STRING_LENGTH];
            CgsCore::SnPrintf(lacTotalString, KI_STRING_LENGTH - 1, "%d", liNumTotal);
            lacTotalString[KI_STRING_LENGTH - 1] = 0;

            mDriveThrusFoundTextField.SetLocalisedText(
                "HUDMESSAGE_X_OF_Y_Z_FOUND", 9, 3,
                lacDiscoveredString, 11,
                lacTotalString, 11,
                lpDriveThruStringID, 9);
        }

        TransOutActiveText();
        mDriveThrusFoundAnimator.Run("transin");
        meOdometerState    = E_ODOMETERSTATE_DRIVETHRUFOUND;
        mfEnteredStateTime = mpGuiCache->GetTime();
    }

    // @0x82424160 -- per-frame tick. In MILEAGE state, refresh the offline-distance readout;
    // in either found-message state, once the hold time elapses transition the message out,
    // animate the mileage readout back in, and return to MILEAGE. An unhandled state trips a
    // streamed assert.
    void OdometerComponent::Update()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
        CGS_ASSERT(mpProfile != 0, "mpProfile");

        switch (meOdometerState)
        {
        case E_ODOMETERSTATE_NONE:
            break;

        case E_ODOMETERSTATE_MILEAGE:
            mMileageTextField.SetLocalisedText(
                "STAT_LABEL_DIST_OFFLINE", 9, mpGuiCache->GetDistanceDriven(), 19);
            break;

        case E_ODOMETERSTATE_EVENTSFOUND:
            if (mpGuiCache->GetTime() > mfEnteredStateTime + KF_SHOW_NEWEVENTFOUND_TIME)
            {
                mEventsFoundAnimator.Run("transout");
                mMileageAnimator.Run("transin");
                meOdometerState    = E_ODOMETERSTATE_MILEAGE;
                mfEnteredStateTime = mpGuiCache->GetTime();
            }
            break;

        case E_ODOMETERSTATE_DRIVETHRUFOUND:
            if (mpGuiCache->GetTime() > mfEnteredStateTime + KF_SHOW_NEWDRIVETHRUFOUND_TIME)
            {
                mDriveThrusFoundAnimator.Run("transout");
                mMileageAnimator.Run("transin");
                meOdometerState    = E_ODOMETERSTATE_MILEAGE;
                mfEnteredStateTime = mpGuiCache->GetTime();
            }
            break;

        default:
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled EOdometerState ";
                lStrStream << (s32)meOdometerState;
                lStrStream << " in OdometerComponent::Update().\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    lacMessage,
                    "..\\..\\..\\GameSource\\Gui/Flow/HUD/Components/BrnOdometerComponent.cpp",
                    324);
                CgsDev::Assert::EndAssert();
            }
            break;
        }
    }

    // @0x82424388 -- kick off the mileage readout: assert we are idle, animate the mileage
    // message in, enter MILEAGE state and stamp the entered-state time.
    void OdometerComponent::TransIn()
    {
        CGS_ASSERT(meOdometerState == E_ODOMETERSTATE_NONE, "meOdometerState == E_ODOMETERSTATE_NONE");
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        mMileageAnimator.Run("transin");
        meOdometerState    = E_ODOMETERSTATE_MILEAGE;
        mfEnteredStateTime = mpGuiCache->GetTime();
    }

    // @0x82424468 -- transition out whichever message is currently showing (the animator for
    // the active state plays its "transout"), then reset to NONE and stamp the entered-state
    // time. An unhandled state trips a streamed assert.
    void OdometerComponent::TransOutActiveText()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        switch (meOdometerState)
        {
        case E_ODOMETERSTATE_NONE:
            break;

        case E_ODOMETERSTATE_MILEAGE:
            mMileageAnimator.Run("transout");
            break;

        case E_ODOMETERSTATE_EVENTSFOUND:
            mEventsFoundAnimator.Run("transout");
            break;

        case E_ODOMETERSTATE_DRIVETHRUFOUND:
            mDriveThrusFoundAnimator.Run("transout");
            break;

        default:
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled EOdometerState ";
                lStrStream << (s32)meOdometerState;
                lStrStream << " in OdometerComponent::TransOut().\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    lacMessage,
                    "..\\..\\..\\GameSource\\Gui/Flow/HUD/Components/BrnOdometerComponent.cpp",
                    392);
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        meOdometerState    = E_ODOMETERSTATE_NONE;
        mfEnteredStateTime = mpGuiCache->GetTime();
    }
}
