#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy (InputBuffer_PostScene::SetActiveRaceCarOutputInterface)

// BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene::GetTrafficAIInterface() (non-const, write)
//   @ 0x827111C0. Reconstructed from BURNOUT_X360_ARTIST.XEX. Asserts the buffer is locked for
// writing (status bit 3, IsBufferLockedForWriting()) then returns the embedded AI interface
// (the X360 returns `a1 + 16416`, i.e. &this->mTrafficAIInterface). The producer
// (ConvertSceneResultsToTrafficDataForAI @ 0x82728518) calls this, reads the entity count at the
// interface's offset 0 and memcpy's 176-byte TrafficAIEntity records onto the active list.

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // X360 0x827111C0: write-lock; return this + 16416.
    TrafficAIInterface* OutputBuffer_PostScene::GetTrafficAIInterface()
    {
        // The AI-interface offset is load-bearing for this getter (the X360 returns this+16416).
        // Pinned from inside the member so offsetof can see the private members. The coarse-query
        // queue precedes it: the X360 places the queue at status+4 (its status word is 4 bytes vs
        // our 1-byte IOBuffer FlagSet), but the 16-aligned mTrafficAIInterface still lands at 16416.
        static_assert(offsetof(OutputBuffer_PostScene, mTrafficAIInterface) == 16416,
                      "mTrafficAIInterface @16416");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTrafficAIInterface;
    }

    // ========================================================================
    // OutputBuffer_PostPhysics handle accessors (reconstructed from BURNOUT_X360_ARTIST.XEX).
    // Each tests the lock state then returns the member's pinned address. The return offsets
    // (+8 / +834784 / +834828) are the load-bearing facts -- pinned here so offsetof can see the
    // private members.
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics, mInterfaceAt8)      == 8,      "mInterfaceAt8 @8");
        static_assert(offsetof(OutputBuffer_PostPhysics, mInterfaceAt834784) == 834784, "mInterfaceAt834784 @834784");
        static_assert(offsetof(OutputBuffer_PostPhysics, mInterfaceAt834828) == 834828, "mInterfaceAt834828 @834828");
    }

    // X360 0x82711A48 (asm-line 392): write-lock; return this + 8.
    OutputBuffer_PostPhysics::InterfaceAt8Storage* OutputBuffer_PostPhysics::GetWriteInterfaceAt8()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInterfaceAt8;
    }

    // X360 0x82711D90 (asm-line 407): write-lock; return this + 834784.
    OutputBuffer_PostPhysics::InterfaceAt834784Storage* OutputBuffer_PostPhysics::GetWriteInterfaceAt834784()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInterfaceAt834784;
    }

    // X360 0x827A0E18 (asm-line 418): read-lock; return this + 834828.
    const OutputBuffer_PostPhysics::InterfaceAt834828Storage* OutputBuffer_PostPhysics::GetReadInterfaceAt834828() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mInterfaceAt834828;
    }

    // X360 0x82712030 (asm-line 419): write-lock; return this + 834828.
    OutputBuffer_PostPhysics::InterfaceAt834828Storage* OutputBuffer_PostPhysics::GetWriteInterfaceAt834828()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInterfaceAt834828;
    }

    // ========================================================================
    // InputBuffer_PreScene (full DWARF layout; batch 0x82710B30 / 0x8279FAD8 /
    // 0x8279FBE8 / 0x8279FCA0 / 0x827ACD28). The setters reverse the X360 XMemCpy /
    // field-copy into the embedded members' operator=; the getter returns &member.
    // ========================================================================
    void InputBuffer_PreScene::_AssertLayout()
    {
        // Only offsets of pointer-free / pre-pointer members are safely assertable on the 64-bit
        // host. mTimerStatusInterface (@4) and mActiveRaceCarOutputInterface (@64) precede any host-
        // widening pointer payload, so they hold. mGlobalRaceCarOutputInterface (@10544) and
        // mTrafficNetworkInputInterface (@12960) sit AFTER RCEntityActiveRaceCarOutputInterface's
        // ResourceHandle/WorldMap2D pointers, which widen to 8 bytes on the LLP64 gate host and push
        // those console offsets off -- so they are NOT asserted here (they are pinned to the X360 XEX
        // by the XMemCpy(0x28F0)/XMemCpy(0x970) sizes the setters reverse). Per the layout rule: drop
        // the drifting assert rather than repad the 32-bit interior.
        static_assert(offsetof(InputBuffer_PreScene, mTimerStatusInterface)         == 4,  "mTimerStatusInterface @4");
        static_assert(offsetof(InputBuffer_PreScene, mActiveRaceCarOutputInterface) == 64, "mActiveRaceCarOutputInterface @64");
    }

    // X360 0x82710B30: GetTimerStatusInterface() const. Read-lock (status bit 4,
    // IsBufferLockedForReading -- the X360 tests `extrwi r11,r11,1,27`), then return
    // &mTimerStatusInterface (the X360 tail `addi r3,this,4` == this+4). DWARF spells the
    // full name GetTimerStatusInterface returning const CgsSystem::TimerStatusInterface*
    // (BrnTrafficEntityModuleIO.h:150); the dossier's 'GetTimerS' is a truncated Hex-Rays symbol.
    const CgsSystem::TimerStatusInterface* InputBuffer_PreScene::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTimerStatusInterface;
    }

    // X360 0x8279FAD8: SetTimerStatusInterface. Write-lock (status bit 3), then copy the source
    // timer status into mTimerStatusInterface (this+4). The X360 body copies field-for-field in two
    // 6-field blocks (word@0, float@4, float@8, byte@0xC, word@0x10, float@0x14 -- at base and
    // base+0x18): that is CgsSystem::TimerStatusInterface::operator= (two CgsSystem::TimerStatus,
    // 24B each, 48B total), NOT a raw 48-byte memcpy (which would drag the bool's trailing pad).
    // DWARF :151 (param const CgsSystem::TimerStatusInterface*).
    void InputBuffer_PreScene::SetTimerStatusInterface(
        const CgsSystem::TimerStatusInterface* lpTimerStatusInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mTimerStatusInterface = *lpTimerStatusInterface;
    }

    // X360 0x8279FBE8: SetActiveRaceCarOutputInterface. Write-lock (status bit 3), then
    // XMemCpy the 10480-byte race-car output payload into mActiveRaceCarOutputInterface (this+0x40).
    // The X360 XMemCpy(this+0x40, src, 0x28F0) is the flat member copy == the committed
    // RCEntityActiveRaceCarOutputInterface::operator= (mirrors UpdateOutputBuffer::
    // SetActiveRaceCarOutputInterface). DWARF :154; element type RCEntityActiveRaceCarOutputInterface.
    void InputBuffer_PreScene::SetActiveRaceCarOutputInterface(
        const ActiveRaceCarOutputInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mActiveRaceCarOutputInterface = *lpInterface;
    }

    // X360 0x8279FCA0: SetGlobalRaceCarOutputInterface. Write-lock (status bit 3), then
    // XMemCpy the 2416-byte global race-car output payload into mGlobalRaceCarOutputInterface
    // (this+0x2930). XMemCpy(this+0x2930, src, 0x970) == the flat member copy modelled as the
    // committed RCEntityGlobalRaceCarOutputInterface::operator=. DWARF :157.
    void InputBuffer_PreScene::SetGlobalRaceCarOutputInterface(
        const GlobalRaceCarOutputInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mGlobalRaceCarOutputInterface = *lpInterface;
    }

    // X360 0x827ACD28: SetTrafficNetworkInputInterface. Write-lock (status bit 3). r31 = this+0x32A0
    // (&mTrafficNetworkInputInterface, whose mActivateHullQueue is at offset 0). `stw 0,8(r31)`
    // zeroes the queue's miLength (BaseEventQueue::miLength @+8) == Clear(); then Append merges the
    // source queue; then `lbz/stb 0x6C` copies mbDiverged (@ +0x6C, right after the 108-byte
    // EventQueue<ActivateHullEvent,8>). DWARF :160. SetTrafficNetworkInputInterface is a member of
    // InputBuffer_PreScene (not of the interface), so it reaches the queue via the const accessor +
    // const_cast, matching the inlined X360 direct-member touches.
    void InputBuffer_PreScene::SetTrafficNetworkInputInterface(
        const TrafficNetworkInputInterface* lpTrafficNetworkInputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        TrafficNetworkInputInterface::ActivateHullQueue& lrQueue =
            const_cast<TrafficNetworkInputInterface::ActivateHullQueue&>(
                mTrafficNetworkInputInterface.GetActivateHullQueue());
        lrQueue.Clear();
        lrQueue.Append(lpTrafficNetworkInputInterface->GetActivateHullQueue());
        mTrafficNetworkInputInterface.SetDiverged(lpTrafficNetworkInputInterface->HasDiverged());
    }

    // ========================================================================
    // InputBuffer_PostScene (batch 0x8279FEA8 / 0x827ACDE8). SetActiveRaceCarOutputInterface
    // block-copies the embedded race-car output interface; SetCrashTrafficOutputInterface
    // clear-then-appends each of the crash-traffic interface's two event queues.
    // NOTE: these two verifier-authoritative bodies KEEP the rodata's trailing "\n" in the lock
    // message (the X360 rodata string aNotLockedForWr is "Not locked for writing\n"); this differs
    // from the no-\n convention the other buffers in this file use.
    // ========================================================================

    // BrnTraffic::BrnTrafficIO::InputBuffer_PostScene::SetActiveRaceCarOutputInterface @ 0x8279FEA8.
    // Write-lock (status bit 3), then block-copies the source active-race-car output interface into
    // the embedded member (X360 XMemCpy(this+0x670, src, 0x28F0) == a 10480-byte copy into
    // &mActiveRaceCarOutputInterface). Producer: WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene.
    void InputBuffer_PostScene::SetActiveRaceCarOutputInterface(
            const InputBuffer_PostScene::ActiveRaceCarOutputInterface* lpInterface)
    {
        // The 1648 offset is load-bearing (the X360 memcpy dest is this+0x670); pinned so offsetof
        // can see the private member. sizeof(RCEntityActiveRaceCarOutputInterface) == 0x28F0 == 10480.
        static_assert(offsetof(InputBuffer_PostScene, mActiveRaceCarOutputInterface) == 1648,
                      "mActiveRaceCarOutputInterface @1648");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(&mActiveRaceCarOutputInterface, lpInterface,
                    sizeof(InputBuffer_PostScene::ActiveRaceCarOutputInterface));
    }

    // BrnTraffic::BrnTrafficIO::InputBuffer_PostScene::SetCrashTrafficOutputInterface @ 0x827ACDE8.
    // Write-lock (status bit 3), then refreshes the embedded crash-traffic output interface from the
    // source: for EACH of its two event queues the X360 zeroes miLength (@+8, i.e. Clear()) then
    // Appends the source queue's live events. queue1 = mCleanupTrafficEventQueue (this+8), queue2 =
    // mStartCrashingNetworkTrafficQueue (this+0x518). Producer: WorldModule::BridgeCrashModuleToTrafficModule_PostScene.
    void InputBuffer_PostScene::SetCrashTrafficOutputInterface(
            const InputBuffer_PostScene::CrashTrafficOutputInterface* lpInterface)
    {
        // mCrashTrafficOutputInterface is the first buffer member (X360 places it at this+8, right
        // after the 1-byte IOBuffer status). Pinned so the +8 base of both queues is exact.
        static_assert(offsetof(InputBuffer_PostScene, mCrashTrafficOutputInterface) == 8,
                      "mCrashTrafficOutputInterface @8");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        // Cleanup-traffic queue: clear then append the source's live events (X360 stw 0,miLength +
        // BaseEventQueue<CleanupTrafficEvent>::Append @0x827A8490).
        mCrashTrafficOutputInterface.GetCleanupTrafficEventQueue().Clear();
        mCrashTrafficOutputInterface.GetCleanupTrafficEventQueue().Append(
            lpInterface->GetCleanupTrafficEventQueue());

        // Start-crashing network-traffic queue: clear then append (X360 stw 0,miLength +
        // BaseEventQueue<NetworkTrafficCrashingEvent>::Append @0x827A8570).
        mCrashTrafficOutputInterface.GetStartCrashingNetworkTrafficQueue().Clear();
        mCrashTrafficOutputInterface.GetStartCrashingNetworkTrafficQueue().Append(
            lpInterface->GetStartCrashingNetworkTrafficQueue());
    }

    // ========================================================================
    // InputBuffer_PrePhysics (batch 0x827A9DE0 / 0x827A9E98 / 0x827A0158). The two queue setters
    // clear-then-append the source queue; the player-reset setter whole-struct copies the 32-byte
    // interface. Assert message drops the rodata's trailing \n per this file's buffer convention.
    // ========================================================================
    void InputBuffer_PrePhysics::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PrePhysics, mPotentialContactQueue) == 16,     "mPotentialContactQueue @16");
        static_assert(offsetof(InputBuffer_PrePhysics, mOverlapPairsQueue)     == 163872, "mOverlapPairsQueue @163872");
        static_assert(offsetof(InputBuffer_PrePhysics, mPlayerResetInterface)  == 199744, "mPlayerResetInterface @199744");
    }

    // X360 0x827A9DE0: write-lock; reset the queue then merge the source potential-contact queue
    // onto it. The X360 stores 0 to miLength (&mPotentialContactQueue+8) then forwards to
    // BaseEventQueue<PotentialContact>::Append.
    void InputBuffer_PrePhysics::SetPotentialContactQueue(const PotentialContactQueue* lpPotentialContactQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mPotentialContactQueue.Clear();             // stw r28,8(&mPotentialContactQueue) -> miLength = 0
        mPotentialContactQueue.Append(*lpPotentialContactQueue);
    }

    // X360 0x827A9E98: write-lock; reset the queue then merge the source overlap-pair queue
    // onto it. The X360 stores 0 to miLength (&mOverlapPairsQueue+8) then forwards to
    // BaseEventQueue<OutOverlapPair>::Append (committed instantiation @0x827A6FE8, 24B stride).
    void InputBuffer_PrePhysics::SetOverlapPairsQueue(const OverlapPairsQueue* lpOverlapPairsQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mOverlapPairsQueue.Clear();                 // stw r28,8(&mOverlapPairsQueue) -> miLength = 0
        mOverlapPairsQueue.Append(*lpOverlapPairsQueue);
    }

    // X360 0x827A0158: write-lock; copy the 32-byte race-car player-reset interface into the
    // buffer (the X360 does 4 QWORD load/store pairs -> a whole-struct assignment; the struct is
    // 32B: alignas(16) Vector3 mRestPos + bool, padded up).
    void InputBuffer_PrePhysics::SetPlayerResetInterface(const RCEntityPlayerResetInterface* lpPlayerResetInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mPlayerResetInterface = *lpPlayerResetInterface;
    }
}
}
