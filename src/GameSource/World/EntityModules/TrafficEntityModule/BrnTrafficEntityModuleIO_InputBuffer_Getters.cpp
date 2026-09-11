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
    // FILE-SPLIT NOTE: InputBuffer_PostPhysics::Construct
    // lives HERE, not with its siblings in BrnTrafficEntityModuleIO_InputBuffer_PostPhysics.cpp,
    // because that TU is NOT MOUNTED -- MEASURED mount cost is 4 unresolved externals (the
    // VehicleOutputInterface / VehicleManagerOutputInterface / DeformationOutputInterface
    // ForEntityModules / RCEntityActiveRaceCarOutputInterface operator=s its five setters call).
    // Construct touches none of them. Re-home it when that TU is mounted.
    // Raise the IOBuffer status AND Construct the
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
        // The scene query-results fan-out Appends onto this seat every frame
        // (WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics), and Append
        // asserts the destination is Constructed.
        mSceneResultQueue.Construct();
        // The physics->traffic readback bridge assigns both interfaces (operator= Clears +
        // Appends every embedded EventQueue), so their queues must be Constructed here.
        mVehicleOutputInterface.Construct();
        mVehicleManagerOutputInterface.Construct();
        // The deformation-for-entity-modules seat: bridge leg 3 (LIVE 2026-09-02) assigns it
        // every frame and its operator= Clears+Appends the two embedded EventQueues, which
        // only Construct seats.
        mDeformationOutputInterfaceForEntityModules.Construct();
    }


    // InputBuffer_PostScene::Construct (:225), store for store. The console runs the race-car
    // interface's two queue Constructs and its two scalar stores FIRST (inlined
    // RaceCarToTrafficInterface::Construct on the seat at +12128), then the crash interface's
    // two queue Constructs (+8), then Clears the active-race-car interface (+1648). Without it
    // the buffer's four embedded queues keep whatever the IO stack's previous tenant left and
    // the first publish Clear+Appends onto a NULL mpEvents.
    void InputBuffer_PostScene::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mRaceCarToTrafficInterface.Construct();
        mCrashTrafficOutputInterface.Construct();
        mActiveRaceCarOutputInterface.Clear();
    }

    // InputBuffer_PostScene::SetRaceCarToTrafficInterface (:235) -- write-lock tripwire
    // ("Not locked for writing"), then publish the source interface onto the member seat
    // (+12128): Clear+Append on each of the two rival queues, then the flag word and the
    // showtime density scale. Those four stores live in RaceCarToTrafficInterface::operator=,
    // where the queues are members. Producer:
    // WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene.
    void InputBuffer_PostScene::SetRaceCarToTrafficInterface(const RaceCarToTrafficInterface* lpRaceCarToTrafficInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mRaceCarToTrafficInterface = *lpRaceCarToTrafficInterface;
    }

    // X360 0x82710F20 (:228) -- read-lock; return &mCrashTrafficOutputInterface (this+8).
    // Consumers: TrafficEntityModule::HandleCrashingNetworkTraffic / CleanUpCrashedVehicles.
    const InputBuffer_PostScene::CrashTrafficOutputInterface* InputBuffer_PostScene::GetCrashTrafficOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mCrashTrafficOutputInterface;
    }

    // (:231) -- read-lock; return &mActiveRaceCarOutputInterface (this+1648). Consumers:
    // TrafficEntityModule::PostNearbyTrafficSceneQueryRequest / AIPostSceneQueryRequests.
    const InputBuffer_PostScene::ActiveRaceCarOutputInterface* InputBuffer_PostScene::GetActiveRaceCarOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mActiveRaceCarOutputInterface;
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

    // Write-lock; return &mSceneResultQueue. Producer:
    // WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics, the second of its two
    // Appends of the same scene query-results ring.
    InputBuffer_PostPhysics::SceneResultQueue* InputBuffer_PostPhysics::GetSceneResultQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneResultQueue;
    }

    // The read-lock twin (the module's own drain side).
    const InputBuffer_PostPhysics::SceneResultQueue* InputBuffer_PostPhysics::GetSceneResultQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneResultQueue;
    }

    // X360 0x827118F8 (:365, IDA truncates the symbol to `BrnTraffic::BrnTraffi`) -- read-lock
    // (`lbz r11,0(this); extrwi 1,27`); return &mDeformationOutputInterfaceForEntityModules
    // (`addi r3, r31, 0x151B0` == 86448). Consumer: TrafficEntityModule::PostPhysicsUpdate
    // @0x8274E898, whose result goes straight into ProcessDeformationData's r4.
    const InputBuffer_PostPhysics::DeformationOutputInterfaceForEntityModules*
    InputBuffer_PostPhysics::GetDeformationOutputInterfaceForEntityModules() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mDeformationOutputInterfaceForEntityModules;
    }
}
}
