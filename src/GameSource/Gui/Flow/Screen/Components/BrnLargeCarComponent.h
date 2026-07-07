#ifndef BRN_LARGE_CAR_COMPONENT_H
#define BRN_LARGE_CAR_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsID (== u64)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"              // BrnGui::IconComponent (base) + SetState(const char*)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"      // CgsGui::GuiEventAptTrigger
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"    // CgsGui::GuiEventLoad/UnloadNotification

// BrnGui::LargeCarComponent - an apt-driven screen component that shows a single large car
// panel (used by the offline rival-shutdown / trophy-car-unlock / instant-results flows).
// It is configured with a CgsID car id + a resource id-type, formats the car's printable
// name into a "CAR_%s" apt string, requests/releases the car's streamed resources through
// its owning StateInterface, and steps a small internal state machine (constructed -> data
// supplied -> resources requested -> ready -> showing -> hiding -> hidden -> unload
// requested) as the apt movie transitions.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (class:BrnGui::LargeCarComponent). Shape from
// DecFIGS DWARF (BrnLargeCarComponent.h): public IconComponent base; the EInternalStates
// enum; and the instance members meCurrentState/macCarNameText/mpGuiCache/muResourceIdType/
// mCurrentCarId. Member BYTE OFFSETS pinned to the X360 asm (Construct stores at 0x94/0x98/
// 0xB8/0xBC/0xC0; SetCarInfo stw 0xBC / std 0xC0 / stb 0xB7). IconComponent occupies +0x93.

namespace CgsGui { class StateInterface; }
namespace BrnGui { class GuiCache; }

namespace BrnGui
{
    // The apt view-state resource id-type handed to SetCarInfo (guest passes a u32).
    typedef u32 BrnGuiResourceId;

    class LargeCarComponent : public IconComponent
    {
    public:
        // DWARF BrnLargeCarComponent.h:129 -- the component's internal lifecycle states.
        enum EInternalStates
        {
            E_INTERNAL_STATE_CONSTRUCTED         = 0,
            E_INTERNAL_STATE_DATA_SUPPLIED       = 1,
            E_INTERNAL_STATE_RESOURCES_REQUESTED = 2,
            E_INTERNAL_STATE_READY               = 3,
            E_INTERNAL_STATE_SHOWING             = 4,
            E_INTERNAL_STATE_HIDING              = 5,
            E_INTERNAL_STATE_HIDDEN              = 6,
            E_INTERNAL_STATE_UNLOAD_REQUESTED    = 7,
            E_INTERNAL_STATE_COUNT               = 8,
        };

        // 0x8241B658 -- run the base IconComponent::Construct then zero this class's members.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // 0x8241B6B0 -- record the selected car id + resource id-type, format its printable
        // name into "CAR_%s", and advance to E_INTERNAL_STATE_DATA_SUPPLIED.
        void SetCarInfo(CgsID lCarId, BrnGuiResourceId luResourceIdType);

        // 0x8243DAF8 -- once resources are ready, play the "CAR_<id>" apt movie (level 4).
        void OnLoad();

        // 0x8243DB98 -- if resources are outstanding, play the unload apt movie, request the
        // resource release, and move to E_INTERNAL_STATE_UNLOAD_REQUESTED.
        void ReleaseResources();

        // 0x8241B738 -- assert >= READY, drive the apt "transIn", and mark SHOWING.
        void ShowCar();

        // 0x8241B928 -- handle the apt E_APT_EVENT_ONLOAD trigger for the bound name.
        bool HandleAptLoadTriggers(const CgsGui::GuiEventAptTrigger* lpAptTrigger);

        // 0x8241B9F8 -- handle the apt E_APT_EVENT_TRANSITION_COMPLETE trigger for the bound name.
        bool HandleAptTransitionTriggers(const CgsGui::GuiEventAptTrigger* lpAptTrigger);

        // 0x8241B7A8 -- resource load notification: RESOURCES_REQUESTED -> READY.
        void HandleLoadNotification(const CgsGui::GuiEventLoadNotification* lpLoadEvent);

        // 0x8241B868 -- resource unload notification: UNLOAD_REQUESTED -> DATA_SUPPLIED.
        void HandleUnloadNotification(const CgsGui::GuiEventUnloadNotification* lpUnloadEvent);

    private:
        // DWARF BrnLargeCarComponent.h:127 -- car-name buffer capacity.
        static const s32 KI_MAX_CAR_NAME_LENGTH = 32;

        EInternalStates  meCurrentState;                         // +0x94
        char             macCarNameText[KI_MAX_CAR_NAME_LENGTH]; // +0x98 .. +0xB7
        GuiCache*        mpGuiCache;                             // +0xB8
        u32              muResourceIdType;                       // +0xBC
        CgsID            mCurrentCarId;                          // +0xC0
    };
}

#endif
