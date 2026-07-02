#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

// BrnAI::AIModuleIO - the AI module's IO buffer slice the world bridges drive.
// FLAG: MINIMAL slice (per the sibling module-IO precedents) -- only the setter
// the race-car->AI bridge calls is declared; the buffer payload is owned by the
// AI IO TUs. RaceCarAIInterface's own home is
// GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h (pointer-only use here).
namespace BrnAI
{
namespace AIModuleIO
{
    struct RaceCarAIInterface;   // BrnRaceCarAIInterfaces.h

    struct InputBuffer : public CgsModule::IOBuffer
    {
        // Latch the race-car module's pre-scene AI view. Real X360 symbol
        // (BrnAI::AIModuleIO::InputBuffer::SetRaceCarAIInterface, called by
        // WorldModule::BridgeRaceCarModuleToAIModule_PreScene @0x827A5014);
        // declaration-only (its own ledger function).
        void SetRaceCarAIInterface(const RaceCarAIInterface* lpRaceCarAIInterface);
    };
}
}
