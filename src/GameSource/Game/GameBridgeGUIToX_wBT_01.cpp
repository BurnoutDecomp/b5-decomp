#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                    // CgsGuiModuleIO::OutputBuffer (out events + game actions)
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"            // CgsGui::ModelIO::OutputBuffer::GetGuiResourceRequestQueue
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"      // EventReceiverQueue<256,16> -> BaseEventReceiverQueue
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::InputBuffer
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"         // RequestInterface<256>
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"     // RequestInterface<N>::GetVehicleList / GetWheelList bodies
#include "GameSource/World/BrnWorldModuleIO.h"                            // BrnWorldIO::UpdateInputBuffer
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventNetworkVehicleListRequest / GuiCarSelectWheelRequest
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventRequestCollisionWorldEvent

// GameBridgeGUIToX partfile (blocked-TU wave): the two GUI-output bridges that feed the
// GameData module and the world, beside their home GameBridgeGUIToX.cpp.

namespace BrnGame
{
    // GameBridgeGUIToX.cpp. Forward the GUI model's resource requests into the GameData
    // input, then translate the GUI's vehicle/wheel list requests into GameData list requests
    // built in a local RequestInterface<256> and appended in one block at the end.
    //   263 GuiEventNetworkVehicleListRequest -> GetVehicleList(id 0) + GetWheelList(id 1)
    //   417 GuiCarSelectWheelRequest          -> GetWheelList(id 1)
    // Both reply into the requesting screen's receiver queue.
    void BrnGameModule::BridgeGuiToResource(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                                            const CgsGui::ModelIO::OutputBuffer* lpModelOutput,
                                            const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        static const s32 KI_VEHICLE_LIST_EVENT_ID = 0;
        static const s32 KI_WHEEL_LIST_EVENT_ID   = 1;

        lpGDMInput->GetRequestInterface()->mRequestQueue.Append(*lpModelOutput->GetGuiResourceRequestQueue());

        const CgsGui::CgsGuiModuleIO::OutputBuffer::GuiEventQueue* lpGuiEventQueue = lpGuiOutput->GetOutEventQueue();
        BrnResource::GameDataIO::RequestInterface<256> lRequestInterface;
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
        lRequestInterface.Construct();

        while (lpEvent != 0)
        {
            if (liEventType == 263)
            {
                const BrnGui::GuiEventNetworkVehicleListRequest* lpRequest =
                    reinterpret_cast<const BrnGui::GuiEventNetworkVehicleListRequest*>(lpEvent);
                lRequestInterface.GetVehicleList(lpRequest->mpReceiverQueue, KI_VEHICLE_LIST_EVENT_ID);
                lRequestInterface.GetWheelList(lpRequest->mpReceiverQueue, KI_WHEEL_LIST_EVENT_ID);
            }
            else if (liEventType == 417)
            {
                const BrnGui::GuiCarSelectWheelRequest* lpRequest =
                    reinterpret_cast<const BrnGui::GuiCarSelectWheelRequest*>(lpEvent);
                lRequestInterface.GetWheelList(lpRequest->mpReceiverQueue, KI_WHEEL_LIST_EVENT_ID);
            }
            liEventType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        lpGDMInput->AppendRequestInterface(lRequestInterface);
    }

    // GameBridgeGUIToX.cpp. Turn the GUI's collision-world requests (493) into the
    // world-entity request flags, then hand the GUI's game actions to the world input.
    // The request kind is compared unsigned: 0 invalidates, 1 validates, anything else
    // trips the (non-gating) assert.
    void BrnGameModule::BridgeGuiToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                         const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        typedef BrnGui::GuiEventRequestCollisionWorldEvent CollisionWorldEvent;

        const CgsGui::CgsGuiModuleIO::OutputBuffer::GuiEventQueue* lpGuiEventQueue = lpGuiOutputBuffer->GetOutEventQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != 0)
        {
            if (liEventType == 493)
            {
                const CollisionWorldEvent* lpCollisionWorldEvent = reinterpret_cast<const CollisionWorldEvent*>(lpEvent);
                const u32 luRequest = static_cast<u32>(lpCollisionWorldEvent->meEventType);
                if (luRequest == CollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE)
                    lpWorldInput->GetWorldEntityRequestInterface()->InvalidateCollisionWorld();
                else if (luRequest == CollisionWorldEvent::E_COLLISON_WORLD_VALIDATE)
                    lpWorldInput->GetWorldEntityRequestInterface()->ValidateCollisionWorld();
                else
                    CGS_ASSERT(false, "Invalid collsion world request event type");
            }
            liEventType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        lpWorldInput->GetGameActionQueue()->Append(*lpGuiOutputBuffer->GetGameActionQueue());
    }
}
