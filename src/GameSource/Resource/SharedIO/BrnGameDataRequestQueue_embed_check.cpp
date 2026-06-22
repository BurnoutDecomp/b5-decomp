#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"

#include <cstddef>

// Layout / instantiation self-check for BrnResource::GameDataIO::RequestInterface<4096>.
namespace
{
    // RequestInterface<N> holds exactly one RequestQueue<N> at offset 0, and the whole
    // base chain (RequestQueue -> ResourceRequestQueue -> VariableEventQueue<N,16>) is
    // single-inheritance at offset 0, so the interface is layout-identical to the queue.
    static_assert(offsetof(BrnResource::GameDataIO::RequestInterface<4096>, mRequestQueue) == 0,
                  "RequestInterface<4096>::mRequestQueue must be at offset 0");

    static_assert(sizeof(BrnResource::GameDataIO::RequestInterface<4096>)
                      == sizeof(CgsModule::VariableEventQueue<4096, 16>),
                  "RequestInterface<4096> must be layout-identical to its VariableEventQueue<4096,16>");

    // GameDataAssetEvent inheritance sanity (members exist + are typed as expected).
    // NOTE: NO absolute-offset assert -- mpReceiverQueue is a host 8-byte pointer, so the
    // X360 32-byte size does not hold on the host (pointer-width FLAG, by design).
    static_assert(static_cast<int>(BrnResource::E_ASSETSET_ATTRIBS) == 4, "EAssetSet value");
    static_assert(static_cast<int>(BrnResource::E_ASSETSET_DATA) == 3, "EAssetSet value");
    static_assert(static_cast<int>(BrnResource::E_ASSETSET_PHYSICS) == 1, "EAssetSet value");

    // Force the request-builder methods to be referenced/emitted.
    void EmbedCheckUse(BrnResource::GameDataIO::RequestInterface<4096>* lpInterface,
                       CgsModule::BaseEventReceiverQueue* lpQueue)
    {
        lpInterface->GetSurfaceList(lpQueue, 1, 2);
        lpInterface->GetVehicleList(lpQueue, 1);
        lpInterface->LoadBundle(lpQueue, 1, 2, "test.bundle", true);
        lpInterface->LoadTrafficLanes(lpQueue, 1, 2);
        lpInterface->LoadWorldCollision(lpQueue, 1, 2);
    }
}
