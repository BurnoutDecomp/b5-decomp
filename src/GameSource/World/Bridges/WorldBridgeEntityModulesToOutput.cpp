#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"      // CgsModule::VariableEventQueue<N,16>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gpDebugPrint (PROPS-BOOT one-shot)

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

// ----------------------------------------------------------------------------
// BridgePropToOutput_PreScene  @ 0x827AF258   (47 instructions)
//   ADDED 2026-08-12 (prop-spawn wave, agent B6) -- was an inert WorldLinkStubs gate.
//
// ⭐ WHY IT MATTERS: this is the PRE-SCENE half of the prop module's resource pipe. Its
// Prepare-phase twin (BridgePropResourceRequestsToOutput_Prepare @0x827AF1D0, above) was
// already real, but PropEntityModule::PreSceneUpdate -> UpdateStreaming is where the prop
// streamer actually raises its GetPropInstances / prop-graphics requests, and it raises them
// into the PRE-SCENE output buffer. With this bridge gated those requests were written every
// frame and then thrown away when the buffer was recycled -- they never reached
// UpdateOutputBuffer, so LoadingScriptedState::UpdateWorldModule / BridgeWorldToResource
// never handed them to the GameData module and no prop bundle was ever loaded.
//
// The console body, statement for statement (asserts at the X360's own
// ../World/Bridges/WorldBridgeEntityModulesToOutput.cpp:268/269, local at :273):
//     assert lpWorldOutput != NULL                                              (:268)
//     assert lpPropOutputBuffer_PreScene != NULL                                (:269)
//     v5 = PropEntityIO::OutputBuffer_PreScene::GetResourceRequestInterface() const
//                                                     (0x827A1A18, read-lock, src+4)
//     VariableEventQueue<4096,16>::Append<1024,16>(
//         UpdateOutputBuffer::GetResourceRequestResourceInterface(out), v5 )
//     GuiOverheadSignInfoEvent lGuiInfo;                                        (:273)
//     lGuiInfo.<array>.Construct()                    ; `li r11,0 ; stw r11, sp+0x450`
//     Array<BrnGui::OverheadSignScore,32>::AppendArray<32>(
//         &lGuiInfo, PropEntityIO::OutputBuffer_PreScene::GetVisibleOverheadSignArray() const )
//     VariableEventQueue<32768,16>::AddEvent(
//         UpdateOutputBuffer::GetGuiEventQueue(out), &lGuiInfo, 210, 1040 )
// (1040 == sizeof(Array<OverheadSignScore,32>) rounded to the type's 16-byte alignment:
//  32 * 0x20 elements + the trailing count word; the `stw 0` at sp+0x400 IS that count word,
//  which is what identifies the event's payload as the bare array.)
//
// The tail forwards AddEvent's result in r3 as a register artifact; the logical return type
// is void (DWARF unity :9723).
//
// [FLAG PARKED -- the overhead-sign GUI leg] Only the resource-request transfer is
// reproduced. The second leg needs two types that have NO committed home:
//   * BrnGui::GuiOverheadSignInfoEvent (the 1040-byte event and its
//     VisibleOverheadSignArray typedef) -- absent from the tree; and
//   * PropEntityIO::OutputBuffer_PreScene::mVisibleOverheadSignArray, which that buffer's
//     home models as a 1-byte opaque VisibleOverheadSignArrayStorage, so there is nothing to
//     AppendArray FROM.
// Its producer is parked for the same reason: the overhead-sign refresh tail of
// PropEntityModule::GenerateDispatchLists (@0x822FBE20, see BrnPropEntityModule_Render.cpp's
// park list #3) is not reconstructed either, so the source array is empty on this build and
// dropping the leg is the consistent observable. It is a HUD score-marker feature -- no prop
// spawns or renders because of it. LAND IT WHEN: GuiOverheadSignInfoEvent gets a home and
// OutputBuffer_PreScene::VisibleOverheadSignArrayStorage is retyped to the real array.
// ----------------------------------------------------------------------------
void BridgePropToOutput_PreScene(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutput_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpOutputBuffer != 0, "lpWorldOutput != NULL");                            // :268
    CGS_ASSERT(lpPropOutput_PreScene != 0, "lpPropOutputBuffer_PreScene != NULL");       // :269

    // The prop streamer's staged GameData requests. Same cross-home reference-cast as the
    // Prepare twin above: PropEntityIO's resource-request interface is still an opaque,
    // correctly-sized span whose embedded VariableEventQueue<1024,16> sits at offset 0
    // (the interface IS its queue), which is exactly what the console passes.
    const auto* lpSourceInterface = lpPropOutput_PreScene->GetResourceRequestInterface();

    lpOutputBuffer->GetResourceRequestResourceInterface()->mRequestQueue.Append<1024, 16>(
        reinterpret_cast<const CgsModule::VariableEventQueue<1024, 16>&>(*lpSourceInterface));

    // DIAGNOSTIC (prop-spawn wave 2026-08-12) -- NOT in the X360 binary, and NOT gated on
    // gxMessageFilterFlags, so a boot log always answers "did a prop resource request ever
    // leave the prop module?". Boot-log grep: "PROPS-BOOT prop->output requests".
    {
        static bool sbLoggedFirstRequests = false;
        if ( !sbLoggedFirstRequests && CgsDev::Log::gpDebugPrint != 0 )
        {
            const s32 liQueued = reinterpret_cast<const CgsModule::VariableEventQueue<1024, 16>&>(
                                     *lpSourceInterface ).GetLength();
            if ( liQueued != 0 )
            {
                sbLoggedFirstRequests = true;
                *CgsDev::Log::gpDebugPrint
                    << "PROPS-BOOT prop->output requests first transfer: events "
                    << liQueued << "\n";
            }
        }
    }
}

