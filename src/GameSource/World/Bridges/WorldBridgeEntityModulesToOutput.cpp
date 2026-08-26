#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"      // CgsModule::VariableEventQueue<N,16>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gpDebugPrint (PROPS-BOOT one-shot)
#include "GameSource/GameState/BrnGameEvents.h"                       // E_EVENT_RECORD_PROP_HIT (111)

#include <stdlib.h>                                                   // getenv ([DIAG] BRN_PROP_DIAG)

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
// BridgeEntityModulesToOutput_PostPhysics  @ 0x827AEEB0   (200 instructions)
//
// ⭐⭐ THE EXPORT HOLE IS CLOSED (2026-08-20, gateui wave, owner `wire`). Every earlier
// revision of this banner said "@0x827AEEB0 is one of the holes in the JSON export set", and
// the leg placement below was therefore GUESSED. It has now been dumped in full (pseudocode
// + all 200 instructions) from a private BURNOUT_X360_ARTIST.XEX.i64 copy. The console body,
// in ITS OWN ORDER, is:
//
//    :74/:75/:76/:77   the four null tripwires (world out / traffic / race car / prop)
//     1  BridgeRaceCarResourceRequestsToOutput(a1, out, raceCarOut)          @0x827AEF78
//     2  Append<4096,16>( out->GetResourceRequestResourceInterface(),
//                         worldEntityOut->GetResourceRequestInterface() )     @0x827AEF94
//     3  [PerfMon a1+6167724]  Append<4096,16>( out->…RequestResourceInterface(),
//                         trafficOut->GetResourceRequestInterface() )         @0x827AEFC0
//     4  Append<32768,16>( out->sub_827A4E08(), trafficOut->…G() )            @0x827AEFE4
//     5  BridgeRaceCarEntityInfoToOutput_PostPhysics(a1, out, raceCarOut)     @0x827AEFF4
//     6  [PerfMon] Append<1536,16>( out->GetGameEventQueue(),
//                         trafficOut->GetGameEventQueue() )                   @0x827AF018
//     7  out->AppendTrafficTypeResponseQueue( trafficOut->… )                 @0x827AF02C
//   ⭐8  out->AppendPropBecamePhysicalEventQueue( propOut->GetPropBecamePhysicalEventQueue()
//                         const /*0x827A1F58, +0x10*/ )                       @0x827AF048
//   ⭐9  out->SetPropVFXLocatorQueue( propOut->GetPropVFXLocatorQueue()
//                         const /*0x827A1E08, +0xCB2C0*/ )                    @0x827AF05C
//  ⭐10  Append<RecordPropHitEvent,50>( out->GetGameEventQueue(),
//                         propOut->GetRecordHitPropQueue() const
//                         /*0x827A1EB0, +0x160*/, 111 )                       @0x827AF07C
//                         (`li r5, 0x6F` == 111 == E_EVENT_RECORD_PROP_HIT @0x827AF078)
//  ⭐11  Append<HitOverheadSignEvent,100>( out->GetGameEventQueue(),
//                         propOut->GetHitOverheadSignQueue() const
//                         /*0x827A1D60, +0x7B0*/, 118 )                       @0x827AF09C
//                         (`li r5, 0x76` == 118 == E_EVENT_OVERHEAD_SIGN_HIT @0x827AF098)
//  ⭐12  if ( propOut->mbShouldRequestProgression /*lbzx at +0xCB61C @0x827AF0A8*/ )
//            out->GetGameEventQueue()->AddEvent(&lEvent, 112, 1)              @0x827AF0C8
//                         (112 == E_EVENT_REQUEST_PROP_PROGRESSION, DWARF
//                          GameSource/GameState/BrnGameEvents.h:122; the payload is the EMPTY
//                          struct RequestPropProgression, DWARF :442 -- `li r6,1` is its
//                          sizeof, and r4 points at an UNINITIALISED 1-byte stack slot)
//    13  [PerfMon] SetTrafficNetworkOutputInterface / SetTrafficSoundOutputInterface /
//                  SetTrafficDirectorOutputInterface                          @0x827AF0E4..
//    14  [PerfMon a1+6167720] SetDirectorVehicleInputInterface( raceCarOut->… ) @0x827AF158
//    15  SetWorldEntityStatusInterface( worldEntityOut->… )                   @0x827AF174
//    16  AppendReplayRequestInterface × 3 (race car, PROP, traffic)           @0x827AF190..
//
// The sole caller is WorldModule::Update @0x827D8218 -- which is the committed
// BrnWorldModule.cpp call site that already brackets it with
// LockBuffersForIO(lpUpdateOutputBuffer, …four source buffers), i.e. **destination
// write-locked, every source READ-locked** (CgsModuleUtils.h). That is why legs 8-12 go
// through the buffer's const accessors; the non-const twins would fire "Not locked for
// writing". (See the read-lock twin table in BrnPropEntityModuleIO.h -- those six symbols are
// real but IDA-unnamed, which is why an export-set grep previously concluded they don't exist.)
//
// Leg 10, the RecordPropHitEvent transfer, is the ONLY world-side hop on the smash-gate /
// billboard UI chain. PropEntityModule::ProcessContacts fills the
// source queue at run time (PropEntityModule_wQ2_03.cpp :: ProcessContacts, the `lbRecord`
// block) and the source queue IS Constructed (BrnPropEntityModuleIO_OutputBuffer_PostPhysics.cpp
// :: Construct -- the never-Constructed-queue trap does not apply here), as is the destination
// (BrnWorldModuleIO_UpdateOutputBuffer.cpp :: Construct, `mGameEventQueue.Construct()`).
//
// STILL DROPPED, DELIBERATELY (each now exactly specified above, so landing one is mechanical):
//   * legs 4/6/7/13 -- the remaining traffic transfers. Source getters un-homed on this
//     build. (Leg 3, the resource-request flush, has landed -- see the body.)
//   * legs 8/9  -- the prop became-physical + VFX-locator queues. Both destinations exist and
//     are typed (BrnWorldModuleIO.h :: AppendPropBecamePhysicalEventQueue /
//     SetPropVFXLocatorQueue) and both const source getters now exist, so these are two
//     one-liners; they are OFF the OnPropHit chain, so the gateui brief says note-don't-land.
//   * leg 11 -- the overhead-sign transfer (game event 118). ⚠️ NOT the billboard/smash-gate
//     feature: it feeds CrashModeScoring::DealWithHitOverheadSign, the Showtime overhead-sign
//     scorer (gateui scout §0.1).
//   (leg 12 is LANDED -- see the block at the end of the body; `E_EVENT_REQUEST_PROP_PROGRESSION
//    = 112` + `struct RequestPropProgression` live in GameSource/GameState/BrnGameEvents.h
//    :43 / :138.)
//   * leg 16's prop arm -- AppendReplayRequestInterface(propOut->GetReplayRequestInterface()).
//   * The console's two CPU monitors (a1+6167720 / +6167724) are not modelled on any leg here;
//     every sibling bridge in this file takes the same disposition.
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

    // Leg 3, the TRAFFIC resource-request flush @0x827AEFC0 --
    // Append<4096,16>( out->GetResourceRequestResourceInterface(),
    // trafficOut->GetResourceRequestInterface() ). Without it,
    // TrafficEntityModule::UpdateStreaming's per-frame carry-over fills the traffic output
    // buffer and the requests die there. The console's PerfMon bracket (a1+6167724) takes
    // this file's standing disposition and is not modelled.
    lpOutputBuffer->GetResourceRequestResourceInterface()->mRequestQueue.Append(
        lpTrafficOutput_PostPhysics->GetResourceRequestInterface()->mRequestQueue);

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
        // frame. (POSITION CORRECTED-BY-MEASUREMENT 2026-08-20: the console runs this leg late,
        // at @0x827AF158 inside the a1+6167720 monitor bracket, AFTER the prop and traffic
        // transfers -- not here with the other race-car ones, which is where it was placed while
        // @0x827AEEB0 was still an export hole. It is left in place: SetDirectorVehicleInputInterface
        // Clears-then-Appends into a destination no other leg in this function touches, so its
        // position is behaviourally free, and moving it would be churn on a working camera chain.)
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

    // ================================================================================
    // ⭐⭐ THE RECORD-PROP-HIT TRANSFER -- console leg 10, @0x827AF064..0x827AF07C.
    //   0x827AF064  mr r3, r28                 ; r28 == lpPropOutput_PostPhysics
    //   0x827AF068  bl sub_827A1EB0            ; GetRecordHitPropQueue() const  -> +0x160
    //   0x827AF070  bl …GetGameEventQue        ; UpdateOutputBuffer::GetGameEventQueue()
    //   0x827AF078  li r5, 0x6F                ; == 111 == E_EVENT_RECORD_PROP_HIT
    //   0x827AF07C  bl Append<RecordPropHitEvent,50>
    // and inside that helper (@0x827AEC10, exported): it walks 0..GetLength() and forwards each
    // element to the three-arg AddEvent with liSize == 32 (`li r6,0x20` @0x827AECCC).
    //
    // ⭐⭐ WHY IT MATTERS: this is THE world-side hop of the smash-gate / billboard chain, and
    // it was the one explicitly dropped (`(void)lpPropOutput_PostPhysics;`). Downstream:
    // BrnGameModule::BridgeWorldToGameState @0x823E5368 appends this GameEventQueue into the
    // GameState PostWorldInputBuffer, GameStateModule::PostWorldUpdate @0x8238F358 carries it,
    // PreWorldUpdate @0x823A5328 merges it, and ProcessGameEvents @0x823A0A18 case 111 calls
    // StuntManager::OnPropHit @0x8236EE18. With this leg dropped every one of those runs on an
    // empty queue -- OnPropHit is unreachable no matter what the GameState side lands.
    //
    // The prop output buffer is READ-locked here (WorldModule::Update's LockBuffersForIO
    // bracket), hence the const accessor; the queue's producer is
    // PropEntityModule_wQ2_03.cpp :: ProcessContacts.
    //
    // ⚠️ NOT BrokenPropEvent. PropEntityIO::BrokenPropEvent (+0x820) has an AddEvent producer but
    // NO Append instantiation anywhere in the image, so it never leaves the world module; the
    // GameState feed is RecordPropHitEvent. (BrnPropEntityModule.h's "the outbound BrokenPropEvent
    // … that GameState's StuntManager latches" comment is stale -- filed, not fixed here.)
    //
    // [FLAG] The `!= 0` guard is NOT in the console (its :77 assert is a non-gating tripwire and
    // it dereferences regardless). It is carried for consistency with the two sibling legs above,
    // which this TU already guards the same way; it can only differ from the console on a path
    // that would fault there.
    // ================================================================================
    if (lpPropOutput_PostPhysics != 0)
    {
        const BrnWorld::PropEntityIO::OutputBuffer_PostPhysics::RecordHitPropQueue* lpRecordHitPropQueue =
            lpPropOutput_PostPhysics->GetRecordHitPropQueue();

        lpOutputBuffer->GetGameEventQueue()->Append(
            *lpRecordHitPropQueue,
            BrnGameState::GameStateModuleIO::E_EVENT_RECORD_PROP_HIT);

        // [DIAG] NOT IN THE X360 BINARY -- the gateui `[UI-gate]` ladder's world rung. Same
        // idiom and same env guard as PropEntityModule_wQ_04.cpp's "[prop-diag] BREAK" line
        // (a once-evaluated static latch + the gpDebugPrint null test), first-N capped so a
        // pile-up of smashes does not flood the boot log. A line here proves the transport
        // fired; its absence with a "[prop-diag] BREAK" above it localises the break to
        // ProcessContacts' lbRecord gate rather than to this bridge.
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            // ⭐ [gateui] ROUND 8: 8 -> 64. Run 9 produced exactly 8 of these lines across a
            // 158 s drive, i.e. the budget was spent, so a long boot-drive lost the
            // bridged/OnPropHit correlation for every smash after the eighth -- which is the
            // correlation the wave's acceptance criterion is read off. 64 keeps it for a full
            // 275 s route without flooding the log.
            // ⚠️ The DOWNSTREAM rungs this one is correlated against are still first-16
            // (`[UI-gate] prop-hit event` in GameStateModule_gUI_00.cpp and
            // `[UI-gate] OnPropHit`/stunt-element in the StuntManager TUs, all
            // KI_UI_GATE_DIAG_FIRST_N == 16). So past the 16th smash a bridged line with no
            // matching downstream line means "the downstream budget is spent", NOT "the event
            // was dropped". Raise those three to match before reading a late gap as a break.
            static s32 siDiagLinesLeft = 64;

            const s32 liQueued = lpRecordHitPropQueue->GetLength();
            if ( sbPropDiag && liQueued > 0 && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siDiagLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] bridged prop-hit n=" << liQueued << "\n";
            }
        }

        // ================================================================================
        // ⭐ [gateui] THE PROP-PROGRESSION REQUEST -- console leg 12, @0x827AF0A8..0x827AF0C8.
        //   0x827AF0A8  lbzx r11, r28, 0xCB61C     ; propOut->mbShouldRequestProgression
        //   (branch on zero)
        //   0x827AF0C8  bl AddEvent(v28 /*out->GetGameEventQueue()*/,
        //                           &v38 /*a stack slot the console never initialises*/,
        //                           112 /*E_EVENT_REQUEST_PROP_PROGRESSION*/,
        //                           1 /*sizeof(RequestPropProgression)*/)
        //
        // `E_EVENT_REQUEST_PROP_PROGRESSION = 112` + `struct RequestPropProgression` live in
        // GameSource/GameState/BrnGameEvents.h:43 and :138, so the world side is the console's `if`.
        //
        // [FLAG] The console's payload is an UNINITIALISED 1-byte stack slot -- the event is a
        // pure marker and its consumer never reads the byte. An uninitialised read is not
        // reproducible across ABIs, so the (empty) record is default-constructed here; every
        // byte the queue copies is therefore defined. Same disposition this file already takes
        // on the console's other indeterminate reads.
        // ⓘ The flag is a LEVEL, not an edge: nothing in the tree clears
        // mbShouldRequestProgression, and the console does not clear it here either (the only
        // `stbx 0` at +0xCB61C is in OutputBuffer_PostPhysics::Construct @0x822EFEAC) -- and
        // that Construct runs every frame, because the buffer is a per-frame
        // IOBufferStack::CreateIOBuffer<T> allocation. So it is a strict one-frame request, and
        // this leg posts at most one event per frame. Not a double-post seam.
        // ================================================================================
        if (lpPropOutput_PostPhysics->ShouldRequestPropProgression())
        {
            const BrnGameState::GameStateModuleIO::RequestPropProgression lRequest =
                BrnGameState::GameStateModuleIO::RequestPropProgression();

            lpOutputBuffer->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                BrnGameState::GameStateModuleIO::E_EVENT_REQUEST_PROP_PROGRESSION,
                static_cast<s32>(sizeof(lRequest)));
        }
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
// [FLAG RETIRED 2026-08-24, tut-ticker wave] the game-event queue append used to be dropped
// ("the race-car module's game-event producers are un-homed"). RaceCarEntityModule::
// SendGameEvents is bodied now (the training-request drain; its event-9 arm stays parked in
// ITS banner) and the transfer is live below.
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

    // ⭐ [tut-ticker] the console's fourth statement -- LANDED 2026-08-24 with its producer
    // (RaceCarEntityModule::SendGameEvents, which now drains the training-request ring as game
    // event 113 into the source queue):
    //     VariableEventQueue<1536,16>::Append<1536,16>( out->GetGameEventQueue(),
    //                                                   in->GetGameEventQueue() );
    // (the old FLAG above -- "the transfer lands with them" -- is paid.)
    lpOutputBuffer->GetGameEventQueue()->Append(
        *lpRaceCarOutput_PostPhysics->GetGameEventQueue());
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

    // ⭐ GUARDS DELETED 2026-08-25 (aimodule wave), exactly as the note they replaced asked
    // ("delete the guards when the AI output buffer's members land"). The four const getters
    // were inert gates returning NULL; they now return the addresses of real typed members of
    // BrnAI::AIModuleIO::OutputBuffer, so all four transfers run unconditionally as the X360's
    // do. ⭐ THE FIRST ONE IS THE WHOLE POINT OF THIS WAVE: it is how AIModule::LoadMapData's
    // LoadBundle("AI.dat") and AcquireResource("WorldMapData") requests leave the AI module and
    // reach the resource module. WorldModule::Prepare's stage-11 NOT-DONE arm calls this bridge
    // on every re-entry, which is the pump that carries the request out and lets the reply come
    // back on the AI module's own receiver queue.
    lpWorldOutput->AppendResourceRequestInterface(
        lpAIOutputBuffer->GetAIResourceRequestInterface() );
    lpWorldOutput->AppendRouteResponseQueue( lpAIOutputBuffer->GetRouteResponseQueue() );
    lpWorldOutput->SetAICarOutputInterface( lpAIOutputBuffer->GetAICarOutputInterfaceConst() );
    lpWorldOutput->AppendGameEventQueue( lpAIOutputBuffer->GetGameEventQueueConst() );
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
