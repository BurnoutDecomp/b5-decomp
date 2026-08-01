#include "GameSource/World/Bridges/WorldBridgeToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// WorldModule game-action -> physics / traffic module bridges. Reconstructed from
// BURNOUT_X360_ARTIST.XEX. Each bridge walks the world Update input buffer's game-action
// queue and forwards the events its destination module cares about (a fixed type-id
// allowlist, the X360 switch jump table) verbatim into that module's own queue.

// @ 0x827AC568 -- WorldBridgeToEntityModules.cpp:222. Forward the physics-relevant
// game actions from the world Update input buffer's GameActionQueue into the physics
// module input buffer's GameActionQueue. Both null tripwires are NON-gating (the X360
// fires the assert and falls through). The X360 tail leaves the last GetNextEvent
// result in r3 as a register artifact; the logical return type is void.
void WorldModule::BridgeActionsToPhysicsModule(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    (void)lpWorldModule;

    // Source: the world input buffer's game-action queue (IDA truncated the read-lock
    // GetGameActionQueue accessor's name to "UpdateInputBuffer", BrnWorldModuleIO.h:321).
    const BrnWorldIO::GameActionQueue* lpInQueue = lpWorldInput->GetGameActionQueue();
    // Destination: the physics module input buffer's game-action queue. The buffer models
    // the slot as opaque GameActionQueueStorage; the real type is VariableEventQueue<13312,16>.
    CgsModule::VariableEventQueue<13312, 16>* lpOutQueue =
        reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(
            lpPhysicsModuleInputBuffer->GetGameActionQueue());

    CGS_ASSERT(lpInQueue != 0, "lpInQueue");     // :226 (non-gating)
    CGS_ASSERT(lpOutQueue != 0, "lpOutQueue");   // :227 (non-gating)

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        switch (liType)
        {
        case 7:
        case 11:
        case 23:
        case 34:
        case 37:
        case 39:
        case 42:
        case 43:
        case 65:
        case 97:
        case 98:
        case 99:
        case 116:
        case 135:
        case 138:
        case 146:
        case 176:
        case 198:
            // Forward the event verbatim (same payload pointer, type id, size).
            lpOutQueue->AddEvent(lpEvent, liType, liSize);
            break;
        default:
            break;
        }
        liType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

// @ 0x827ABFF0 -- WorldBridgeToEntityModules.cpp:107. Forward the traffic-relevant game
// actions from the world Update input buffer's GameActionQueue into the traffic module
// input buffer's GameActionQueue. Both null tripwires are NON-gating. The X360 tail
// leaves the last GetNextEvent result in r3 as a register artifact; logical return void.
void WorldModule::BridgeActionsToTrafficModule(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficModuleInputBuffer,
    const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    (void)lpWorldModule;

    const BrnWorldIO::GameActionQueue* lpInQueue = lpWorldInput->GetGameActionQueue();
    // The traffic buffer models its game-action slot as opaque GameActionQueueStorage;
    // the real type is VariableEventQueue<13312,16>.
    CgsModule::VariableEventQueue<13312, 16>* lpOutQueue =
        reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(
            lpTrafficModuleInputBuffer->GetGameActionQueue());

    CGS_ASSERT(lpInQueue != 0, "lpInQueue");     // :111 (non-gating)
    CGS_ASSERT(lpOutQueue != 0, "lpOutQueue");   // :112 (non-gating)

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        switch (liType)
        {
        case 13:
        case 23:
        case 28:
        case 29:
        case 30:
        case 34:
        case 39:
        case 47:
        case 73:
        case 75:
        case 77:
        case 97:
        case 98:
        case 99:
        case 100:
        case 110:
        case 143:
        case 192:
        case 225:
        case 226:
        case 236:
        case 244:
            // Forward the event verbatim (same payload pointer, type id, size).
            lpOutQueue->AddEvent(lpEvent, liType, liSize);
            break;
        default:
            break;
        }
        liType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

// ============================================================================
// @ 0x827ABF40 -- WorldBridgeToEntityModules.cpp:86/:87. The RACE-CAR leg of the
// game-action fan-out, run by WorldModule::Update @0x827D63E8 in the pre-scene input
// staging block (BrnWorldModule.cpp:2275, which has been calling an inert
// WorldLinkStubs gate since the world-drive wave).
//
// ⭐ THIS BRIDGE IS THE ONLY PRODUCER OF THE RACE-CAR MODULE'S GAME-ACTION QUEUE in the
// whole image (0x8279D060, InputBuffer_PreScene::GetGameActionQueue, is called from here
// and nowhere else). RaceCarEntityModule::HandleGameActions @0x8230BE08 drains that queue,
// and its case 0 is HandleResetPlayerCarAction -- the record that places the player's car
// at a junkyard spawn. Until this bridge was real, that queue could not be non-empty.
//
// ⚠️ UNLIKE the physics/traffic/world-entity siblings above there is NO ALLOWLIST and no
// per-event walk: the console Appends the WHOLE queue (the mangled callee at 0x827ABFC4 is
// CgsModule::VariableEventQueue<13312,16>::Append<13312,16>(const VariableEventQueue&)),
// so the race-car module sees every game action the world saw. The second half appends the
// world input's audio-car-data-loaded queue into the race-car input's own.
// Both null tripwires are NON-gating (the X360 fires the assert and falls through); the
// X360 tail leaves Append's result in r3 as a register artifact, logical return void.
void WorldModule::BridgeActionsToRaceCarModule(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarModuleInputBuffer,
    const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    (void)lpWorldModule;   // X360 r3 -- never read by this bridge

    const BrnWorldIO::GameActionQueue* lpInQueue = lpWorldInput->GetGameActionQueue();
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene::GameActionQueue* lpOutQueue =
        lpRaceCarModuleInputBuffer->GetGameActionQueue();

    CGS_ASSERT(lpInQueue != 0, "lpInQueue");     // :86 (non-gating)
    CGS_ASSERT(lpOutQueue != 0, "lpOutQueue");   // :87 (non-gating)

    if (lpInQueue != 0 && lpOutQueue != 0)
    {
        lpOutQueue->Append(*lpInQueue);
    }

    // The audio-car-data-loaded queue leg (X360 sub_827A4040 -> sub_8279D308 ->
    // EventQueue<AudioCarDataLoadedEvent,16>::Append). RaceCarAudioStreamer::Update is the
    // consumer on the race-car side.
    const BrnWorldIO::UpdateInputBuffer::AudioCarLoadedDataQueue* lpInAudio =
        lpWorldInput->GetAudioCarDataLoadedQueue();
    BrnWorld::RaceCarEntityModuleIO::AudioCarLoadedDataQueue* lpOutAudio =
        lpRaceCarModuleInputBuffer->GetAudioCarLoadedDataQueue();
    if (lpInAudio != 0 && lpOutAudio != 0)
    {
        lpOutAudio->Append(*lpInAudio);
    }
}

// @ 0x827AC488 -- WorldBridgeToEntityModules.cpp:178. The WORLD-ENTITY leg of the
// game-action fan-out, run by WorldModule::Update @0x827D63E8 just before the
// post-physics spine: CLEAR the world-entity module's post-physics game-action queue
// (this destination is rebuilt every frame, unlike the physics/traffic ones), then
// forward every type-192 game action verbatim. Type 192 is the world/streamer action
// the world-entity module consumes in its post-physics update -- this is the last hop
// before the streamer's own request machine sees the frame's world actions.
// Both null tripwires are NON-gating; the X360 tail leaves the last GetNextEvent
// result in r3 as a register artifact (logical return type void).
void WorldModule::BridgeActionsToWorldModule(
    void* lpWorldModule,
    BrnWorld::WorldEntityIO::InputBuffer_PostPhysics* lpWorldEntityInputBuffer_PostPhysics,
    const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    (void)lpWorldModule;   // X360 r3 -- never read by this bridge

    const BrnWorldIO::GameActionQueue* lpInQueue = lpWorldInput->GetGameActionQueue();
    BrnWorld::WorldEntityIO::InputBuffer_PostPhysics::GameActionQueue* lpOutQueue =
        lpWorldEntityInputBuffer_PostPhysics->GetGameActionQueue();

    CGS_ASSERT(lpInQueue != 0, "lpInQueue");     // :178 (non-gating)
    CGS_ASSERT(lpOutQueue != 0, "lpOutQueue");   // :179 (non-gating)

    lpOutQueue->Clear();

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        if (liType == 192)
        {
            lpOutQueue->AddEvent(lpEvent, 192, liSize);
        }
        liType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}