// ----------------------------------------------------------------------------
// BridgeEntityModulesToOutput_PostPhysics  @ 0x827AEEB0
//
// The post-physics output fan-in. THE STREAMER LEG IS THE FIRST TRANSFER: the
// world-entity post-physics output's staged GameData resource requests are
// appended into the world update-output's request interface --
//     v12 = WorldEntityIO::OutputBuffer_PostPhysics::GetResourceRequestInterface(a6);
//     VariableEventQueue<4096,16>::Append<4096,16>(
//         UpdateOutputBuffer::GetResourceRequestResourceInterface(a2), v12);
// -- which LoadingScriptedState::UpdateWorldModule then forwards into the
// GameData input (BridgeWorldToResource @0x823E5300), i.e. this is how a TRK_UNIT
// load request raised by the streamer in PreSceneUpdate leaves the world module.
//
// PARTIAL SLICE (FLAG): the console body continues with the traffic/race-car/prop
// legs (traffic resource requests under the miUT_* monitor, the traffic scene-result
// and game-event appends, BridgeRaceCarEntityInfoToOutput_PostPhysics, the prop VFX/
// physical/update-notification queues and the crash-network output). Every one of
// those SOURCE modules is itself a documented inert boot gate on this build, so
// dropping their transfers is the consistent observable; they land with their
// modules. The four X360 null tripwires (:74-:77) are reproduced.
// ----------------------------------------------------------------------------
void BridgeEntityModulesToOutput_PostPhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutput_PostPhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutput_PostPhysics,
    const BrnWorld::PropEntityIO::OutputBuffer_PostPhysics* lpPropOutput_PostPhysics,
    const BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics* lpWorldEntityOutput_PostPhysics)
{
    (void)lpWorldModule;
    (void)lpPropOutput_PostPhysics;

    CGS_ASSERT(lpOutputBuffer != 0, "lpWorldOutput != NULL");                                   // :74
    CGS_ASSERT(lpTrafficOutput_PostPhysics != 0, "lpTrafficOutputBuffer_PostPhysics != NULL");  // :75
    CGS_ASSERT(lpRaceCarOutput_PostPhysics != 0, "lpRaceCarOutputBuffer_PostPhysics != NULL");  // :76
    CGS_ASSERT(lpPropOutput_PostPhysics != 0, "lpPropOutputBuffer_PostPhysics != NULL");        // :77

    // The world-entity (streamer) resource-request flush.
    if (lpWorldEntityOutput_PostPhysics != 0)
    {
        lpOutputBuffer->GetResourceRequestResourceInterface()->mRequestQueue.Append(
            lpWorldEntityOutput_PostPhysics->GetResourceRequestInterface()->mRequestQueue);
    }

    // The RACE-CAR (per-car streamer) resource-request flush -- the same transfer, one
    // buffer over. RaceCarEntityModule::PostPhysicsUpdate -> SendStreamerEvents @0x82304F70
    // has just drained the five component streamers' rings into this buffer's request
    // interface; without this append they never reach the GameData module and no VEH_
    // bundle is ever requested. (Part of the console's
    // BridgeRaceCarEntityInfoToOutput_PostPhysics leg, whose other transfers -- the
    // scene/game-event/network queues -- still reach un-homed interiors and stay dropped.)
    if (lpRaceCarOutput_PostPhysics != 0)
    {
        lpOutputBuffer->GetResourceRequestResourceInterface()->mRequestQueue.Append(
            lpRaceCarOutput_PostPhysics->GetResourceRequestInterface()->mRequestQueue);

        // ⭐ THE NEW-VEHICLE QUEUE TRANSFER (added 2026-08-02, camera parameter-chain wave).
        // X360-attested by the xref set: BrnWorldIO::UpdateOutputBuffer::
        // SetDirectorVehicleInputInterface @0x827AD1A0 has exactly ONE caller in the whole
        // image, and it is THIS function (@0x827AEEB0). The setter is the inlined
        // BrnDirectorVehicleInputInterface::Append -- it Clears the destination queue and
        // merges the source's events, which is why it is safe to run unconditionally every
        // frame. (@0x827AEEB0 is one of the holes in the JSON export set, so the leg's
        // POSITION inside this function is not asm-ordered; it is placed with the other
        // race-car transfers, which is the only group it can belong to.)
        //
        // ⭐⭐ WHY IT MATTERS: this is the only way a car's attribute key reaches the
        // director. Without it BridgeWorldToDirector step 6 has an empty source, so
        // MainDirector::ProcessNewVehicleEvents never runs, Parameters::Set never runs, and
        // BOTH shared gameplay cameras stay on mbIsValid == false -- i.e. the chase camera's
        // whole Update body is skipped. Full map in BrnBehaviourGameplayExternal.h.
        lpOutputBuffer->SetDirectorVehicleInputInterface(
            lpRaceCarOutput_PostPhysics->GetDirectorVehicleInputInterface());

        // ⭐ THE RACE-CAR OUTPUT-INTERFACE TRANSFER. The console runs this as its own
        // named leg; see the function below.
        BridgeRaceCarEntityInfoToOutput_PostPhysics(lpWorldModule, lpOutputBuffer,
                                                    lpRaceCarOutput_PostPhysics);
    }
}

