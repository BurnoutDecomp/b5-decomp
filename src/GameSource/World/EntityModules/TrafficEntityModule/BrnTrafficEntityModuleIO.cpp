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
    // Each tests the lock state (read = bit 4, write = bit 3) then returns the member's address
    // BY NAME. No host byte offset is pinned: several members are queue aggregates whose host
    // header is 16 bytes where the console's is 12, so host offsets legitimately exceed the
    // console ones (see the header's FLAG). The rodata lock messages carry the verbatim "\n".
    //
    // ⭐ RELAID OUT 2026-08-19 (wave Q6 cluster C3) from Construct @0x82761908 -- see the header
    // for the full store-for-store roll-call and for what the old +834784 scene seat was.
    // ========================================================================
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        // Parity is by NAMED MEMBER + SEQUENCE, so no offsetof pin is possible here. What IS
        // pinnable is the fact that made the old layout wrong: the scene seat has to be able to
        // hold the whole 25-queue aggregate, not a 44-byte span.
        static_assert(sizeof(OutputBuffer_PostPhysics::SceneInputInterface) > 800000,
                      "mSceneInputInterface is the real InSceneUpdateInterface (console 818768 B "
                      "at +11376), not the 44-byte span the +834784 accessor returns");
        static_assert(sizeof(OutputBuffer_PostPhysics::InterfaceAt834784) == 44,
                      "the X360-only member at console +834784 is 44 bytes (Construct zeroes 11 words)");
    }

    // X360 0x82761908 -- run by CreateIOBuffer<OutputBuffer_PostPhysics> @0x827B79D0 (which
    // allocates 0xD3D20 == 867,616 bytes). Store for store -- ⭐ TRUE ONLY AS OF THE Q6
    // ROUND-1 FIX below: the three members with no Construct call of their own
    // (mTrafficSoundOutputInterface's leading halfword, the array count inside
    // mTrafficDirectorOutputInterface, and the whole 44-byte mInterfaceAt834784) reach the
    // console as raw zero stores, and all three were missing from this body while the banner
    // already claimed store-for-store parity.
    //
    // RECOVERED THIS CLUSTER: 0x82761908 has no per-address JSON under .ida-exports/ (an
    // export-run gap, not a missing function -- AGENTS gotcha 6); dumped by a targeted headless
    // idat run on a PRIVATE .i64 copy, scratchpad/waveQ6/ida_bridges/.
    void OutputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();               // stb 1, 0(this)
        mCrashTrafficInputInterface.Construct();        // +8
        mNetworkInterface.Construct();                  // +0xDA0  (3488)

        // ⭐ Q6 FIX (round-1 verifier, bridges #2): THREE console legs were dropped by the
        // "store for store" claim above. CreateIOBuffer stopped zero-filling on 2026-08-15,
        // so without these the three regions keep whatever the IO stack's previous tenant
        // left. All three are re-measured off 0x82761908 (scratchpad/waveQ6/ida_bridges).
        //
        //   0x82761938  sth r29,0xE30(r31)   -- +3632 == mTrafficSoundOutputInterface's
        //                                       leading mu16EntityCount. A halfword store of
        //                                       the count only; the 32 entity records are NOT
        //                                       cleared.
        mTrafficSoundOutputInterface.mu16EntityCount = 0;
        //   0x8276193C  stw r29,0x2650(r31)  -- +9808 == mTrafficDirectorOutputInterface
        //                                       (+6208) + 16 (its array) + 32*112 (its
        //                                       records) == the Array<TrafficDirectorEntity,32>
        //                                       miCount word, i.e. the INLINED
        //                                       maActiveEntityArray.Construct(). The interface's
        //                                       own mu16EntityCount (+0) is NOT stored.
        mTrafficDirectorOutputInterface.GetTrafficDirectorEntityArray().Construct();

        mGameEventQueue.Construct();                    // +0x2660 (9824)
        mSceneInputInterface.Construct();               // +0x2C70 (11376)  ⭐
        mResourceRequestInterface.Construct();          // +0xCAB90 (830672)
        mResourceRequestInterface.Clear();              // the console's paired Clear

        //   0x82761964-0x8276197C  addis r11,r31,0xD / addi r11,r11,-0x4320 / mtctr 0xB /
        //                          stw r29,0(r11) / addi r11,r11,4 / bdnz
        //                                    -- 11 words == the whole 44-byte
        //                                       mInterfaceAt834784 at +834784, zeroed by an
        //                                       unrolled-to-a-loop store run. The console runs
        //                                       it HERE, between the mResourceRequestInterface
        //                                       leg and the mTrafficTypeResponseQueue leg.
        mInterfaceAt834784 = InterfaceAt834784();

        mTrafficTypeResponseQueue.Construct();          // +0xCAAC0 (830144)
        mGuiEventQueue.Construct();                     // +0xCFD0C (834828)
    }

    // X360 0x827A0830 (:387): read-lock; return &mCrashTrafficInputInterface (this + 8).
    const OutputBuffer_PostPhysics::CrashTrafficInputInterface* OutputBuffer_PostPhysics::GetCrashTrafficInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mCrashTrafficInputInterface;
    }

    // X360 0x82711A48 (:392): write-lock; return &mCrashTrafficInputInterface (this + 8).
    OutputBuffer_PostPhysics::CrashTrafficInputInterface* OutputBuffer_PostPhysics::GetCrashTrafficInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mCrashTrafficInputInterface;
    }

    // X360 0x827A0980 (:391, wave35 reconciliation -- const read overload of the +3632 member):
    // read-lock; return &mTrafficSoundOutputInterface (this + 3632).
    const TrafficSoundOutputInterface* OutputBuffer_PostPhysics::GetTrafficSoundOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficSoundOutputInterface;
    }

    // X360 0x82711B98 (:394): write-lock; return &mTrafficSoundOutputInterface (this + 3632).
    // Producer: BrnTraffic::TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults.
    TrafficSoundOutputInterface* OutputBuffer_PostPhysics::GetTrafficSoundOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTrafficSoundOutputInterface;
    }

    // X360 0x827A08D8 (baked 394): read-lock; return &mNetworkInterface (this + 3488).
    const TrafficNetworkOutputInterface* OutputBuffer_PostPhysics::GetNetworkInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mNetworkInterface;
    }

    // X360 0x827A0A28 (baked 400): read-lock; return &mTrafficDirectorOutputInterface (this + 6208).
    // ⚠ RE-HOMED 2026-08-19 (wave Q6/C3): this accessor used to be modelled as the const read of
    // mGameEventQueue. Its epilogue is `addi r3,r28,0x1840` == +6208, which is the director
    // interface's seat (Construct's `stw 0,0x2650` lands inside its span); the game-event queue's
    // read accessor is 0x827A0AD0 (baked 403) at +0x2660 == 9824.
    const TrafficDirectorOutputInterface* OutputBuffer_PostPhysics::GetTrafficDirectorOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficDirectorOutputInterface;
    }

    // X360 0x827A0AD0 (baked 403): read-lock; return &mGameEventQueue (this + 9824).
    const OutputBuffer_PostPhysics::GameEventQueue* OutputBuffer_PostPhysics::GetGameEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGameEventQueue;
    }

    // X360 0x82711CE8 (baked 404): write-lock; return &mGameEventQueue (this + 9824).
    // Producers: BrnTraffic::TrafficEntityModule::HandleExternalRequests / ::PostPhysicsUpdate.
    OutputBuffer_PostPhysics::GameEventQueue* OutputBuffer_PostPhysics::GetGameEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGameEventQueue;
    }

    // X360 0x827A0B78 (baked 406): read-lock; return the X360-only 44-byte member (this + 834784).
    const OutputBuffer_PostPhysics::InterfaceAt834784* OutputBuffer_PostPhysics::GetReadInterfaceAt834784() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mInterfaceAt834784;
    }

    // X360 0x82711D90 (baked 407): write-lock; same member (this + 834784).
    OutputBuffer_PostPhysics::InterfaceAt834784* OutputBuffer_PostPhysics::GetWriteInterfaceAt834784()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mInterfaceAt834784;
    }

    // ⭐ X360 0x827A0C20 (baked 409): read-lock; return &mSceneInputInterface (this + 11376).
    // The leg WorldModule::BridgeEntityModulesToScene_PostPhysics @0x827AB608 calls first
    // (0x827AB6C0). Construct @0x82761908 builds this member with
    // InSceneUpdateInterface::Construct(this+0x2C70), and 11376 + 818768 == 830144 == the next
    // member -- the two witnesses that put the aggregate here rather than at the 44-byte +834784
    // seat the old model used.
    const OutputBuffer_PostPhysics::SceneInputInterface* OutputBuffer_PostPhysics::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSceneInputInterface;
    }

    // X360 0x827A0CC8 (baked 412): read-lock; return &mTrafficTypeResponseQueue (this + 830144).
    const OutputBuffer_PostPhysics::TrafficTypeResponseQueue* OutputBuffer_PostPhysics::GetTrafficTypeResponseQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficTypeResponseQueue;
    }

    // X360 0x827A0D70 (baked 415): read-lock; return &mResourceRequestInterface (this + 830672).
    const OutputBuffer_PostPhysics::ResourceRequestInterface* OutputBuffer_PostPhysics::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mResourceRequestInterface;
    }

    // X360 0x827A0E18 (baked 418): read-lock; return &mGuiEventQueue (this + 834828).
    const OutputBuffer_PostPhysics::GuiEventInputQueue* OutputBuffer_PostPhysics::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGuiEventQueue;
    }

    // X360 0x82712030 (baked 419): write-lock; return &mGuiEventQueue (this + 834828).
    OutputBuffer_PostPhysics::GuiEventInputQueue* OutputBuffer_PostPhysics::GetGuiEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGuiEventQueue;
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
    // X360 0x827615F8 (DWARF :274), store for store:
    //   stb 1,0(this)                                   IOBuffer::Construct (status = constructed)
    //   EventQueue<PotentialContact,2048>::Construct     this+0x10     mPotentialContactQueue
    //   EventQueue<OutOverlapPair,128>::Construct        this+0x28020  mOverlapPairsQueue
    //   VariableEventQueue<32768,16>::Construct          this+0x28C30  mSceneResultQueue
    //   stvx128 v0(zero) ; stb 0,0x10                    this+0x30C40  mPlayerResetInterface = {0, false}
    //   TrafficLightKnockDownEvent<32>::Construct        this+0x30C60  mPropToTrafficInterface queue 0
    //   TrafficLightRestoreEvent<80>::Construct          this+0x30CEC  mPropToTrafficInterface queue 1
    //
    // ⭐⭐ COMPLETED 2026-08-19 (wave Q6 cluster C3). The 2026-08-19 (wave Q5 round-3) landing
    // ran only three of the six legs, because mSceneResultQueue and mPropToTrafficInterface were
    // opaque byte spans that no expression could reach; both are their real types now
    // (see the header), so all six legs are here in the console's order.
    //
    // ⚠️ THIS IS THE gotcha-14 CASE, TWICE OVER, AND IT IS NOT THEORETICAL: the last two legs
    // are the two rings WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70
    // Clear()s and Append()s every pre-physics frame from this wave on. Without them
    // mpEvents stays at whatever the IO stack's previous tenant left and the first smashed
    // traffic light writes events through a foreign pointer. The mSceneResultQueue leg is the
    // same trap one member earlier (a never-Constructed VariableEventQueue asserts
    // "Not Constructed" on its first AddEvent, CgsVariableEventQueue.h).
    void InputBuffer_PrePhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();              // stb 1, 0(this)
        mPotentialContactQueue.Construct();            // +0x10
        mOverlapPairsQueue.Construct();                // +0x28020
        mSceneResultQueue.Construct();                 // +0x28C30
        mPlayerResetInterface.Clear();                 // the 16-byte zero splat + the bool byte
        mPropToTrafficInterface.Construct();           // +0x30C60 (knock-down) + +0x30CEC (restore)
    }

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

    // X360 0x827113B8 (:289): read-lock; return &mPropToTrafficInterface (this+199776).
    const InputBuffer_PrePhysics::PropToTrafficInterface* InputBuffer_PrePhysics::GetPropToTrafficInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropToTrafficInterface;
    }

    // X360 0x827A02D0 (:290): write-lock; return &mPropToTrafficInterface (this+199776).
    InputBuffer_PrePhysics::PropToTrafficInterface* InputBuffer_PrePhysics::GetPropToTrafficInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropToTrafficInterface;
    }

    // ========================================================================
    // OutputBuffer_PostScene extra accessors (wave35): the non-const scene coarse-query queue
    // (0x82711118) and the const traffic-AI interface (0x827A0008). The committed non-const
    // GetTrafficAIInterface / Construct live above; these grow the same buffer.
    // ========================================================================

    // X360 0x82711118: write-lock; return &mSceneCoarseQueryQueue (this + 4, first member).
    OutputBuffer_PostScene::SceneCoarseQueryQueue* OutputBuffer_PostScene::GetSceneCoarseQueryQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mSceneCoarseQueryQueue;
    }

    // X360 0x827A0008 (:255): read-lock; return &mTrafficAIInterface (this + 16416).
    const TrafficAIInterface* OutputBuffer_PostScene::GetTrafficAIInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficAIInterface;
    }

    // ========================================================================
    // OutputBuffer_PreScene accessors.
    //
    // ⭐ RELAID OUT 2026-08-19 (wave Q6 cluster C3) from Construct @0x82761790 + the recovered
    // read-lock ladder (baked lines 186 -> +16, 189 -> +818784, 192 -> +819328, a uniform +4 skew
    // off the DecFIGS declaration lines :182/:185/:188). See the header for what was wrong: the
    // +63424 accessor 0x827A00B0 bakes line 262 and belongs to OutputBuffer_PostScene, and the
    // +818784 pair was homed one member too early.
    // ========================================================================
    void OutputBuffer_PreScene::_AssertLayout()
    {
        // The two console offsets that ARE reproducible on the host: the scene interface is the
        // first member after the status pad (console +16), and the traffic->race-car block is the
        // 544-byte span BrnWorldModule.cpp's pre-scene snapshot copies.
        static_assert(offsetof(OutputBuffer_PreScene, mSceneInputInterface) == 16,
                      "mSceneInputInterface @16 (console; Construct calls "
                      "InSceneUpdateInterface::Construct(this+0x10))");
        static_assert(sizeof(OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene) == 544,
                      "the traffic->race-car pre-scene block is console [818784, 819328) == 544 B");
        // Everything after mSceneInputInterface drifts: the aggregate's 25 queues carry an 8-byte
        // mpEvents on the host where the console has 4, so its host sizeof EXCEEDS the console's
        // 818768. Parity from here down is by NAMED MEMBER + SEQUENCE (the same disposition
        // PhysicsModuleIO::OutputBuffer's scene seat took on 2026-08-19).
        static_assert(sizeof(OutputBuffer_PreScene::SceneInputInterface) >= 818768,
                      "the host scene aggregate must cover the console span [16, 818784)");
    }

    // X360 0x82761790 -- run by CreateIOBuffer<OutputBuffer_PreScene> @0x827B6330. Store for
    // store: status byte; InSceneUpdateInterface::Construct(this+0x10); the 544-byte
    // traffic->race-car block zeroed (7 doublewords + four words + four flt_82001CC0 floats);
    // VariableEventQueue<131072,16>::Construct(this+0xC8080) and
    // EventQueue<InRemoveTriggerEvent,256>::Construct(this+0xE8090) -- i.e. the two queues of
    // mTriggerManagementInputInterface, in member order; then the count word of mPotentialScorees
    // (console +951072), which this slice does not model.
    //
    // ⭐ Q6 ROUND-1 FIX (bridges #1): the 544-byte block used to be value-initialised as an
    // opaque whole, on the (false) grounds that "the member has no home in the tree". It does --
    // BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene, in this subsystem's own
    // SharedIO -- so the leg is now that type's own Construct(), which spells the console's zero
    // pattern FIELD BY FIELD (7 doublewords of mSympatheticCrashers; the two near-miss Array
    // counts; miPotentialStompeeCount; muNearbyStaticVehicleCount; the four flt_82001CC0 floats)
    // instead of blanket-zeroing bytes the console leaves alone (mPotentialStompees[8]).
    void OutputBuffer_PreScene::Construct()
    {
        CgsModule::IOBuffer::Construct();                            // stb 1, 0(this)
        mSceneInputInterface.Construct();                            // +0x10
        mTrafficToRaceCarInterface_PreScene.Construct();             // the +0xC7E60 zero run
        mTriggerManagementInputInterface.GetAddTriggerEventQueue().Construct();     // +0xC8080
        mTriggerManagementInputInterface.GetRemoveTriggerEventQueue().Construct();  // +0xE8090
    }

    // ⭐ X360 0x8279FD58 (baked 186, DWARF :182): read-lock; return &mSceneInputInterface
    // (this+16). The leg WorldModule::BridgeEntityModulesToSceneModule_PreScene @0x827AB490 calls
    // first (0x827AB570).
    const OutputBuffer_PreScene::SceneInputInterface*
    OutputBuffer_PreScene::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSceneInputInterface;
    }

    // X360 0x827BB090 (baked 189, DWARF :185): read-lock; return
    // &mTrafficToRaceCarInterface_PreScene (this+818784). Caller: WorldModule::Update's pre-scene
    // snapshot (BrnWorldModule.cpp:2519), which copies the whole 544-byte block.
    const OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*
    OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficToRaceCarInterface_PreScene;
    }

    // X360 0x82710DD0 (baked 190, DWARF :186): write-lock; same member (this+818784).
    OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*
    OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTrafficToRaceCarInterface_PreScene;
    }

    // X360 0x8279FE00 (baked 192, DWARF :188): read-lock; return
    // &mTriggerManagementInputInterface (this+819328).
    const OutputBuffer_PreScene::TriggerManagementInputInterface*
    OutputBuffer_PreScene::GetTriggerManagementInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriggerManagementInputInterface;
    }

    // X360 0x82710E78 (baked 193, DWARF :189): write-lock; same member (this+819328).
    OutputBuffer_PreScene::TriggerManagementInputInterface*
    OutputBuffer_PreScene::GetTriggerManagementInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTriggerManagementInputInterface;
    }

    // ========================================================================
    // OutputBuffer_Prepare accessors (wave35 new home).
    // ========================================================================
    void OutputBuffer_Prepare::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_Prepare, mSceneInputInterface) == 16,
                      "mSceneInputInterface @16 (console; Construct @0x82761740 calls "
                      "InSceneUpdateInterface::Construct(this+0x10))");
        // ⛔ THE `offsetof(mResourceRequestInterface) == 818784` PIN IS DELETED, and that is the
        // CORRECT direction (wave Q6/C3). It pinned a CONSOLE byte offset on a host layout whose
        // preceding member is now the real 25-queue aggregate -- whose host sizeof exceeds the
        // console's 818,768 because each queue carries an 8-byte mpEvents. Parity from here down
        // is by NAMED MEMBER + SEQUENCE (the standing x64 rule). What survives is the relation
        // that actually matters:
        static_assert(sizeof(OutputBuffer_Prepare::SceneInputInterface) >= 818768,
                      "the host scene aggregate must cover the console span [16, 818784)");
    }

    // X360 0x82761740 -- run by CreateIOBuffer<OutputBuffer_Prepare> @0x827B5C70. Store for
    // store: status byte; InSceneUpdateInterface::Construct(this+0x10);
    // VariableEventQueue<4096,16>::Construct(this+0xC7E60) then ::Clear on the same address --
    // i.e. mResourceRequestInterface, Constructed then Cleared, exactly as the console does.
    void OutputBuffer_Prepare::Construct()
    {
        CgsModule::IOBuffer::Construct();      // stb 1, 0(this)
        mSceneInputInterface.Construct();      // +0x10
        mResourceRequestInterface.Construct(); // +0xC7E60 (818784)
        mResourceRequestInterface.Clear();     // the console's paired Clear
    }

    // X360 0x8279F988: read-lock; return &mSceneInputInterface (this + 16).
    const OutputBuffer_Prepare::SceneInputInterface* OutputBuffer_Prepare::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSceneInputInterface;
    }

    // X360 0x827109E0: write-lock; return &mSceneInputInterface (this + 16).
    OutputBuffer_Prepare::SceneInputInterface* OutputBuffer_Prepare::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mSceneInputInterface;
    }

    // X360 0x8279FA30 (:125): read-lock; return &mResourceRequestInterface (this + 818784).
    const OutputBuffer_Prepare::ResourceRequestInterface* OutputBuffer_Prepare::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mResourceRequestInterface;
    }

    // X360 0x82710A88 (:130): write-lock; return &mResourceRequestInterface (this + 818784).
    OutputBuffer_Prepare::ResourceRequestInterface* OutputBuffer_Prepare::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mResourceRequestInterface;
    }

    // ========================================================================
    // OutputBuffer_PrePhysics (wave35 new home; the three interfaces made REAL 2026-08-10 --
    // see the retype banner in the header for why the 1-byte slice was a memory bug and why
    // the console-offset static_assert had to go with it).
    // ========================================================================

    // ⭐ @0x827618A0 (DWARF :307) -- NEW 2026-08-10 (pre-physics bridge wave).
    // Was inherited base-only (`lpTrafficOutput_PrePhysics->Construct()` in WorldModule::Update
    // resolved to CgsModule::IOBuffer::Construct), which left every embedded queue's mpEvents
    // NULL. Harmless while nothing read the buffer; fatal the moment
    // BridgeEntityModulesToPhysicsModule_PrePhysics started merging all three interfaces --
    // the exact "mpEvents != NULL" + "Reached Max length" death the race-car pre-physics buffer
    // already suffered once (BrnVehicleInputInterface.h's Construct banner). 25 X360
    // instructions, read verbatim; every member reached BY NAME, not by the console offset the
    // comment records:
    //   0x827618B8  stb  1, 0(this)                          -- IOBuffer::Construct (status = 1)
    //   0x827618BC  bl   VehicleInputInterface::Construct     (this+16)
    //   0x827618CC  bl   CreateAirRamEvent<20>::Construct     (this+142192)  \  == the effects
    //   0x827618D4  bl   CreateSpinEvent<10>::Construct       (this+143488)  /     interface
    //   0x827618E4  stw  0, 8(this+142192) ; stw 0, 0x518(this+142192)  -- the two queue lengths
    //   0x826718EC  bl   VehicleDriverInputInterface::Construct(this+143984)
    //   0x827618F8  stbx 0, this, 0x24720                     -- mbPlayingShowtime = false
    // ⭐ The two explicit length stores are what VehicleEffectsInputInterface::Construct's two
    // queue Constructs already do (EventQueue::Construct writes mpEvents AND clears miLength);
    // the X360 emits them separately because it inlined both Constructs and the compiler kept
    // the redundant zeroing. Calling the named Construct reproduces the end state exactly.
    void OutputBuffer_PrePhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mVehicleInputInterface.Construct();
        mVehicleEffectsInterface.Construct();
        mVehicleDriverInterface.Construct();

        mbPlayingShowtime = false;
    }

    // X360 0x827A0378 (:310): read-lock; return &mVehicleInputInterface (this + 16).
    const OutputBuffer_PrePhysics::VehicleInputInterface* OutputBuffer_PrePhysics::GetVehicleInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleInputInterface;
    }

    // (:311): write-lock; return &mVehicleInputInterface (this + 16).
    OutputBuffer_PrePhysics::VehicleInputInterface* OutputBuffer_PrePhysics::GetVehicleInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleInputInterface;
    }

    // X360 0x827A0420 (:313): read-lock; return &mVehicleEffectsInterface (this + 142192).
    const OutputBuffer_PrePhysics::VehicleEffectsInputInterface* OutputBuffer_PrePhysics::GetVehicleEffectsInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleEffectsInterface;
    }

    // (:314): write-lock; return &mVehicleEffectsInterface (this + 142192).
    OutputBuffer_PrePhysics::VehicleEffectsInputInterface* OutputBuffer_PrePhysics::GetVehicleEffectsInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleEffectsInterface;
    }

    // X360 0x827A04C8 (:316): read-lock; return &mVehicleDriverInterface (this + 143984).
    const OutputBuffer_PrePhysics::VehicleDriverInputInterface* OutputBuffer_PrePhysics::GetVehicleDriverInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleDriverInterface;
    }

    // X360 0x82711508 (:317): write-lock; return &mVehicleDriverInterface (this + 143984).
    OutputBuffer_PrePhysics::VehicleDriverInputInterface* OutputBuffer_PrePhysics::GetVehicleDriverInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleDriverInterface;
    }
}
}
