// ===================================================================================
// BrnGui::LargeCarComponent -- out-of-line bodies (class:BrnGui::LargeCarComponent).
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x8241B658, SetCarInfo @0x8241B6B0, ShowCar @0x8241B738,
//   HandleLoadNotification @0x8241B7A8, HandleAptLoadTriggers @0x8241B928,
//   HandleAptTransitionTriggers @0x8241B9F8, OnLoad @0x8243DAF8, ReleaseResources @0x8243DB98.
// (HandleUnloadNotification @0x8241B868 is SKIPPED this wave -- its event record type is
//  not yet homed.)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnLargeCarComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface

#include <cstring>   // memset / strcmp

namespace BrnGui
{
    // @ 0x8241B658
    void LargeCarComponent::Construct(const char* lpacName,
                                      CgsGui::StateInterface* lpStateInterface,
                                      const char* lpacParentName)
    {
        // Base init with a null identifier table (li r6,0 forwarded as the table arg).
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        meCurrentState = E_INTERNAL_STATE_CONSTRUCTED;   // stw 0, 0x94
        memset(macCarNameText, 0, sizeof(macCarNameText));
        mpGuiCache       = 0;                            // stw 0, 0xB8
        muResourceIdType = 0;                            // stw 0, 0xBC
        mCurrentCarId    = 0;                            // std 0, 0xC0
    }

    // @ 0x8241B6B0
    void LargeCarComponent::SetCarInfo(CgsID lCarId, BrnGuiResourceId luResourceIdType)
    {
        CGS_ASSERT(lCarId != 0, "kCGSID_NULL != lCarId");

        muResourceIdType = static_cast<u32>(luResourceIdType);   // this+0xBC
        mCurrentCarId    = lCarId;                                // this+0xC0 (full 64-bit CgsID)

        char lacCarIdText[16];
        CgsIDConvertToString(lCarId, lacCarIdText);
        CgsCore::SnPrintf(macCarNameText, KI_MAX_CAR_NAME_LENGTH, "CAR_%s", lacCarIdText);

        macCarNameText[KI_MAX_CAR_NAME_LENGTH - 1] = '\0';       // this+0xB7 (guarantee terminator)
        meCurrentState = E_INTERNAL_STATE_DATA_SUPPLIED;         // this+0x94
    }

    // @ 0x8241B738
    void LargeCarComponent::ShowCar()
    {
        CGS_ASSERT(meCurrentState >= E_INTERNAL_STATE_READY,
                   "E_INTERNAL_STATE_READY <= meCurrentState");

        SetState("transIn");
        meCurrentState = E_INTERNAL_STATE_SHOWING;
    }

    // @ 0x8241B7A8
    void LargeCarComponent::HandleLoadNotification(const CgsGui::GuiEventLoadNotification* lpLoadEvent)
    {
        CGS_ASSERT(lpLoadEvent != 0, "lpLoadEvent");

        if (lpLoadEvent->meRequestType == CgsGui::E_GUI_RESOURCETYPE_APT &&
            muResourceIdType == lpLoadEvent->muLoadRequestId)
        {
            CGS_ASSERT(E_INTERNAL_STATE_RESOURCES_REQUESTED == meCurrentState,
                       "E_INTERNAL_STATE_RESOURCES_REQUESTED == meCurrentState");
            CGS_ASSERT(muResourceIdType != 0, "muResourceIdType != 0");
            meCurrentState = E_INTERNAL_STATE_READY;
        }
    }

    // @ 0x8241B928
    bool LargeCarComponent::HandleAptLoadTriggers(const CgsGui::GuiEventAptTrigger* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger != 0, "lpAptTrigger");
        CGS_ASSERT(CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD == lpAptTrigger->meEventType,
                   "CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD == lpAptTrigger->meEventType");

        bool lbHandled = false;
        if (strcmp(GetName(), lpAptTrigger->mpacComponentName) == 0)
        {
            if (meCurrentState == E_INTERNAL_STATE_SHOWING)
            {
                SetState("transIn");
            }
            lbHandled = true;
        }
        return lbHandled;
    }

    // @ 0x8241B9F8
    bool LargeCarComponent::HandleAptTransitionTriggers(const CgsGui::GuiEventAptTrigger* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger != 0, "lpAptTrigger");
        CGS_ASSERT(CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE == lpAptTrigger->meEventType,
                   "CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE == lpAptTrigger->meEventType");

        bool lbHandled = false;
        if (strcmp(GetName(), lpAptTrigger->mpacComponentName) == 0)
        {
            lbHandled = true;
            if (meCurrentState == E_INTERNAL_STATE_HIDING)
            {
                meCurrentState = E_INTERNAL_STATE_HIDDEN;
            }
            else
            {
                CGS_ASSERT(false,
                           "This component should not generate a trans complete in this state (");
            }
        }
        return lbHandled;
    }

    // @ 0x8243DAF8
    void LargeCarComponent::OnLoad()
    {
        CGS_ASSERT(meCurrentState >= E_INTERNAL_STATE_READY,
                   "E_INTERNAL_STATE_READY <= meCurrentState");

        mpStateInterface->PlayAptMovie(macCarNameText, 4);
    }

    // @ 0x8243DB98
    void LargeCarComponent::ReleaseResources()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (meCurrentState >= E_INTERNAL_STATE_RESOURCES_REQUESTED &&
            meCurrentState <  E_INTERNAL_STATE_UNLOAD_REQUESTED)
        {
            CGS_ASSERT(muResourceIdType != 0, "muResourceIdType != 0");

            mpStateInterface->PlayAptMovie("", 4);
            mpStateInterface->RequestResource(
                macCarNameText,
                CgsGui::E_GUI_RESOURCETYPE_APT,
                static_cast<s32>(muResourceIdType),
                CgsGui::E_GUI_RESOURCEREQUEST_UNLOAD);
            meCurrentState = E_INTERNAL_STATE_UNLOAD_REQUESTED;
        }
    }
}
