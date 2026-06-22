// Embedder gate for the ScoringSystemDebugComponent TU: proves the component derives cleanly from the
// real CgsDev::DebugComponent base and that a caller can reach the public render entry by name with
// the recovered argument shape. Compile-only; not part of the shipped tree.
#include "GameSource/GameState/ModeManager/Debug/BrnScoringSystemDebugComponent.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnGameState
{
void ScoringSystemDebugComponent_EmbedGate_Probe(
    ScoringSystemDebugComponent* lpComponent,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarOutput,
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpPlayerStatusInterface)
{
    // The owner (ModeManager::PreWorldUpdate) drives the render each frame through this entry, passing
    // the per-cell max multiplier (a6) and the 30fps display flag (a7) it forwards to every cell.
    lpComponent->DebugRenderChainableStunts(lpActiveCarOutput, lpPlayerStatusInterface, 0, false);
}
}
