// ============================================================================
// BrnTrafficEntityModule_wT1_04.cpp -- wave T1 (parked traffic cars) round 3,
// CLOSURE item 1: THE STREAMER PUMP.
//
// TWO FUNCTIONS LIVE HERE:
//   * BrnTraffic::TrafficEntityModule::UpdateStreaming          @0x82748848  *** COMPLETE-ish ***
//   * BrnTraffic::TrafficEntityModule::AddVehiclesToTargetList   @0x82722470  *** PARTIAL ***
//
// ---------------------------------------------------------------------------
// ⭐⭐ WHY THIS FILE IS THE WAVE-1 GATE, IN ONE PARAGRAPH.
//
// LoadData stage 1 already calls mStreamer.SetAssetList(...) and the rounds-1+2 boot log
// proves it runs -- "[T1-stream] SetAssetList published 27 traffic vehicle assets". But
// SetAssetList publishes the CATALOGUE; it requests nothing. A bundle is requested by
// TrafficCarStreamer::Update @0x8274F740 pushing entries into the BASE streamer's target
// list, and TrafficCarStreamer::Update has exactly ONE caller in the whole image: this
// function. Both of ITS call sites are arms of PostPhysicsUpdate @0x8274E6D0, and both were
// LogMissingLeg gates in BrnTrafficEntityModule_wT1_01.cpp until this round. That is why no
// VEH_T*_GR.BNDL was ever requested: the pump was never turned.
//
// Round 2 (cluster R2C) transcribed the whole function from the asm and then parked it,
// because landing it needed FOUR declarations in files that cluster did not own. This round
// owns every traffic-module file, so all four landed:
//     BrnTrafficEntityModule.h    void UpdateStreaming(BrnTrafficIO::OutputBuffer_PostPhysics*)
//     BrnTrafficEntityModule.h    void AddVehiclesToTargetList()
//     BrnTrafficEntityModuleIO.h  ResourceRequestInterface* GetResourceRequestInterface()   (NON-const)
//     (here)                      the GUI event id -- see the KI_GUI_EVENT_* note below
//
// ---------------------------------------------------------------------------
// SOURCE LADDER.
//   rung 1  ARTIST 0x82748848 / 0x82722470 -- pseudocode AND assembly, both read in full.
//           Authoritative for behaviour, argument order and every branch.
//   rung 2  DecFIGS. TWO things it supplies that the asm cannot:
//             * the declaration: `void UpdateStreaming(OutputBuffer_PostPhysics *)` at
//               BrnTrafficEntityModule.h:1554 -- VOID, even though the X360 leaves the tail
//               Append's bool in r3. A tail-position value is not a return type.
//             * the NAMES inside the body: the DWARF scope tree for this function
//               (_compile/BrnTrafficUnity.cpp:15427) lists `TrafficCarStreamer::ClearAssetList`,
//               `TrafficCarStreamer::AreAllAssetsUnloaded`, `TrafficCarStreamer::AreAllAssetsLoaded`,
//               `OutputBuffer_PostPhysics::GetGuiEventQueue`, `AddGuiEvent<BrnGui::
//               GuiEventTrafficPoolEmptied>`, `OutputBuffer_PostPhysics::GetResourceRequestInterface`,
//               `Append<2048>` and the local `GuiEventTrafficPoolEmptied lTrafficPoolEmptiedEvent`
//               (.cpp:8081 and .cpp:8101). Every inlined blob in the X360 asm is named by that
//               list -- the byte loop at streamer+9592 IS ClearAssetList, and the "are all
//               load-states zero" scan IS AreAllAssetsUnloaded. Nothing here is a guess at
//               what an inlined loop was called.
//   rung 3  Feb-2007 -- the leak's UpdateStreaming does not exist in this form (its
//           TrafficCarStreamer::Update takes no parameters at all), so it arbitrates nothing.
//
// ⚠️ DWARF-vs-SHIP DELTA, and the asm wins: the PS3 scope tree shows NO call to
// AddVehiclesToTargetList and NO call to TrafficCarStreamer::Update in this function. The
// X360 asm has both (0x82748944 / 0x82748984 and 0x827489F0). That is merge-window drift in
// the PS3 direction; rung 1 arbitrates, so both calls are here.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData (muNumFlowTypes)
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull (muNumVehicleAssets / mauVehicleAssets)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // KU_INVALID_HULL

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"// CgsModule::Event / AddEvent / Append
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // gpDebugPrint / gxMessageFilterFlags

