#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"      // CgsModule::VariableEventQueue<N,16>

// WorldModule entity-modules -> update-output bridges, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX.
//
// The null tripwires are NON-gating (the X360 falls through after firing the assert). The
// X360-baked d:\p4 file/line pairs are intentionally not reproduced -- CGS_ASSERT stamps
// __FILE__/__LINE__; the X360 source line is noted in a trailing comment. Every function
// tail-forwards the last call's result, but these bridges are logically void (the returned
// register is an artifact of the tail branch).
//
// Resource-request appends (Prepare phase): the world side handle
// (UpdateOutputBuffer::GetResourceRequestResourceInterface -> RequestInterface<4096>) exposes
// its embedded VariableEventQueue<4096,16> as mRequestQueue. The race-car / prop module output
// buffers hand back their own resource-request interface as an opaque, still-un-homed byte span
// (RaceCarEntityModuleIO::ResourceRequestInterface == 8208 B, PropEntityIO ResourceRequest ==
// RequestInterface<1024>); both are layout-compatible with their embedded VariableEventQueue at
// offset 0 (the interface IS its queue), so the source is bound via a reference-cast -- matching
// the X360, which passes the interface pointer straight into
// VariableEventQueue<4096,16>::Append<SRC,16>.

namespace WorldModule
{

// @ 0x827AD950
void BridgeRaceCarResourceRequestsToOutput_Prepare(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_Prepare* lpRaceCarOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpWorldOutput != 0, "lpWorldOutput != NULL");                                   // :169
    CGS_ASSERT(lpRaceCarOutputBuffer_Prepare != 0, "lpRaceCarOutputBuffer_Prepare != NULL");   // :170

    const auto* lpSourceInterface = lpRaceCarOutputBuffer_Prepare->GetResourceRequestInterface();

    lpWorldOutput->GetResourceRequestResourceInterface()->mRequestQueue.Append<8192, 16>(
        reinterpret_cast<const CgsModule::VariableEventQueue<8192, 16>&>(*lpSourceInterface));
}

// @ 0x827AF1D0
void BridgePropResourceRequestsToOutput_Prepare(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
    const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpWorldOutput != 0, "lpWorldOutput != NULL");                                 // :247
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");       // :248

    const auto* lpSourceInterface = lpPropOutputBuffer_Prepare->GetResourceRequestInterface();

    lpWorldOutput->GetResourceRequestResourceInterface()->mRequestQueue.Append<1024, 16>(
        reinterpret_cast<const CgsModule::VariableEventQueue<1024, 16>&>(*lpSourceInterface));
}

// @ 0x827AEDE0
void BridgeEntityModulesToOutput_PrePhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutput_PrePhysics,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutput_PrePhysics,
    const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PrePhysics* lpTriggerOutput_PrePhysics)
{
    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");                          // :42
    CGS_ASSERT(lpRaceCarOutput_PrePhysics != 0, "lpRaceCarOutput_PrePhysics");  // :43
    CGS_ASSERT(lpTriggerOutput_PrePhysics != 0, "lpTriggerOutput_PrePhysics");  // :44

    BridgeRaceCarEntityInfoToOutput_PrePhysics(lpWorldModule, lpOutputBuffer, lpRaceCarOutput_PrePhysics);
    BridgeTrafficCarEntityInfoToOutput_PrePhysics(lpWorldModule, lpOutputBuffer, lpTrafficOutput_PrePhysics);

    lpOutputBuffer->SetTriggerEntityOutputInterface(lpTriggerOutput_PrePhysics->GetOutputInterface());
}

}   // namespace WorldModule
