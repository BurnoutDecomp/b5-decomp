#pragma once

#include "types.hpp"
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ChallengePlayerScoreEntry (HandleNewPersonalBest param)

// Minimal reconstructed surface for the online road-rules manager (BrnNetwork::NetworkRoadRulesManager).
// Only the two entry points its debug component drives are declared here; the full manager - message
// queues, player score tables, the delivered/arrived callbacks - is reconstructed in its own (class-
// sourced) TU and extends this header. Declarations are enough for the per-TU `cl /c` gate; the bodies
// link from the manager's own TU.

namespace BrnNetwork
{
    class NetworkRoadRulesManager
    {
    public:
        // Debug "Trigger Personal Best": feed a fabricated personal-best record through the normal
        // new-PB pipeline (the debug component builds the record on the stack and hands it over).
        void HandleNewPersonalBest( BrnStreetData::ChallengePlayerScoreEntry* lpScoreEntry );

        // Debug "Get Road Rules High Scores": kick off the server-side road-rules score download.
        void StartDownloadingRoadRulesScoresFromServer();
    };
}
