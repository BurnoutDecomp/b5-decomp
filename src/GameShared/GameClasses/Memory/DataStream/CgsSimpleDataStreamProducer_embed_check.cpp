// Tiny embed/compile check for the CgsMemory::SimpleDataStreamProducer home:
// includes the header and touches the reconstructed entry point so the gate
// exercises the TU.
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"

namespace
{
void CgsSimpleDataStreamProducerEmbedCheck()
{
    CgsMemory::SimpleDataStreamProducer lProducer{};
    char lCommandBuffer[256] = {};
    char lResultBuffer[256] = {};
    lProducer.Construct(8, 16, lCommandBuffer, 8, 16, lResultBuffer);
}
} // namespace
