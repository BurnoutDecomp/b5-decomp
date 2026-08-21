#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy (InputBuffer_PostScene::SetActiveRaceCarOutputInterface)
#include <cstdlib>   // getenv  ([T1-rinfo] bring-up probe only)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint ([T1-rinfo] only)

// BrnTrafficIO IO-buffer bodies, from BURNOUT_X360_ARTIST.XEX. Each accessor tests its lock bit
// (read = bit 4, write = bit 3) and returns the member by name; the console offsets in the
// comments are provenance for which member, never arithmetic in the body.

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // X360 0x827111C0: write-lock; return this + 16416. Its producer
    // (ConvertSceneResultsToTrafficDataForAI @0x82728518) reads the entity count at the
    // interface's offset 0 and memcpy's 176-byte TrafficAIEntity records onto the active list.
    TrafficAIInterface* OutputBuffer_PostScene::GetTrafficAIInterface()
    {
        // The offset is load-bearing here, and pinned from inside the member so offsetof can see
        // the private ones. The coarse-query queue precedes it at status+4 (the console's status
        // word is 4 bytes to our 1-byte FlagSet), and the 16-aligned interface still lands at 16416.
        static_assert(offsetof(OutputBuffer_PostScene, mTrafficAIInterface) == 16416,
                      "mTrafficAIInterface @16416");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTrafficAIInterface;
    }

    // ========================================================================
    // OutputBuffer_PostPhysics accessors. No host byte offset is pinned: several members are
    // queue aggregates whose host header is 16 bytes where the console's is 12, so host offsets
    // legitimately exceed the console ones. The rodata lock messages carry the verbatim "\n".
    // ========================================================================
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        // Parity is by named member and sequence, so no offsetof pin is possible. What is
        // pinnable is that the scene seat must hold the whole 25-queue aggregate, not 44 bytes.
        static_assert(sizeof(OutputBuffer_PostPhysics::SceneInputInterface) > 800000,
                      "mSceneInputInterface is the real InSceneUpdateInterface (console 818768 B "
                      "at +11376), not the 44-byte span the +834784 accessor returns");
        static_assert(sizeof(OutputBuffer_PostPhysics::InterfaceAt834784) == 44,
                      "the X360-only member at console +834784 is 44 bytes (Construct zeroes 11 words)");
    }

    // X360 0x82761908, store for store -- run by CreateIOBuffer<OutputBuffer_PostPhysics>
    // @0x827B79D0, which allocates 0xD3D20 == 867,616 bytes. Three members have no Construct call
    // of their own and reach the console as raw zero stores: mTrafficSoundOutputInterface's
    // leading halfword, the array count inside mTrafficDirectorOutputInterface, and the whole
    // 44-byte mInterfaceAt834784. 0x82761908 has no per-address JSON under .ida-exports/, an
    // export-run gap rather than a missing function.
    void OutputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();               // stb 1, 0(this)
        mCrashTrafficInputInterface.Construct();        // +8
        mNetworkInterface.Construct();                  // +0xDA0  (3488)

        // CreateIOBuffer does not zero-fill, so without the next two stores these regions keep
        // whatever the IO stack's previous tenant left.
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
        mSceneInputInterface.Construct();               // +0x2C70 (11376)
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

    // X360 0x827A0A28 (baked 400): read-lock; return &mTrafficDirectorOutputInterface (this+6208).
    // Not mGameEventQueue: its epilogue is `addi r3,r28,0x1840` == +6208, the director interface's
    // seat (Construct's `stw 0,0x2650` lands inside that span), while the game-event queue's read
    // accessor is 0x827A0AD0 (baked 403) at +0x2660 == 9824.
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

    // X360 0x827A0C20 (baked 409): read-lock; return &mSceneInputInterface (this + 11376).
    // The leg WorldModule::BridgeEntityModulesToScene_PostPhysics @0x827AB608 calls first
    // (0x827AB6C0). Two witnesses put the aggregate here rather than at the 44-byte +834784 seat:
    // Construct @0x82761908 builds it with InSceneUpdateInterface::Construct(this+0x2C70), and
    // 11376 + 818768 == 830144, the next member.
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

    // X360 0x82711F88 (baked 416): write-lock; return &mResourceRequestInterface (this + 830672).
    // TrafficEntityModule::UpdateStreaming @0x82748848 is its only caller, and appends the
    // traffic streamer's own GameData request queue into it.
    OutputBuffer_PostPhysics::ResourceRequestInterface* OutputBuffer_PostPhysics::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
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

    // ------------------------------------------------------------------------
    // X360 0x82710BD8 (IDA `sub_82710BD8`): InputBuffer_PreScene::
    // GetActiveRaceCarOutputInterface() const.  DWARF :153.
    //
    // TrafficEntityModule::PreSceneUpdate's E_STARTINGUPSTATE_WAITING_FOR_PLAYER arm tests
    // `lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive()`. Without this the module
    // never leaves WAITING_FOR_PLAYER, so POPULATING never runs and no parked car is created.
    //
    // Body, instruction for instruction (0x82711850's twin shape):
    //     lbz    r11, 0(r28) ; extrwi r11,r11,1,27      ; IsBufferLockedForReading (bit 4)
    //     bne -> tail
    //     ... BeginAssert / StrStream "Not locked for reading\n" /
    //         FireAssert(msg, "d:\\p4\\...\\BrnTrafficEntityModuleIO.h", 157) / EndAssert
    //     addi   r3, r28, 0x40                          ; return &mActiveRaceCarOutputInterface
    // The +0x40 tail is what fixes the member: mActiveRaceCarOutputInterface is @64 here
    // (static_asserted in _AssertLayout above and written by the 0x8279FBE8 setter below).
    // ------------------------------------------------------------------------
    const InputBuffer_PreScene::ActiveRaceCarOutputInterface*
    InputBuffer_PreScene::GetActiveRaceCarOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mActiveRaceCarOutputInterface;
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
    // InputBuffer_PostScene (0x8279FEA8 / 0x827ACDE8). SetActiveRaceCarOutputInterface block-copies
    // the embedded race-car output interface; SetCrashTrafficOutputInterface clear-then-appends
    // each of the crash-traffic interface's two event queues.
    // These two keep the rodata's trailing "\n" in the lock message (the X360 string
    // aNotLockedForWr is "Not locked for writing\n"), unlike the other buffers in this file.
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
    // InputBuffer_PostPhysics -- the one getter this file owns. The rest live in the sibling TU
    // BrnTrafficEntityModuleIO_InputBuffer_Getters.cpp.
    // ========================================================================

    // ------------------------------------------------------------------------
    // X360 0x82711850 (IDA `sub_82711850`): InputBuffer_PostPhysics::
    // GetActiveRaceCarOutputInterface() const.  DWARF :358.
    //
    // The traffic module reads the player car through this getter in three places:
    // UpdateRaceCarHulls @0x82721460 (the sim-box centre), PostPhysicsUpdate @0x8274E6D0's tail
    // (meLocalPlayerIndex / mLocalPlayerPosition / mLocalPlayerDirection), and
    // StaticVehicles_CreateNewVehicles @0x827229F0's online-only proximity reject.
    //
    // Body (0x82711860..0x827118EC):
    //     lbz    r11, 0(r28) ; extrwi r11,r11,1,27      ; IsBufferLockedForReading (bit 4)
    //     bne -> tail
    //     ... BeginAssert / "Not locked for reading\n" /
    //         FireAssert(msg, "d:\\p4\\...\\BrnTrafficEntityModuleIO.h", 362) / EndAssert
    //     addis  r3, r28, 1 ; addi r3, r3, 0x28C0       ; == this + 0x128C0 == 75968
    // 75968 is mActiveRaceCarOutputInterface's console offset, the same destination the
    // 0x827A06C0 setter's XMemCpy(0x28F0) writes. Not static_asserted: the members ahead of it
    // (VehicleOutputInterface, the two queues, VehicleManagerOutputInterface) carry SIMD
    // aggregates and pointers that widen on the 64-bit host.
    // ------------------------------------------------------------------------
    const InputBuffer_PostPhysics::ActiveRaceCarOutputInterface*
    InputBuffer_PostPhysics::GetActiveRaceCarOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mActiveRaceCarOutputInterface;
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
    // The last two legs are the two rings
    // WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70 Clear()s and Append()s
    // every pre-physics frame. Without them mpEvents stays at whatever the IO stack's previous
    // tenant left and the first smashed traffic light writes events through a foreign pointer.
    // The mSceneResultQueue leg is the same trap one member earlier: a never-Constructed
    // VariableEventQueue asserts "Not Constructed" on its first AddEvent.
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
    // OutputBuffer_PostScene extra accessors: the non-const scene coarse-query queue (0x82711118)
    // and the const traffic-AI interface (0x827A0008).
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
    // OutputBuffer_PreScene accessors, laid out from Construct @0x82761790 plus the read-lock
    // ladder (baked lines 186 -> +16, 189 -> +818784, 192 -> +819328, a uniform +4 skew off the
    // DecFIGS declaration lines :182/:185/:188).
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
        // mpEvents on the host where the console has 4, so its host sizeof exceeds the console's
        // 818768. Parity from here down is by named member and sequence.
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
    // The 544-byte block's leg is TrafficToRaceCarInterface_PreScene::Construct(), which spells
    // the console's zero pattern field by field (7 doublewords of mSympatheticCrashers, the two
    // near-miss Array counts, miPotentialStompeeCount, muNearbyStaticVehicleCount, the four
    // flt_82001CC0 floats). Blanket-zeroing the block would also clear mPotentialStompees[8],
    // which the console leaves alone.
    void OutputBuffer_PreScene::Construct()
    {
        CgsModule::IOBuffer::Construct();                            // stb 1, 0(this)
        mSceneInputInterface.Construct();                            // +0x10
        mTrafficToRaceCarInterface_PreScene.Construct();             // the +0xC7E60 zero run
        mTriggerManagementInputInterface.GetAddTriggerEventQueue().Construct();     // +0xC8080
        mTriggerManagementInputInterface.GetRemoveTriggerEventQueue().Construct();  // +0xE8090
    }

    // X360 0x8279FD58 (baked 186, DWARF :182): read-lock; return &mSceneInputInterface
    // (this+16). The leg WorldModule::BridgeEntityModulesToSceneModule_PreScene @0x827AB490 calls
    // first (0x827AB570).
    const OutputBuffer_PreScene::SceneInputInterface*
    OutputBuffer_PreScene::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSceneInputInterface;
    }

    // X360 sub_82710D28 (baked 187): write-lock; return &mSceneInputInterface (this+16). The
    // producer side of the scene seat, i.e. the interface
    // TrafficEntityModule::CreateNewVehicleEntities calls AddEntity on.
    OutputBuffer_PreScene::SceneInputInterface*
    OutputBuffer_PreScene::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
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
    // OutputBuffer_Prepare accessors.
    // ========================================================================
    void OutputBuffer_Prepare::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_Prepare, mSceneInputInterface) == 16,
                      "mSceneInputInterface @16 (console; Construct @0x82761740 calls "
                      "InSceneUpdateInterface::Construct(this+0x10))");
        // No offsetof pin on mResourceRequestInterface: the member ahead of it is the real
        // 25-queue aggregate, whose host sizeof exceeds the console's 818,768 because each queue
        // carries an 8-byte mpEvents. What survives is the relation that matters:
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
    // OutputBuffer_PrePhysics. The header banner has the layout witnesses.
    // ========================================================================

    // @0x827618A0 (DWARF :307). Without this the buffer inherits
    // CgsModule::IOBuffer::Construct and every embedded queue's mpEvents stays NULL, which kills
    // BridgeEntityModulesToPhysicsModule_PrePhysics on "mpEvents != NULL" the moment it merges
    // the three interfaces. 25 X360 instructions, read verbatim; every member reached by name,
    // not by the console offsets the comments record:
    //   0x827618B8  stb  1, 0(this)                          -- IOBuffer::Construct (status = 1)
    //   0x827618BC  bl   VehicleInputInterface::Construct     (this+16)
    //   0x827618CC  bl   CreateAirRamEvent<20>::Construct     (this+142192)  \  == the effects
    //   0x827618D4  bl   CreateSpinEvent<10>::Construct       (this+143488)  /     interface
    //   0x827618E4  stw  0, 8(this+142192) ; stw 0, 0x518(this+142192)  -- the two queue lengths
    //   0x826718EC  bl   VehicleDriverInputInterface::Construct(this+143984)
    //   0x827618F8  stbx 0, this, 0x24720                     -- mbPlayingShowtime = false
    // The two explicit length stores are what VehicleEffectsInputInterface::Construct's two queue
    // Constructs already do (EventQueue::Construct writes mpEvents and clears miLength); the X360
    // emits them separately because it inlined both Constructs and kept the redundant zeroing.
    // Calling the named Construct reproduces the end state exactly.
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

    // ========================================================================
    // The PRE-DISPATCH pair -- the traffic render seam. The header's pre-dispatch banner
    // carries the allocator/Construct/Clear attestation for both interiors.
    //
    // LINK BLOCKER -- READ BEFORE BUILDING THE EXE. GameSource/World/WorldLinkStubs.cpp still
    // defines four gates that duplicate the bodies below:
    //   BrnTrafficIO::InputBuffer_PreDispatch::Construct()
    //   BrnTrafficIO::InputBuffer_PreDispatch::SetCameraPosition(rw::math::vpu::Vector3)
    //   BrnTrafficIO::InputBuffer_PreDispatch::SetVisibleEntities(
    //       const Array<CgsSceneManager::EntityId,650>&)
    //   BrnTrafficIO::OutputBuffer_PreDispatch::Construct()
    // Both TUs are mounted in tools/build/build_game_exe.bat (this file at :2032,
    // WorldLinkStubs.cpp at :2168), so the exe link fails with four LNK2005s until the four gate
    // DEFINITIONS are deleted outright. DELETE THIS BANNER WHEN they are gone. The per-TU
    // `cl /c` gate cannot see duplicate definitions, so a green selfcheck here proves nothing
    // about the link.
    //
    // BEHAVIOUR CHANGE on retirement, in both cases a correction: each Construct gate is
    // `memset(this, 0, sizeof(*this))` and never calls CgsModule::IOBuffer::Construct, so every
    // PreDispatch buffer so far has run with eStatusConstructed clear, a state the console cannot
    // be in (@0x8275CEE8 and @0x8275CF28 both open with `stb 1, 0(r3)`). The gates also
    // bulk-zero the element buffers where the real bodies write only the console's stores.
    // ========================================================================

    void InputBuffer_PreDispatch::_AssertLayout()
    {
        // Pointer-invariant pins only. Every member here is pointer-free, so the console's spans
        // survive to x64 and can be pinned as facts about the types, not as X360 byte offsets.
        static_assert(sizeof(CgsSceneManager::EntityId) == 4,
                      "EntityId is one packed 32-bit handle on both platforms");
        static_assert(sizeof(Array<CgsSceneManager::EntityId, 650u>) == 650 * 4 + 4,
                      "Array<EntityId,650> is 650 elements plus the trailing live-count word -- "
                      "the console's Construct @0x8275CEE8 zeroes that word at +2632, i.e. 2600 "
                      "bytes past the +32 the elements start at");
        static_assert(sizeof(Vector3) == 16,
                      "mCameraPosition is the 16-byte vector Construct stores with a single "
                      "stvx128 at this+16");
        static_assert(offsetof(InputBuffer_PreDispatch, maTrafficEntityIds)
                        - offsetof(InputBuffer_PreDispatch, mCameraPosition) == 16,
                      "maTrafficEntityIds follows mCameraPosition immediately (console +16 -> +32)");
    }

    // X360 0x8275CEE8 -- run by CreateIOBuffer<InputBuffer_PreDispatch> @0x827B7250
    // (which allocates 0xA50 == 2640 bytes). Store for store:
    //   stb  1,     0(this)     -- IOBuffer::Construct (status = constructed)
    //   stw  0,  0xA48(this)    -- maTrafficEntityIds' live-count word (+2632)
    //   stvx v0(0), this+16     -- mCameraPosition = the zero vector
    // The console's order is status, count, vector; the vector store is last only
    // because its {0,0,0,0} had to be built on the stack first.
    void InputBuffer_PreDispatch::Construct()
    {
        CgsModule::IOBuffer::Construct();   // stb 1, 0(this)

        maTrafficEntityIds.Clear();         // stw 0, 0xA48(this)  -- off the -1 sentinel
        mCameraPosition.SetZero();          // stvx {0,0,0,0}, this+16
    }

    // DWARF :442. The console inlines it (no exported symbol); "clear" for this
    // buffer is the same single store Construct makes -- the visible-entity list
    // back to empty. The camera position is deliberately NOT touched: Construct
    // is what seeds it, and every producer overwrites it in the same breath.
    void InputBuffer_PreDispatch::Clear()
    {
        maTrafficEntityIds.Clear();
    }

    // The two de-inlined writers. WorldModule::GenerateDispatchLists @0x827D1CE8 fills both
    // members inline, between the buffer's Construct() and its LockForRead(), so unlike every
    // other setter in this file neither takes or asserts a write lock. None is held at that
    // point, and adding an assert here would fire on the first frame.
    void InputBuffer_PreDispatch::SetVisibleEntities(
        const Array<CgsSceneManager::EntityId, 650u>& lrEntities)
    {
        maTrafficEntityIds = lrEntities;
    }

    void InputBuffer_PreDispatch::SetCameraPosition(Vector3 lvCameraPosition)
    {
        mCameraPosition = lvCameraPosition;
    }

    void OutputBuffer_PreDispatch::_AssertLayout()
    {
        // POINTER-INVARIANT ONLY -- VehicleRenderInfo is {u32, f32, 4-byte enum}.
        static_assert(sizeof(BrnTraffic::VehicleRenderInfo) == 12,
                      "VehicleRenderInfo is 12 bytes (Array<VehicleRenderInfo,64>::Append "
                      "@0x8270A148 copies three dwords at a 12-byte stride)");
        static_assert(sizeof(Array<BrnTraffic::VehicleRenderInfo, 64u>) == 64 * 12 + 4,
                      "the whole array is 772 bytes -- which is exactly why the console's "
                      "Construct @0x8275CF28 zeroes the live-count word at +772 and "
                      "CreateIOBuffer @0x827B7320 allocates 0x308 == 776 for a buffer whose "
                      "payload starts at +4 (WorldModule::GenerateDispatchLists' `addi r22,r19,4`)");
    }

    // X360 0x8275CF28 -- run by CreateIOBuffer<OutputBuffer_PreDispatch> @0x827B7320
    // (0x308 == 776 bytes). The entire body is two stores:
    //   stb 1, 0(this)  ;  stw 0, 0x304(this)
    void OutputBuffer_PreDispatch::Construct()
    {
        CgsModule::IOBuffer::Construct();    // stb 1, 0(this)

        maTrafficRenderInfos.Clear();        // stw 0, 0x304(this)  -- off the -1 sentinel
    }

    // X360 0x82755BB8 -- `*(this + 772) = 0` and nothing else.
    void OutputBuffer_PreDispatch::Clear()
    {
        // [T1-rinfo] report what is being dropped BEFORE the count goes to zero.
        T1Diag_ReportTrafficRenderInfoCount(*this);

        maTrafficRenderInfos.Clear();
    }

    // ------------------------------------------------------------------------
    // [T1-rinfo] BRING-UP PROBE -- NOT IN THE X360 BINARY. DELETE WHEN STABLE.
    // Value-latched: prints only when the produced render-info count differs from the last one
    // printed, and only under BRN_TRAFFIC_DIAG. It is the one number that separates "the traffic
    // module produced no cars" from "the renderer dropped them".
    // ------------------------------------------------------------------------
    void T1Diag_ReportTrafficRenderInfoCount(const OutputBuffer_PreDispatch& lrBuffer)
    {
        static const bool sbTrafficDiag = (getenv("BRN_TRAFFIC_DIAG") != 0);
        if (!sbTrafficDiag || CgsDev::Log::gpDebugPrint == 0)
        {
            return;
        }

        // GetCount(), not GetLength(): the latter asserts the array left the
        // KI_UNCONSTRUCTED(-1) sentinel, and a probe must never fire an assert.
        const s32 liCount = lrBuffer.maTrafficRenderInfos.GetCount();

        static s32 siLastReported = -2;   // distinct from both 0 and the -1 sentinel
        if (siLastReported == liCount)
        {
            return;
        }
        siLastReported = liCount;

        *CgsDev::Log::gpDebugPrint
            << "[T1-rinfo] traffic VehicleRenderInfo count = " << liCount
            << " [DELETE-WHEN-STABLE]\n";
    }
}
}
