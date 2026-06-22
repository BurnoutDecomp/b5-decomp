// Embed check for the CgsNet-serverif-2 group: include the three server-interface data
// homes together to catch cross-header ODR / redefinition / include-order problems and
// exercise the bodied function in each TU. Compile-only.
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h"

namespace
{
    // TU3: ServerInterfacePrepareParams::Construct (zero-init the param block).
    void ExerciseConstruct(CgsNetwork::ServerInterfacePrepareParams& lrParams)
    {
        lrParams.Construct();
    }

    // TU1 / TU2: the scalar deleting destructors are exercised by deleting a
    // heap instance through a base pointer (drives the virtual ~ + conditional free).
    void ExerciseServerInfoDtor(CgsNetwork::ServerInterfaceServerInfo* lpInfo)
    {
        delete lpInfo;
    }

    void ExerciseRankingsDtor(CgsNetwork::ServerInterfaceRankings* lpRankings)
    {
        delete lpRankings;
    }

    // Touch one symbol from each header so the translation unit is non-trivial and the
    // function bodies are referenced.
    void (*kpfnConstruct)(CgsNetwork::ServerInterfacePrepareParams&) = &ExerciseConstruct;
    void (*kpfnInfoDtor)(CgsNetwork::ServerInterfaceServerInfo*)     = &ExerciseServerInfoDtor;
    void (*kpfnRankDtor)(CgsNetwork::ServerInterfaceRankings*)       = &ExerciseRankingsDtor;
}