#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG probes)

namespace BrnTraffic
{
namespace
{
    // ------------------------------------------------------------------------------------
    // THE GUI EVENT. X360: `AddEvent( guiQueue, &oneByte, 512, 1 )` at both post sites
    // (0x82748920/0x82748928 and 0x8274896C/0x82748974) -- type 512, payload ONE byte.
    // The DWARF names the payload BrnGui::GuiEventTrafficPoolEmptied and gives it a single
    // `bool mbTrafficPoolEmpty` member (BrnGuiEventTypeDefs.h:234), deriving GuiEvent<502> --
    // 502 is the PS3 id; the SHIP id is 512, which is the literal in the asm. Ship wins.
    //
    // ⚠️ WHY THE PAYLOAD IS DECLARED HERE AND NOT IN GameSource/Gui/BrnGuiEventTypeDefs.h.
    // That header's events derive `CgsGui::GuiEvent<N>`, which THIS TREE models with three
    // u32 header words (CgsGuiEvent.h:32) -- so a GuiEventTrafficPoolEmptied spelled that way
    // would be 13+ bytes, and the console posts ONE. (The same contradiction is already
    // written down beside GuiEventWrapper in CgsGuiEvent.h: "a GuiEvent<N> base (12 bytes)
    // would force offset 24" -- the console's own AddEvent offsets say the base is empty.)
    // Fixing CgsGui::GuiEvent<N> is a cross-subsystem change touching every GUI event in the
    // tree and is NOT this wave's business. So this file uses the pattern the tree ALREADY
    // uses for exactly this situation -- a file-local payload struct deriving the empty
    // CgsModule::Event plus a named KI_GUI_EVENT_* id, as in
    // GameSource/GameState/PaybackManager/BrnPaybackManager.cpp:33-42. Same bytes on the
    // wire as the console, no fabricated 12-byte header.
    // [FLAG PC-platform leaf: local GUI payload spelling; see the paragraph above.]
    // ------------------------------------------------------------------------------------
    const s32 KI_GUI_EVENT_TRAFFIC_POOL_EMPTIED = 512;   // X360 `li r5, 0x200`

    struct GuiEventTrafficPoolEmptied : public CgsModule::Event
    {
        bool mbTrafficPoolEmpty;   // BrnGuiEventTypeDefs.h:236
    };

