// Tiny embed/compile check for the CgsMemory::DataStreamResultPoster home:
// includes the header and touches the reconstructed entry point so the gate
// exercises the TU.
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultPoster.h"

#include <cstddef>

// The poster owns nothing but the reader back pointer (single member @ +0).
static_assert(sizeof(CgsMemory::DataStreamResultPoster) ==
                  sizeof(CgsMemory::DataStreamResultReader*),
              "DataStreamResultPoster is a thin one-pointer wrapper");

namespace
{
void CgsDataStreamResultPosterEmbedCheck()
{
    CgsMemory::DataStreamResultPoster lPoster{};
    int lResults[4] = {0, 0, 0, 0};
    (void)lPoster.AddResults(lResults, 4);
}
} // namespace
