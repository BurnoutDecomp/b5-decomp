// =============================================================================
// BrnWorld::WorldEntityModule::BridgePVSToOutput_Prepare
//   GameSource/World/EntityModules/WorldEntityModule/Bridges/
//     WorldEntityBridgePVSToOutput.cpp  (DWARF home, :36)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x822F9EC8.
// While the PVS module is still preparing, forward its GameData requests
// (RequestInterface<512>) into the world-entity prepare output's
// RequestInterface<4096>.
// =============================================================================

#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{

void
WorldEntityModule::BridgePVSToOutput_Prepare( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    CGS_ASSERT( lpOutputBuffer, "lpOutputBuffer" );

    lpOutputBuffer->GetResourceRequestInterface()->Append(
        *mPVSModule.GetGameDataRequestInt() );
}

} // namespace BrnWorld
