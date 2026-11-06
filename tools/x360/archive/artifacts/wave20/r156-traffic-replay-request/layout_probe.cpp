#include "GameSource/World/BrnWorldModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

typedef BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics TrafficOutput;
typedef TrafficOutput::ReplayRequestInterface ReplayRequests;

CGS_STATIC_ASSERT((sizeof(ReplayRequests) == 0x2C), "eleven native pointers");
CGS_STATIC_ASSERT((__alignof(ReplayRequests) == 4), "native pointer alignment");
CGS_STATIC_ASSERT((sizeof(TrafficOutput) == 0xD3D20), "post-physics buffer size");
CGS_STATIC_ASSERT((__alignof(TrafficOutput) == 16), "post-physics buffer alignment");

void ProbeRegistration(TrafficOutput* lpTraffic, BrnReplays::BaseSerialiser* lpSerialiser)
{
    lpTraffic->GetReplayRequestInterface()->RegisterSerialiser(lpSerialiser);
}

void ProbeWorldForwarding(BrnWorldIO::UpdateOutputBuffer* lpWorld, const TrafficOutput* lpTraffic)
{
    lpWorld->AppendReplayRequestInterface(lpTraffic->GetReplayRequestInterface());
}
