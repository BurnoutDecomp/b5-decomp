#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // gpDebugPrint (PROPS-BOOT one-shot)

// ============================================================================
// GameSource/World/Bridges/WorldBridgeWorldModuleToPropModule.cpp
//
// WorldModule::BridgeWorldModuleToPropModule_PreScene  @ 0x827AACF8  (32 instructions)
//
// THE STREAMING HAND-OFF. The world entity module runs its PVS query and its
// WorldGraphicsStreamer every pre-scene frame and publishes the result as three event
// queues plus the player's zone number on its OWN pre-scene output buffer
// (WorldEntityModule::UpdateStream @0x822F9740 fills mPropInstancesNeededForZoneQueue and
// writes miPlayerZoneNumber; OnWorldGraphicsLoadComplete @0x822D7828 /
// OnWorldGraphicsUnloadBegin post into the two graphics queues). NOTHING ELSE carries them
// across the module boundary -- this bridge is the only reader. While it was an inert
// WorldLinkStubs gate the prop entity module's pre-scene input buffer saw an empty
// PropInstancesNeededForZone queue every frame, so PropEntityModule::UpdateStreaming never
// asked GameData for a single prop-instance bundle and no zone ever loaded.
//
// ---- DWARF home ------------------------------------------------------------
// The PS3 DecFIGS unity dump (_compile/BrnWorldBridgesUnity.cpp:3872) places this function
// between BridgeRaceCarModuleToWorldModule_PreScene (WorldBridgeEntityModulesToEntityModules
// .cpp:92) and BridgeCrashModuleToPropModule_PostScene (:163), so its home TU is
// WorldBridgeEntityModulesToEntityModules.cpp and its declaration already lives in that TU's
// header (included above). It is bodied in THIS file for one reason only:
//
//   ⭐ FILE SPLIT, exactly the WorldBridgeRaceCarToWorldModule.cpp precedent (2026-08-01).
//   WorldBridgeEntityModulesToEntityModules.cpp IS NOT MOUNTED -- its two remaining bridges
//   need three IO accessors that are still declaration-only (TriggerEntityModuleIO::
//   InputBuffer_PreScene::GetInputInterface, BrnTrafficIO::OutputBuffer_PostScene::
//   GetTrafficToRaceCarInterface_PostScene, BrnTrafficIO::OutputBuffer_PreScene::
//   GetTriggerManagementInputInterface). This bridge needs NONE of them and closes with
//   ZERO unresolved externals, so it gets its own mountable TU rather than waiting.
//   DELETE-WHEN: those three accessors land and the parent TU can be mounted whole.
//
// ---- The console body, instruction for instruction (0x827AACF8..0x827AAD74) ----
//   r4 = lpPropInputBuffer_PreScene (dest), r5 = lpWorldOutputBuffer_PreScene (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827AAD0C and never read.
//
//   bl 0x827A2B30   WorldEntityIO::OutputBuffer_PreScene::GetPropInstancesNeededForZoneQueue() const   (R, src+0x004)
//   bl 0x827A14D0   PropEntityIO::InputBuffer_PreScene::GetPropInstancesNeededForZoneQueue()           (W, dest+0x004)
//   bl 0x827A8810   EventQueue<PropInstancesNeededForZoneEvent,30>::Append
//   bl 0x827A2A88   WorldEntityIO::OutputBuffer_PreScene::GetPropGraphicsLoadedQueue() const           (R, src+0x04C)
//   bl 0x827A1380   PropEntityIO::InputBuffer_PreScene::GetPropGraphicsLoadedQueue()                   (W, dest+0x04C)
//   bl 0x827A88F0   EventQueue<PropGraphicsLoadedEvent,25>::Append
//   bl 0x827A2BD8   WorldEntityIO::OutputBuffer_PreScene::GetPropGraphicsUnloadedQueue() const         (R, src+0x08C)
//   bl 0x827A1428   PropEntityIO::InputBuffer_PreScene::GetPropGraphicsUnloadedQueue()                 (W, dest+0x08C)
//   bl 0x827A89D0   EventQueue<PropGraphicsUnloadedEvent,25>::Append
//   lis r11,0xC ; ori r11,r11,0x8604 ; lwzx r11,src,r11 ; stw r11,0x788(dest)
//                   -- the INLINED GetPlayerZoneNumber()/SetPlayerZoneNumber() pair.
//
// ⚠ THE ZONE-NUMBER WORD. src+0xC8604 == 820740 is the SAME word WorldEntityModule::
// UpdateStream writes (`lis r10,0xC ; ori r10,r10,0x8604 ; stwx r11,r21,r10` @0x822F98B8),
// i.e. WorldEntityIO::OutputBuffer_PreScene::miPlayerZoneNumber, and dest+0x788 is
// PropEntityIO::InputBuffer_PreScene::miPlayerZoneNumber. (Both IO headers carried the
// console offset as "+821764" == 0xC8A04 in a provenance comment; corrected in this wave --
// see the banner there. Neither end was miscompiled: both are reached BY NAME here, and the
// pair below is the whole reason the two numbers had to be reconciled.)
// Its resting value MATTERS: OutputBuffer_PreScene::Construct seeds it to -1 ("no zone"),
// and the prop module's streaming state machine distinguishes -1 from zone 0. With this
// bridge gated the prop input kept Construct's own 0, i.e. "the player is in zone 0".
//
// NOT reproduced: nothing. The console body has no asserts, no early-outs and no other
// stores; the tail `b __restgprlr_29` forwards the last Append's bool in r3 as a register
// artifact -- the logical return type is void (DWARF unity :3872).
//
// LOCKING: the caller brackets both buffers (BrnWorldModule.cpp:2020
// CgsModule::LockBuffersForIO( lpPropInputBuffer_PreScene, lpWorldOutputBuffer_PreScene ) --
// dest write-locked, source read-locked), which is why the source side must go through the
// read-lock const getters and the destination side through the write-lock ones.
// ============================================================================

