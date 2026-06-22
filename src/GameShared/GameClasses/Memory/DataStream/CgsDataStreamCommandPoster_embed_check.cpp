// Tiny embed/compile check for the CgsMemory::DataStreamCommandPoster home:
// includes the header and touches both reconstructed entry points so the gate
// exercises the TU.
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

namespace
{
void CgsDataStreamCommandPosterEmbedCheck()
{
    CgsMemory::DataStreamCommandPoster lPoster{};
    int lCommand = 0;
    void* lpSlot = 0;
    (void)lPoster.AddCommand(&lCommand);
    (void)lPoster.AllocateCommand(&lpSlot);
    (void)lpSlot;
}
} // namespace