// ----------------------------------------------------------------------------
// BridgeRaceCarEntityInfoToOutput_PostPhysics  @ 0x827ADC38
//
// The console body, statement for statement:
//     assert lpWorldOutput != NULL                                   (:402)
//     assert lpRaceCarOutputBuffer_PostPhysics != NULL               (:403)
//     StartMonitor(worldModule + 6167720);
//     SetActiveRaceCarOutputInterface      ( out, in->GetActiveRaceCarOutputInterface() );
//     SetReplayActiveRaceCarOutputInterface( out, in->GetReplayActiveRaceCarOutputInterface() );
//     SetRaceCarGlobalOutputInterface      ( out, in->GetGlobalRaceCarOutputInterface() );
//     VariableEventQueue<1536,16>::Append<1536,16>( out->GetGameEventQueue(),
//                                                   in->GetGameEventQueue() );
//     StopMonitor(...);
//
// ⭐ WHY THIS MATTERS: RaceCarEntityModule::UpdateOutputInterfaces is the only producer of
// RCEntityActiveRaceCarOutputInterface, and THIS is the only thing that carries its answer
// out of the race-car module. Everything downstream -- WorldModule::Update's own player
// position/speed latch, the scoring system, and BrnGameModule::BridgeWorldToDirector's
// per-car VehicleInfo publish -- reads the world update-output copy this writes.
//
// [FLAG PC bring-up] the REPLAY GLOBAL interface has no destination: the world update-output
// buffer has no replay-global member and the console's own leg is the three above plus the
// game-event append (it never forwards the replay global one either).
// [FLAG] the game-event queue append is dropped: the race-car module's game-event producers
// (SendGameEvents @ the console's own PostPhysicsUpdate tail) are un-homed on this build, so
// the source ring is always empty; the transfer lands with them.
// [FLAG] the console's CPU monitor (worldModule + 6167720) is not modelled on this leg --
// the sibling bridges in this file take the same disposition.
// ----------------------------------------------------------------------------
void BridgeRaceCarEntityInfoToOutput_PostPhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutput_PostPhysics)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpOutputBuffer != 0, "lpWorldOutput != NULL");                                  // :402
    CGS_ASSERT(lpRaceCarOutput_PostPhysics != 0, "lpRaceCarOutputBuffer_PostPhysics != NULL"); // :403

    if (lpOutputBuffer == 0 || lpRaceCarOutput_PostPhysics == 0)
    {
        return;
    }

    lpOutputBuffer->SetActiveRaceCarOutputInterface(
        lpRaceCarOutput_PostPhysics->GetActiveRaceCarOutputInterface());
    lpOutputBuffer->SetReplayActiveRaceCarOutputInterface(
        lpRaceCarOutput_PostPhysics->GetReplayActiveRaceCarOutputInterface());
    lpOutputBuffer->SetRaceCarGlobalOutputInterface(
        lpRaceCarOutput_PostPhysics->GetGlobalRaceCarOutputInterface());
}


