// ===================================================================================
// BrnGui::OnlineSelectRoute -- the online route-selection screen state.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.cpp
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"                  // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface (language manager, out queue)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"          // LanguageManager::FormatAndAddText
#include "GameSource/Gui/BrnGuiCache.h"                                  // GuiCache::GetGuiTracker
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiOverlayRequest
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                         // GuiTracker::GetRouteDistance

namespace BrnGui
{
namespace
{
    // The shortest route an online event accepts (rodata 1000.0; the main-option handler
    // tests the route against the same word).
    const f32 KF_MINIMUM_ROUTE_DISTANCE = 1000.0f;

    // The GUI-out channel the overlay request rides.
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // The overlay request as the out-queue carries it: the GuiEvent<184> header sized to the
    // request, a pad word, then the request itself (304 bytes on the console).
    struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
    {
        u32               muPad0C;    // +0x0C
        GuiOverlayRequest mRequest;   // +0x10
        GuiOverlayRequestWire()
            : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
            , muPad0C(0) {}
    };
}

    // the route screen and the shared online frame.
    const CgsGui::sResourceTuple OnlineSelectRoute::maResourceTuplesToLoad[] =
        { { 178, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineSelectRoute::miNumResourcesToLoad = 2;

    // member construction only: the console body installs the state and
    // route-component vtables, runs the two MenuComponent constructors, the HelpBar
    // constructor and the map component's MapManager constructor, and installs the vtables
    // of the text fields, the eight animators, the borough outline and the cursor. No
    // scalar member is stored until OnEnter.
    OnlineSelectRoute::OnlineSelectRoute()
    {
    }

    // Publish the current route length and the minimum as the two small-distance strings
    // "ONLINE_ROUTE_DISTANCE" / "ONLINE_ROUTE_MINIMUM_DISTANCE", then post the "OnRouteShort"
    // overlay request that shows them.
    void OnlineSelectRoute::DisplayTooShortRouteMessage()
    {
        char lacDistance[64];

        CgsCore::SPrintf(lacDistance, sizeof(lacDistance), "%f", mpGuiCache->GetGuiTracker()->GetRouteDistance());
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "ONLINE_ROUTE_DISTANCE", lacDistance, CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);

        CgsCore::SPrintf(lacDistance, sizeof(lacDistance), "%f", static_cast<double>(KF_MINIMUM_ROUTE_DISTANCE));
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "ONLINE_ROUTE_MINIMUM_DISTANCE", lacDistance, CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);

        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct("OnRouteShort");
        lWire.mRequest.AddMessageParam(2, "ONLINE_ROUTE_DISTANCE");
        lWire.mRequest.AddMessageParam(2, "ONLINE_ROUTE_MINIMUM_DISTANCE");
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(GuiOverlayRequestWire)));
    }
}
