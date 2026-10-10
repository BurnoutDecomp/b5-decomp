#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::InputBuffer
#include "GameSource/World/BrnWorldModuleIO.h"                            // BrnWorldIO::UpdateOutputBuffer

// GameBridgeWorldToX partfile (blocked-TU wave), beside its home GameBridgeWorldToX.cpp.

namespace BrnGame
{
    // GameBridgeWorldToX.cpp. Carry the world's per-frame resource requests (the
    // streamer's bundle loads) and its AttribSys vault requests into the GameData input.
    void BrnGameModule::BridgeWorldToResource(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                                              const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput)
    {
        lpGDMInput->AppendRequestInterface(*lpWorldOutput->GetResourceRequestResourceInterface());
        lpGDMInput->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpWorldOutput->GetAttribSysVaultRequestInterface()->mRequestQueue);
    }
}