    // Same PARTIAL-pattern one-shot leg gate the sibling partfiles use, so one boot log reads
    // as a single stream. [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    void LogMissingLeg( bool& lrbAlreadyLogged, const char* lpcLegNameAndReason )
    {
        if ( lrbAlreadyLogged )
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[T1-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // BRN_TRAFFIC_DIAG bring-up knob, same spelling as every sibling partfile.
    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool IsTrafficDiagOn()
    {
        static const bool sbOn = ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        return sbOn;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::AddVehiclesToTargetList  @ 0x82722470   *** PARTIAL ***
//
// Decide WHICH traffic vehicle assets should be resident right now, and flag them in the
// streamer (TrafficCarStreamer::AddVehiclesToTargetList @0x8274F6A0 ORs E_LOADFLAG_REQUESTED
// into maxLoadFlags for each id it is handed). UpdateStreaming has just cleared those flags,
// so this call re-states the whole desired set every frame -- which is why an unpumped
// streamer unloads rather than keeps.
//
// THE CONSOLE'S SHAPE (0x82722470, read from the asm):
//     if (meLocalPlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID) return;      // `!= -1` gate
//     if (miDEBUGFlowtypeOverride in [0, mpData->muNumFlowTypes))
//         ...flow-type arm...                                                 // GATED, below
//     else
//         ...hull arm...                                                      // REAL, below
//
// ⚠️ WHICH ARM THIS BUILD TAKES, AND WHY THAT IS THE CONSOLE'S SHIPPING ANSWER.
// ⭐ CORRECTED 2026-08-21 (round 3 FIX pass). What stood here claimed the selector lived at
// module +0x726D8, that no traffic function ever wrote it, that its ship default was therefore
// unknowable, and that the member was not modelled. All four claims were WRONG, and the
// arithmetic error is what produced them: the selector is at +0x727D8, so the sweep that
// "found no writer" was run against a constant that does not exist in the image.
//
//     0x82722494  addis r31, r26, 7          ; 7 << 16          == 0x70000
//     0x82722498  addi  r31, r31, 0x27D8     ; + 0x27D8         -> this + 0x727D8 == 468952
//
// 468952 is the very decimal the old banner quoted two lines below the wrong hex. The
// selector's identity is not inferred -- the traffic debug component binds that exact address
// by name (DebugComponent::OnActivate @0x82762530: `AddSlider(module + 468952, "Flowtype
// override")`, `SetRange(..., -1, mpData->muNumFlowTypes - 1)`), so -1 is the slider's own
// "off" value and the branch above is literally "is the flow-type override engaged?".
//
// ITS SHIP DEFAULT IS -1 AND IT IS FULLY ESTABLISHED FROM THIS BUILD:
//     TrafficEntityModule::Construct @0x82740220
//         0x82740BE4  li   r29, -1
//         0x82740C2C  ori  r10, r10, 0x27D8   # 0x727D8
//         0x82740C48  stwx r29, r31, r10      -> miDEBUGFlowtypeOverride = -1
// A repo-wide grep for `0x27D8` over the export set returns exactly four functions: this one,
// PickVehicleToSpawn @0x827235F8 (the other reader), Construct @0x82740220 (the WRITER), and
// DebugComponent::OnActivate @0x82762530 (the slider bind).
//
// The member IS modelled -- BrnTrafficEntityModule.h:1099 `s32 miDEBUGFlowtypeOverride` -- and
// _wT1_01.cpp already seeds it (`miDEBUGFlowtypeOverride = -1;  // 0x727D8 stwx r29`) and reads
// it in PickVehicleToSpawn. So the branch below is spelled as the console spells it, and only
// the override-ON arm's BODY is gated. Previously the code took the hull arm unconditionally
// and was right only by luck (default -1); with the slider engaged it silently streamed the
// wrong set.
// ----------------------------------------------------------------------------
void TrafficEntityModule::AddVehiclesToTargetList()
{
    // 0x8272247C `lwz r11, 0x713F0(this) ; cmpwi r11, -1 ; beq` -- with no local player there
    // is no camera to stream around and the function does nothing at all.
    if ( meLocalPlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }

    // 0x8272249C `lwz r11, 0(r31) ; cmpwi r11, 0 ; blt loc_827225AC` then
    // 0x827224B8 `lwz r11, 0(r31) ; lhz r10, 0x14(r3) ; cmplw ; bge loc_827225AC` -- the
    // console's own two-part test, and it short-circuits exactly like this: mpData is
    // dereferenced ONLY when the override is >= 0. Same spelling as PickVehicleToSpawn
    // @0x827235F8 (_wT1_01.cpp), which is the other reader of this selector.
    // TrafficData +0x14 == muNumFlowTypes (BrnTrafficDataResourceType.h).
    if ( miDEBUGFlowtypeOverride >= 0
         && static_cast<u32>( miDEBUGFlowtypeOverride ) < mpData->muNumFlowTypes )
    {
        // GATED LEG 1 -- the BODY of the DEBUG FLOW-TYPE OVERRIDE arm (0x827224A8..0x827225A8).
        //
        // When the slider is engaged the console streams the assets of ONE flow type instead
        // of the player's hull, and then RETURNS (0x827225A8 `b __restgprlr_26`) -- it does
        // not fall through to the hull arm, which is why this gate returns too:
        //     const FlowType* lpFlow = mpData->mpapFlowTypes[ miDEBUGFlowtypeOverride ];
        //     Array<?,16> lAssetIds;  lAssetIds.Clear();                 // `stw 0, var_40`
        //     for (i < lpFlow->muNumVehicleTypes)                        // `lbz r11, 8(r29)`
        //         asset = mpData->mpaVehicleTypes[ lpFlow->mpauVehicleTypeIds[i] ].muAssetId;
        //         if (!lAssetIds.Contains(asset)) lAssetIds.Append(asset);
        //     mStreamer.AddVehiclesToTargetList( lAssetIds.GetLength(), &lAssetIds[0] );
        //
        // ⭐ THE GATE REASON WAS REWRITTEN 2026-08-21 (round 3 FIX pass) BECAUSE BOTH OF THE
        // ORIGINAL REASONS WERE FALSE. Reason (1) was the +0x726D8 arithmetic error -- see the
        // banner; the selector is modelled, written and defaulted. Reason (2) claimed
        // "FlowType's own +0 / +8 members are not modelled in this tree either"; they ARE, and
        // by name: SharedClasses/Traffic/BrnTrafficFlowType.h declares `u16* mpauVehicleTypeIds`
        // (X360 +0) and `u8 muNumVehicleTypes` (X360 +8), and PickVehicleToSpawn in _wT1_01.cpp
        // already walks both. `VehicleTypeData::muAssetId` (element +5, stride 8) is modelled
        // too, and Array<...,16>::Clear/Contains/Append/GetLength/GetItem are all committed
        // inline in CgsArray.h with the explicit instantiation mounted (Array_char_16.cpp).
        //
        // THE ONE THING GENUINELY UNSETTLED, and it is small and precisely actionable: the
        // ELEMENT TYPE of the console's local array. The ledger and Array_char_16.cpp spell the
        // instantiation `Array<char,16>`, but IDA's own name for those four bodies is truncated
        // at the front ("char,16>::Append" at 0x8270B970 -- the `Array<` is missing, so a lost
        // `unsigned ` cannot be ruled out) and no mangled name is exported for them. Both ends
        // of the data path are u8: the source is `VehicleTypeData::muAssetId` (u8) and the sink
        // is `TrafficCarStreamer::AddVehiclesToTargetList(u32, const u8*)`. Landing the arm
        // against `Array<char,16>` would therefore need a reinterpret_cast at the sink -- a
        // fabricated pointer-type pun in the middle of a faithful path -- and landing it against
        // a fresh `Array<u8,16>` would mint an instantiation this build has not attested.
        // NEXT STEP (one line in the .i64, no reasoning): `idc.get_name(0x8270B970)` /
        // `ida_name.get_name(0x8271B6F8)` -- the untruncated name settles char vs unsigned char,
        // and the arm is ~15 lines of already-modelled types after that.
        //
        // OBSERVABLE CONSEQUENCE: with the slider at its ship default of -1 this arm is never
        // entered, so none. Engaging it (traffic debug menu, itself a gated leg in wQ7_02)
        // now streams NOTHING for that frame instead of silently streaming the hull's set --
        // a visible, logged hole rather than a wrong answer.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "AddVehiclesToTargetList DEBUG flow-type-override arm BODY (the selector, its -1 "
            "ship default from Construct @0x82740220, FlowType::mpauVehicleTypeIds/"
            "muNumVehicleTypes and VehicleTypeData::muAssetId are ALL modelled; the only open "
            "item is whether the console's local Array<...,16> is char or unsigned char, which "
            "the truncated IDA names at 0x8270B970/0x8271B6F8 do not settle and the sink "
            "TrafficCarStreamer::AddVehiclesToTargetList(u32, const u8*) makes load-bearing)" );
        return;
    }

    // ---- the HULL arm (0x82722578..0x82722640) -------------------------------------------
    // Stream the assets the LOCAL PLAYER's current hull declares. Each Hull carries its own
    // inline asset-index table (Hull::mauVehicleAssets, X360 hull+64, count at hull+6), which
    // is exactly the "these are the cars that belong in this part of the city" list.
    u16 luHull = KU_INVALID_HULL;

    {
        // GATED LEG 2 -- the REPLAY-PLAYBACK source (0x82722588..0x827225A0).
        //
        // Console: `if (*(this + 0x72520)) { luHull = *(sub_82707090(this + 0x724C0) + 74); }`
        // -- during replay playback the hull comes out of the recorded stream instead of the
        // live per-player hull list. Both members are inside the DWARF's UN-EMITTED :776/:777
        // window: +0x724C0 is the BrnReplays::TrafficEntitySerialiser the console registers
        // (`RegisterSerialiser(..., this + 468160)` in PostPhysicsUpdate @0x8274E6D0), and
        // +0x72520 is the replay-playback latch that SubmitCoronasForVehicle @0x82727BB0 and
        // RenderTrafficCar @0x82728B08 both test before reading the serialiser instead of live
        // state. NEITHER has a member in the keystone header, and TrafficEntitySerialiser has
        // NO owning header at all in this tree -- the class is declared inside
        // GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.cpp, a file this
        // wave does not own.
        //
        // ⚠️ SAFE FOR WAVE 1 AND SAY WHY: the latch is written ZERO by TrafficEntityModule::
        // Construct (`stb 0, 0x72520`), so live gameplay -- which is the only thing this build
        // reaches -- takes the else arm below. This gate changes replay playback, not driving.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "AddVehiclesToTargetList replay-playback hull source -- it reads the hull out of "
            "BrnReplays::TrafficEntitySerialiser (module +0x724C0) under the replay latch at "
            "module +0x72520, and BOTH sit in the DecFIGS un-emitted :776/:777 window with no "
            "member in BrnTrafficEntityModule.h; the serialiser class has no owning header in "
            "this tree at all. Construct writes the latch ZERO, so live gameplay takes the "
            "per-player hull list below" );
    }

    // maaRaceCarHulls[meLocalPlayerIndex] -- the per-active-race-car active-hull list
    // (X360 this + 350220 + 24*index; `short_9_::GetLength` / `short_9_::GetItem(.,0)` in the
    // pseudocode, and Reset @0x8272CDA0 Appends into the same array at the same base).
    // Entry 0 is the hull the car is actually in; the rest are neighbours.
    const ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR>& lrPlayerHulls =
        maaRaceCarHulls[meLocalPlayerIndex];

    if ( lrPlayerHulls.GetLength() == 0 )
    {
        // 0x827225A8 `bne` -> the console's own early-out (LABEL_11): before the first
        // RecalculateActiveHulls the list is empty and there is nothing to stream.
        return;
    }

    luHull = lrPlayerHulls.GetItem( 0 );

    if ( luHull != KU_INVALID_HULL )
    {
        const Hull* lpHull = GetHull( luHull );
        CGS_ASSERT( lpHull != 0, "lpHull" );            // baked .cpp line 8287

        mStreamer.AddVehiclesToTargetList( lpHull->muNumVehicleAssets,
                                           lpHull->mauVehicleAssets );

        if ( IsTrafficDiagOn() && CgsDev::Log::gpDebugPrint != 0 )
        {
            // [T1-stream] one-shot: the FIRST time the module actually asks the streamer for
            // anything. If this line never prints, the pump is not turning and no VEH_T*
            // bundle can ever be requested -- which was the rounds-1+2 state exactly.
            // DELETE-WHEN-STABLE.
            static bool sbLogged = false;
            if ( !sbLogged )
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-stream] AddVehiclesToTargetList: hull " << luHull
                    << " requests " << static_cast<s32>( lpHull->muNumVehicleAssets )
                    << " vehicle assets (player " << static_cast<s32>( meLocalPlayerIndex )
                    << ")\n";
            }
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateStreaming  @ 0x82748848   *** COMPLETE except one arm ***
//
// DWARF :1554 `void UpdateStreaming(OutputBuffer_PostPhysics *)`.
//
// Per-frame streamer pump, in the console's own order:
//   1. mStreamer.ClearAssetList()                       -- drop last frame's "wanted" bits
//   2. switch (meEmptyTrafficPoolState)                 -- the pool-empty handshake with the GUI
//   3. mStreamer.Update(bonusList, bonusCount)          -- rebuild the target list + pump the base
//   4. the "all loaded" latch for the replay serialiser
//   5. append the streamer's OWN GameData request queue into this frame's output buffer
//
// Step 5 is the one that actually reaches the resource system: the base streamer
// (BrnWorld::InternalBaseStreamer, BrnBaseStreamer.cpp) does NOT post its own requests --
// Update() fills mGDRequestInterface's queue and somebody has to carry it to the module's
// output buffer. THIS is that somebody. Verified end to end for this round:
//     InternalBaseStreamer::mGDRequestInterface  is  RequestInterface<2048>
//       -> RequestQueue<2048> -> ResourceRequestQueue<2048> -> VariableEventQueue<2048,16>
//     OutputBuffer_PostPhysics::mResourceRequestInterface  is  RequestInterface<4096>
//       -> ... -> VariableEventQueue<4096,16>
//     the console call is VariableEventQueue<4096,16>::Append<2048,16>( <2048,16> const& )
//       @0x82748A30, with the source at `this + 469872` == &mStreamer + 24.
// The console's `+24` lands on mGDRequestInterface because the base's five preceding fields
// are the vtable pointer plus mpTargetEntryList / mpPotentialList / mpCurrentEntryList /
// miStreamListLength / miPotentialListLength -- 6 x 4 == 24 on a 32-bit target
// (BrnBaseStreamer.h:233-:238). Reached BY NAME here through
// GetGameDataRequestInterface(), so the host's own widths apply and no console offset
// survives into the code.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateStreaming( BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput )
{
    // ---- 1. 0x82748864..0x82748894 --------------------------------------------------------
    // The console inlines the whole loop (`lbz / clrrwi r9,r9,2 / stb`, bounded by
    // muNumAssets); the DWARF names it TrafficCarStreamer::ClearAssetList, and the header's
    // inline is exactly that loop. De-inlined.
    mStreamer.ClearAssetList();

    // ---- 2. the pool-empty handshake (0x82748898..0x827489AC) -----------------------------
    // meEmptyTrafficPoolState is the module's half of a two-way GUI conversation: the HUD is
    // told when the traffic pool has emptied (state 1 -> 2, payload true) and when it has
    // filled again (state 3 -> 0, payload false). States 0 and 3 also re-state the streaming
    // target list, which is what keeps assets resident while the pool drains and refills.
    switch ( meEmptyTrafficPoolState )
    {
    case E_EMPTYTRAFFICPOOLSTATE_IDLE:
        // jumptable case 0 @0x82748980.
        AddVehiclesToTargetList();
        break;

    case E_EMPTYTRAFFICPOOLSTATE_EMPTYING:
        // jumptable case 1 @0x827488D0. The inlined scan is "every mauLoadStates[i] == 0",
        // which the DWARF names TrafficCarStreamer::AreAllAssetsUnloaded -- note it is the
        // UNLOADED predicate here, not AreAllAssetsLoaded (the DWARF scope tree lists BOTH
        // names for this one function, one per arm, which is how the pair is told apart).
        // An empty catalogue counts as "all unloaded" (the console's `beq` past the loop
        // straight to `v9 = 1`).
        if ( mStreamer.AreAllAssetsUnloaded() )
        {
            GuiEventTrafficPoolEmptied lTrafficPoolEmptiedEvent;   // DWARF local, .cpp:8081
            lTrafficPoolEmptiedEvent.mbTrafficPoolEmpty = true;    // `li r11,1 ; stb`

            lpOutput->GetGuiEventQueue()->AddEvent( &lTrafficPoolEmptiedEvent,
                                                    KI_GUI_EVENT_TRAFFIC_POOL_EMPTIED,
                                                    sizeof( bool ) );

            meEmptyTrafficPoolState = E_EMPTYTRAFFICPOOLSTATE_EMPTY;
        }
        break;

    case E_EMPTYTRAFFICPOOLSTATE_EMPTY:
        // jumptable case 2 @0x827489AC -- genuinely empty on the console. The pool stays
        // empty until something else moves the state on; nothing to stream and nothing to say.
        break;

    case E_EMPTYTRAFFICPOOLSTATE_FILLING:
        // jumptable case 3 @0x82748940. Re-state the target list every frame while refilling,
        // and tell the HUD the moment the assets are back.
        AddVehiclesToTargetList();

        if ( mStreamer.AreAllAssetsLoaded() )
        {
            GuiEventTrafficPoolEmptied lTrafficPoolEmptiedEvent;   // DWARF local, .cpp:8101
            lTrafficPoolEmptiedEvent.mbTrafficPoolEmpty = false;   // `stb r31` with r31 == 0

            lpOutput->GetGuiEventQueue()->AddEvent( &lTrafficPoolEmptiedEvent,
                                                    KI_GUI_EVENT_TRAFFIC_POOL_EMPTIED,
                                                    sizeof( bool ) );

            meEmptyTrafficPoolState = E_EMPTYTRAFFICPOOLSTATE_IDLE;
        }
        break;

    default:
        CGS_ASSERT( false, "Unhandled case in switch" );   // baked .cpp line 8221
        break;
    }

    // ---- 3. the pump itself (0x827489AC..0x827489F0) ---------------------------------------
    // The console computes an OVERRIDE BONUS-ASSET list before calling Update:
    //     const u8* lpauBonus = 0;  u32 luNumBonus = 0;
    //     if (*(this + 0x72520))                      // the replay-playback latch
    //     {
    //         luNumBonus = *(sub_82707090(this + 0x724C0) + 150);   // count byte
    //         lpauBonus  = sub_82707090(this + 0x724C0) + 151;      // the bytes after it
    //     }
    //     mStreamer.Update(lpauBonus, luNumBonus);
    // i.e. during REPLAY PLAYBACK the recorded bonus set replaces the one Update would derive
    // from the rendering history, so a replay streams the cars it was recorded with.
    {
        // GATED LEG 3 -- the same two un-modelled members as AddVehiclesToTargetList's
        // replay leg (the +0x72520 latch and the +0x724C0 TrafficEntitySerialiser); the
        // getter sub_82707090 is a serialiser accessor with no name in this tree either.
        //
        // ⚠️ THE (0, 0) PASSED BELOW IS NOT A PLACEHOLDER. It is the console's OWN value on
        // every non-replay frame -- the two locals are initialised to zero and only the
        // latched arm changes them (`mr r4, r31` with r31 == 0 at 0x827489B0). Live gameplay
        // is bit-identical; only replay playback loses the recorded bonus set.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "UpdateStreaming replay-playback bonus-asset override -- reads a count byte + "
            "list out of BrnReplays::TrafficEntitySerialiser (module +0x724C0) under the "
            "replay latch (module +0x72520); neither member exists in the keystone header "
            "(DecFIGS un-emitted :776/:777 window) and the serialiser has no owning header. "
            "The (0,0) passed to Update IS the console's own non-replay value, not a stand-in" );
    }

    mStreamer.Update( 0, 0 );

    {
        // ---- 4. the "all loaded" latch (0x827489F4..0x82748A1C) ---------------------------
        // Console: `if (*(this+0x72520) && !*(this+0x72521)) *(this+0x72521) = AreAllAssetsLoaded();`
        // Both bytes are in the same un-modelled :776/:777 window; the second is the flag
        // Construct sets to ONE and BOTH EnterReplay @0x827081D8 and LeaveReplay @0x82708248
        // clear -- a "the replay's assets have finished arriving" latch, re-armed on every
        // replay boundary. Guarded by the same replay latch as legs 2 and 3, so it is dead on
        // every live-gameplay frame this build reaches.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "UpdateStreaming replay assets-arrived latch (module +0x72521, set from "
            "AreAllAssetsLoaded under the +0x72520 replay latch) -- same un-modelled "
            ":776/:777 window members. Dead on every non-replay frame" );
    }

    // ---- 5. carry the streamer's requests into this frame's output buffer -----------------
    // 0x82748A20..0x82748A30. The write-locked getter that lands here this round
    // (sub_82711F88) is the ONLY consumer of UpdateStreaming's second parameter, and this is
    // the ONLY statement that uses it -- which is why the parameter could not be dropped even
    // though every other leg is `this`-only.
    //
    // ⚠️ THE RETURN VALUE IS DISCARDED ON PURPOSE. Append returns bool and the console leaves
    // it in r3 at the tail, but the DWARF types this function `void` (:1554) -- a tail-position
    // value is not a return type. Discarding it matches the declaration; the queue-overflow
    // case it would report is Append's own assert.
    lpOutput->GetResourceRequestInterface()->mRequestQueue.Append(
        mStreamer.GetGameDataRequestInterface()->mRequestQueue );
}

}
