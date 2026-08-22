#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnTraffic::BrnTrafficIO input-buffer GETTERS, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Companions to the committed setters (BrnTrafficEntityModuleIO_InputBuffer_PostPhysics.cpp) --
// each takes the address of its embedded member. Const (read) getters test the IOBuffer read-lock
// bit (status>>4 &1 == IsBufferLockedForReading(), `lbz r11,0(this); extrwi r11,r11,1,27`); the
// non-const GetGameActionQueue tests the write-lock bit (status>>3 &1 == IsBufferLockedForWriting(),
// `extrwi r11,r11,1,28`) -- reproducing WHICHEVER bit the asm tests. The streamed "Not locked for
// reading/writing" assert is the non-gating tripwire; the corpus drops the rodata trailing "\n"
// (matching the committed _PostPhysics / _Dispatch bodies). Base ptr is u8*, so each `addi/addis`
// tail is the true member byte offset.

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // FILE-SPLIT NOTE (2026-08-01, BridgeGameStateToWorld wave): InputBuffer_PostPhysics::Construct
    // lives HERE, not with its siblings in BrnTrafficEntityModuleIO_InputBuffer_PostPhysics.cpp,
    // because that TU is NOT MOUNTED -- MEASURED mount cost is 4 unresolved externals (the
    // VehicleOutputInterface / VehicleManagerOutputInterface / DeformationOutputInterface
    // ForEntityModules / RCEntityActiveRaceCarOutputInterface operator=s its five setters call).
    // Construct touches none of them. Re-home it when that TU is mounted.
    // ⛔ 2026-08-01 (BridgeGameStateToWorld wave): raise the IOBuffer status AND Construct the
    // embedded game-action queue. Before this the explicit ->Construct() calls in
    // WorldModule::Update / UpdateForBootUpVideo resolved to the base
    // CgsModule::IOBuffer::Construct (status byte only), so BridgeActionsToTrafficModule's
    // AddEvent hit an unconstructed VariableEventQueue<13312,16> -- measured as a pair of
    // "Not Constructed" asserts (CgsVariableEventQueue.h:454 then :728) on the first game action
    // the new GameState->World bridge delivered.
    void InputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mGameActionQueue.Construct();
        // The physics->traffic readback bridge assigns both interfaces (operator= Clears +
        // Appends every embedded EventQueue), so their queues must be Constructed here.
        mVehicleOutputInterface.Construct();
        mVehicleManagerOutputInterface.Construct();
    }


    // X360 0x82710F20 (:228) -- read-lock; return &mCrashTrafficOutputInterface (this+8).
    // Consumers: TrafficEntityModule::HandleCrashingNetworkTraffic / CleanUpCrashedVehicles.
    const InputBuffer_PostScene::CrashTrafficOutputInterface* InputBuffer_PostScene::GetCrashTrafficOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mCrashTrafficOutputInterface;
    }

    // X360 0x82711070 (:234) -- read-lock; return &mRaceCarToTrafficInterface (this+0x2F60 == 12128).
    // Consumer: TrafficEntityModule::PostSceneUpdate.
    const InputBuffer_PostScene::RaceCarToTrafficInterface* InputBuffer_PostScene::GetRaceCarToTrafficInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mRaceCarToTrafficInterface;
    }

    // X360 0x827115B0 (:346) -- read-lock; return &mVehicleOutputInterface (this+0x10 == 16).
    // Consumers: TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults / HandleExternalResponses.
    const InputBuffer_PostPhysics::VehicleOutputInterface* InputBuffer_PostPhysics::GetVehicleOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputInterface;
    }

    // X360 0x82711700 (:352) -- read-lock; return &mVehicleManagerOutputInterface (this+0xEC30 == 60464).
    // Consumers: TrafficEntityModule::HandleExternalResponses / HandleResetRaceCarEvents / PostPhysicsUpdate.
    const InputBuffer_PostPhysics::VehicleManagerOutputInterface* InputBuffer_PostPhysics::GetVehicleManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleManagerOutputInterface;
    }

    // X360 0x827117A8 (:355) -- read-lock; return &mGameActionQueue (this+0xF4B0 == 62640).
    // Consumer: TrafficEntityModule::HandleExternalRequests.
    const InputBuffer_PostPhysics::GameActionQueueStorage* InputBuffer_PostPhysics::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameActionQueue;
    }

    // X360 0x827A0618 (:356) -- write-lock; return &mGameActionQueue (this+0xF4B0 == 62640).
    // Producers: WorldModule::BridgeActionsToTrafficModule / UpdateForBootUpVideo.
    InputBuffer_PostPhysics::GameActionQueueStorage* InputBuffer_PostPhysics::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGameActionQueue;
    }
}
}