// ----------------------------------------------------------------------------
// BridgeWorldResourceRequestsToOutput_Prepare  @ 0x827ADA28
//   Append the world-entity prepare output's staged resource requests into the
//   world update-output request interface. (Ledger-'reviewed' phantom made real
//   2026-07-24 -- no body had ever been committed.)
// ----------------------------------------------------------------------------
void BridgeWorldResourceRequestsToOutput_Prepare(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
    const BrnWorld::WorldEntityIO::OutputBuffer_Prepare* lpWorldEntityOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    // The source buffer is READ-locked by the caller's LockBuffersForIO bracket
    // (dest write / source read -- CgsModuleUtils.h), so the read side must go
    // through the CONST accessor (@0x827A2728, the read-lock tripwire); the old
    // const_cast into the write accessor tripped "Not locked for writing".
    lpWorldOutput->GetResourceRequestResourceInterface()->Append(
        *lpWorldEntityOutputBuffer_Prepare->GetResourceRequestInterface() );
}

// ----------------------------------------------------------------------------
// BridgeTrafficResourceRequestsToOutput  @ 0x827AD9D8
//   Same append for the traffic prepare output. (Phantom made real 2026-07-24.)
// ----------------------------------------------------------------------------
void BridgeTrafficResourceRequestsToOutput(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare* lpTrafficOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    lpWorldOutput->GetResourceRequestResourceInterface()->Append(
        *lpTrafficOutputBuffer_Prepare->GetResourceRequestInterface() );
}

