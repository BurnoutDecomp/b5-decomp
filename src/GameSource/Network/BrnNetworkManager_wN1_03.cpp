#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                                           // AddOutputGuiEvent
#include "GameSource/Network/BrnNetworkModuleIO.h"                                         // OutputBuffer::GetPlayerResultsInterface
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                      // GuiEventNetworkPlayerImage
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                  // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                       // GetNextPlayerID
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // IsPlayerInGameByID

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- GUI and results output (partfile).
//
// The player-image display event and the per-frame results-interface writer. The other
// event-handling and output functions of the class wait on record and accessor shapes in
// headers this partfile does not own.
// ============================================================================================

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::PackTextureAndSendDisplayEventToGui
    //
    // Hand a received player image to the GUI for display in the given slot.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::PackTextureAndSendDisplayEventToGui(const CgsNetwork::NetworkTexture* lpTexture,
                                                                s32 liTextureIndex)
    {
        CgsDev::PerfMonCpu::StartMonitor(miPackTextureToSendToGuiPM);

        // The reference record holds a const texture pointer; the committed one does not yet.
        BrnGui::GuiEventNetworkPlayerImage lEvent;
        lEvent.mpTexture      = const_cast<CgsNetwork::NetworkTexture*>(lpTexture);
        lEvent.miTextureIndex = liTextureIndex;
        GetNetworkModule()->AddOutputGuiEvent(lEvent);

        CgsDev::PerfMonCpu::StopMonitor(miPackTextureToSendToGuiPM);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerResultsInfo
    //
    // Write the results record of every finalised player in the game whose standings result
    // has arrived, packed from slot 0.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerResultsInfo(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != NULL, "lpOutput");

        NetworkPlayerID lPlayerID            = CgsNetwork::KI_INVALID_PLAYER_ID;
        s32             liPlayerResultsIndex = 0;

        while (GetPlayerManager()->GetNextPlayerID(&lPlayerID,
                                                   CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            if (GetServerInterface()->GetGameComponent()->IsPlayerInGameByID(lPlayerID) &&
                GetStandingsManager()->ArePlayersResultsValid(lPlayerID))
            {
                BrnNetworkModuleIO::PlayerResultsData* lpPlayerResultsData =
                    lpOutput->GetPlayerResultsInterface()->GetPlayerResultsDataForWriting(liPlayerResultsIndex);
                GetStandingsManager()->FillOutResultsData(lpPlayerResultsData, lPlayerID);
                ++liPlayerResultsIndex;
            }
        }
    }
} // namespace BrnNetwork