namespace WorldModule
{

// ----------------------------------------------------------------------------
// Never called. Pins the ONE structural fact this bridge relies on that the
// compiler does not already enforce: the two ends of each Append are the SAME
// EventQueue instantiation, so the source can never overrun the destination.
// (The element type is compiler-enforced -- BaseEventQueue<T>::Append is typed --
// but the CAPACITY is not: it is what the console's 30/25/25 pairs assert on, and
// a future re-slice of either IO header could quietly widen one side.)
// No offsetof pins here: nothing in this TU touches a private member or a byte
// offset -- every field goes through its named accessor, and both IO buffers
// deliberately depart from the console layout on x64.
// ----------------------------------------------------------------------------
static void _AssertLayout()
{
    typedef BrnWorld::PropEntityIO::InputBuffer_PreScene   PropInput;
    typedef BrnWorld::WorldEntityIO::OutputBuffer_PreScene WorldOutput;

    static_assert(PropInput::PropInstancesNeededForZoneQueue::KI_LENGTH ==
                  WorldOutput::PropInstancesNeededForZoneQueue::KI_LENGTH,
                  "PropInstancesNeededForZone queue capacity must match on both ends (X360: 30)");
    static_assert(PropInput::PropInstancesNeededForZoneQueue::KI_LENGTH == 30,
                  "X360 EventQueue<PropInstancesNeededForZoneEvent,30>");
    static_assert(PropInput::PropGraphicsLoadedQueue::KI_LENGTH ==
                  WorldOutput::PropGraphicsLoadedQueue::KI_LENGTH,
                  "PropGraphicsLoaded queue capacity must match on both ends (X360: 25)");
    static_assert(PropInput::PropGraphicsLoadedQueue::KI_LENGTH == 25,
                  "X360 EventQueue<PropGraphicsLoadedEvent,25>");
    static_assert(PropInput::PropGraphicsUnloadedQueue::KI_LENGTH ==
                  WorldOutput::PropGraphicsUnloadedQueue::KI_LENGTH,
                  "PropGraphicsUnloaded queue capacity must match on both ends (X360: 25)");
    static_assert(PropInput::PropGraphicsUnloadedQueue::KI_LENGTH == 25,
                  "X360 EventQueue<PropGraphicsUnloadedEvent,25>");
}

// @ 0x827AACF8
void BridgeWorldModuleToPropModule_PreScene(
    void* lpWorldModule,
    BrnWorld::PropEntityIO::InputBuffer_PreScene* lpPropInputBuffer_PreScene,
    const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldOutputBuffer_PreScene)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827AAD0C, never read

    CGS_ASSERT(lpPropInputBuffer_PreScene != 0, "lpPropInputBuffer_PreScene != NULL");
    CGS_ASSERT(lpWorldOutputBuffer_PreScene != 0, "lpWorldOutputBuffer_PreScene != NULL");

    lpPropInputBuffer_PreScene->GetPropInstancesNeededForZoneQueue()->Append(
        *lpWorldOutputBuffer_PreScene->GetPropInstancesNeededForZoneQueue());

    lpPropInputBuffer_PreScene->GetPropGraphicsLoadedQueue()->Append(
        *lpWorldOutputBuffer_PreScene->GetPropGraphicsLoadedQueue());

    lpPropInputBuffer_PreScene->GetPropGraphicsUnloadedQueue()->Append(
        *lpWorldOutputBuffer_PreScene->GetPropGraphicsUnloadedQueue());

    lpPropInputBuffer_PreScene->SetPlayerZoneNumber(
        lpWorldOutputBuffer_PreScene->GetPlayerZoneNumber());

    // DIAGNOSTIC (prop-spawn wave 2026-08-12) -- NOT in the X360 binary. Deliberately NOT
    // gated on gxMessageFilterFlags (bit 0 is off in a default PC boot), so a boot log
    // always answers "did the streamer's zone request ever reach the prop module?".
    // One-shot: fires on the FIRST frame this bridge carries a non-empty queue.
    // Boot-log grep: "PROPS-BOOT world->prop bridge".
    {
        static bool sbLoggedFirstTransfer = false;
        if ( !sbLoggedFirstTransfer && CgsDev::Log::gpDebugPrint != 0 )
        {
            const s32 liNeeded =
                lpWorldOutputBuffer_PreScene->GetPropInstancesNeededForZoneQueue()->GetLength();
            const s32 liLoaded =
                lpWorldOutputBuffer_PreScene->GetPropGraphicsLoadedQueue()->GetLength();
            const s32 liUnloaded =
                lpWorldOutputBuffer_PreScene->GetPropGraphicsUnloadedQueue()->GetLength();
            if ( liNeeded != 0 || liLoaded != 0 || liUnloaded != 0 )
            {
                sbLoggedFirstTransfer = true;
                *CgsDev::Log::gpDebugPrint
                    << "PROPS-BOOT world->prop bridge first transfer: instancesNeeded "
                    << liNeeded << " graphicsLoaded " << liLoaded
                    << " graphicsUnloaded " << liUnloaded
                    << " playerZone " << lpWorldOutputBuffer_PreScene->GetPlayerZoneNumber()
                    << "\n";
            }
        }
    }
}

}   // namespace WorldModule