// ----------------------------------------------------------------------------
// BridgeAIModuleToOutput  @ 0x827AD480
//   Forward the AI prepare/update output into the world update-output buffer:
//   resource requests, route responses, the AI car-output interface, and the
//   AI-raised game events. (Phantom made real 2026-07-24.)
// ----------------------------------------------------------------------------
void BridgeAIModuleToOutput(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
    const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT( lpWorldOutput != 0, "lpWorldOutput != NULL" );
    CGS_ASSERT( lpAIOutputBuffer != 0, "lpAIOutputBuffer != NULL" );

    // [FLAG PC boot gate] BrnAI::AIModuleIO::OutputBuffer is still an opaque image on
    // this build and its four const getters are documented inert gates that return NULL;
    // every one of the four world-side Append/Set entry points below dereferences its
    // argument unguarded. Skip each transfer while its source getter is gated -- delete
    // the guards when the AI output buffer's members land.
    const BrnResource::GameDataIO::RequestInterface<4096>* lpAIRequests =
        lpAIOutputBuffer->GetAIResourceRequestInterface();
    if ( lpAIRequests != 0 )
    {
        lpWorldOutput->AppendResourceRequestInterface( lpAIRequests );
    }
    const BrnWorldIO::UpdateOutputBuffer::RouteResponseQueue* lpRouteResponses =
        lpAIOutputBuffer->GetRouteResponseQueue();
    if ( lpRouteResponses != 0 )
    {
        lpWorldOutput->AppendRouteResponseQueue( lpRouteResponses );
    }
    const BrnAI::AIModuleIO::AICarOutputInterface* lpAICarOutput =
        lpAIOutputBuffer->GetAICarOutputInterfaceConst();
    if ( lpAICarOutput != 0 )
    {
        lpWorldOutput->SetAICarOutputInterface( lpAICarOutput );
    }
    const BrnWorldIO::UpdateOutputBuffer::GameEventQueue* lpAIGameEvents =
        lpAIOutputBuffer->GetGameEventQueueConst();
    if ( lpAIGameEvents != 0 )
    {
        lpWorldOutput->AppendGameEventQueue( lpAIGameEvents );
    }
}

// ----------------------------------------------------------------------------
// BridgeWorldEntityInfoToOutput  @ 0x827ADD78
//   The pre-scene WORLD-ENTITY flush -- the leg the world streamer's per-frame
//   traffic rides out of the world module. Two forwards, exactly as the X360:
//     * the world-entity pre-scene out-event queue (VariableEventQueue<1536,16>,
//       read through OutputBuffer_PreScene::GetGameEventQueue() const @0x827A29E0)
//       is appended into the update output's game-event queue
//       (UpdateOutputBuffer::GetGameEventQueue() @0x827A4B30, then
//       VariableEventQueue<1536,16>::Append<1536,16>);
//     * the world-entity sound world-load events (GetSoundWorldLoadInterface()
//       const @0x827A2C80) are appended through
//       UpdateOutputBuffer::AppendSoundWorldLoadInterface @0x827AA7C8.
//   Both buffers are locked by the CALLER (WorldModule::Update brackets the whole
//   pre-scene output bridge set with LockBuffersForIO), so no locking here.
//   The X360 tail returns the AppendSoundWorldLoadInterface result in r3; the
//   logical return type is void.
// ----------------------------------------------------------------------------
void BridgeWorldEntityInfoToOutput(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldEntityOutput_PreScene)
{
    (void)lpWorldModule;   // X360 r3 -- never read by this bridge

    lpOutputBuffer->GetGameEventQueue()->Append(
        *lpWorldEntityOutput_PreScene->GetGameEventQueue() );

    lpOutputBuffer->AppendSoundWorldLoadInterface(
        lpWorldEntityOutput_PreScene->GetSoundWorldLoadInterface() );
}

}   // namespace WorldModule
