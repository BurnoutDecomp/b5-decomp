// ============================================================================
// BrnTrafficEntityModule_wT1_04.cpp -- the traffic streamer pump.
//
//   * TrafficEntityModule::UpdateStreaming         @0x82748848  (DWARF :1554)
//   * TrafficEntityModule::AddVehiclesToTargetList @0x82722470  PARTIAL
//
// SetAssetList publishes the catalogue but requests nothing. Bundles are requested by
// TrafficCarStreamer::Update @0x8274F740 pushing entries into the base streamer's target
// list, and its only caller in the image is UpdateStreaming, itself called from two arms of
// PostPhysicsUpdate @0x8274E6D0.
//
// The DWARF scope tree names every blob the X360 inlined here (ClearAssetList, the
// AreAllAssetsUnloaded/AreAllAssetsLoaded pair, GetGuiEventQueue, Append<2048>), so no
// inlined loop below is a guess at its own name.
//
// DWARF-vs-SHIP DELTA, asm wins: the PS3 scope tree shows no call to
// AddVehiclesToTargetList and none to TrafficCarStreamer::Update. The X360 asm has both
// (0x82748944 / 0x82748984 and 0x827489F0), so both are here.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData (muNumFlowTypes)
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull (muNumVehicleAssets / mauVehicleAssets)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // KU_INVALID_HULL

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"// CgsModule::Event / AddEvent / Append
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // gpDebugPrint / gxMessageFilterFlags


namespace BrnTraffic
{
namespace
{
    // THE GUI EVENT. X360 posts `AddEvent( guiQueue, &oneByte, 512, 1 )` at 0x82748920 and
    // 0x8274896C: type 512, payload one byte. The DWARF names the payload
    // BrnGui::GuiEventTrafficPoolEmptied with one `bool mbTrafficPoolEmpty` member
    // (BrnGuiEventTypeDefs.h:234) deriving GuiEvent<502>; 502 is the PS3 id, 512 is the ship's.
    //
    // The payload is declared here rather than in BrnGuiEventTypeDefs.h because that header's
    // events derive CgsGui::GuiEvent<N>, which this tree models with three u32 header words
    // (CgsGuiEvent.h:32). That spelling would be 13+ bytes and the console posts one. Until
    // CgsGui::GuiEvent<N> is fixed tree-wide, this uses the pattern already used for the same
    // problem in BrnPaybackManager.cpp:33-42: a local payload over the empty CgsModule::Event
    // plus a named id. Same bytes on the wire, no fabricated header.
    // [FLAG PC-platform leaf: local GUI payload spelling; see above.]
    const s32 KI_GUI_EVENT_TRAFFIC_POOL_EMPTIED = 512;   // X360 `li r5, 0x200`

    struct GuiEventTrafficPoolEmptied : public CgsModule::Event
    {
        bool mbTrafficPoolEmpty;   // BrnGuiEventTypeDefs.h:236
    };

    // One-shot leg gate, same pattern as the sibling partfiles.
    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
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
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::AddVehiclesToTargetList  @ 0x82722470   PARTIAL
//
// Decide which traffic vehicle assets should be resident, and flag them in the streamer
// (TrafficCarStreamer::AddVehiclesToTargetList @0x8274F6A0 ORs E_LOADFLAG_REQUESTED into
// maxLoadFlags per id). UpdateStreaming has just cleared those flags, so this re-states the
// whole desired set every frame; an unpumped streamer therefore unloads rather than keeps.
//
// miDEBUGFlowtypeOverride is module +0x727D8 == 468952 (`addis r31, r26, 7 ; addi r31, r31,
// 0x27D8` @0x82722494). DebugComponent::OnActivate @0x82762530 binds that address as the
// "Flowtype override" slider with range -1 .. muNumFlowTypes-1, so the branch below is "is the
// override engaged?" and -1 is off. Construct @0x82740220 writes -1 (0x82740C48 `stwx r29`);
// the only other reader is PickVehicleToSpawn @0x827235F8.
// ----------------------------------------------------------------------------
void TrafficEntityModule::AddVehiclesToTargetList()
{
    // 0x8272247C `lwz r11, 0x713F0(this) ; cmpwi r11, -1 ; beq` -- no local player, no camera
    // to stream around.
    if ( meLocalPlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }

    // The console's two-part test (0x8272249C, 0x827224B8) short-circuits exactly like this:
    // mpData is dereferenced only when the override is >= 0. TrafficData +0x14 ==
    // muNumFlowTypes.
    if ( miDEBUGFlowtypeOverride >= 0
         && static_cast<u32>( miDEBUGFlowtypeOverride ) < mpData->muNumFlowTypes )
    {
        // GATE -- the DEBUG flow-type-override arm body (0x827224A8..0x827225A8). The console
        // streams one flow type's assets instead of the player's hull and then returns, which
        // is why this gate returns too:
        //     const FlowType* lpFlow = mpData->mpapFlowTypes[ miDEBUGFlowtypeOverride ];
        //     Array<?,16> lAssetIds;  lAssetIds.Clear();
        //     for (i < lpFlow->muNumVehicleTypes)
        //         asset = mpData->mpaVehicleTypes[ lpFlow->mpauVehicleTypeIds[i] ].muAssetId;
        //         if (!lAssetIds.Contains(asset)) lAssetIds.Append(asset);
        //     mStreamer.AddVehiclesToTargetList( lAssetIds.GetLength(), &lAssetIds[0] );
        //
        // BLOCKER: the element type of that local array. The ledger and Array_char_16.cpp say
        // `Array<char,16>`, but IDA's names for the four bodies are front-truncated
        // ("char,16>::Append" at 0x8270B970) and no mangled name is exported, so a lost
        // `unsigned ` cannot be ruled out. Both ends of the path are u8 (VehicleTypeData::
        // muAssetId, and the sink AddVehiclesToTargetList(u32, const u8*)), so `char` would
        // need a reinterpret_cast at the sink and a fresh `Array<u8,16>` would mint an
        // unattested instantiation.
        // DELETE WHEN: `idc.get_name(0x8270B970)` / `ida_name.get_name(0x8271B6F8)` in the .i64
        // settles char vs unsigned char. The arm is ~15 lines of modelled types after that.
        //
        // Ship default -1 means this arm is never entered; engaging the slider streams nothing
        // for that frame instead of silently streaming the hull's set.
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

    // The HULL arm (0x82722578..0x82722640): stream the assets the local player's current hull
    // declares. Each Hull carries an inline asset-index table (Hull::mauVehicleAssets, X360
    // hull+64, count at hull+6).
    u16 luHull = KU_INVALID_HULL;

    {
        // GATE -- the replay-playback hull source (0x82722588..0x827225A0), i.e.
        // `if (*(this + 0x72520)) luHull = *(sub_82707090(this + 0x724C0) + 74);`. Module
        // +0x724C0 is the BrnReplays::TrafficEntitySerialiser and +0x72520 the replay-playback
        // latch; both sit in the DWARF's un-emitted :776/:777 window with no member in
        // BrnTrafficEntityModule.h, and the serialiser class has no owning header in this tree.
        // Construct writes the latch zero, so live gameplay takes the arm below; this gate
        // changes replay playback only.
        // DELETE WHEN: the two members are modelled and the serialiser gets a header.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "AddVehiclesToTargetList replay-playback hull source -- it reads the hull out of "
            "BrnReplays::TrafficEntitySerialiser (module +0x724C0) under the replay latch at "
            "module +0x72520, and BOTH sit in the DecFIGS un-emitted :776/:777 window with no "
            "member in BrnTrafficEntityModule.h; the serialiser class has no owning header in "
            "this tree at all. Construct writes the latch ZERO, so live gameplay takes the "
            "per-player hull list below" );
    }

    // maaRaceCarHulls[meLocalPlayerIndex] -- the per-active-race-car active-hull list, X360
    // this + 350220 + 24*index (Reset @0x8272CDA0 Appends into the same array at the same
    // base). Entry 0 is the hull the car is in; the rest are neighbours.
    const ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR>& lrPlayerHulls =
        maaRaceCarHulls[meLocalPlayerIndex];

    if ( lrPlayerHulls.GetLength() == 0 )
    {
        // 0x827225A8 `bne` -- before the first RecalculateActiveHulls the list is empty.
        return;
    }

    luHull = lrPlayerHulls.GetItem( 0 );

    if ( luHull != KU_INVALID_HULL )
    {
        const Hull* lpHull = GetHull( luHull );
        CGS_ASSERT( lpHull != 0, "lpHull" );            // baked .cpp line 8287

        mStreamer.AddVehiclesToTargetList( lpHull->muNumVehicleAssets,
                                           lpHull->mauVehicleAssets );
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateStreaming  @ 0x82748848   COMPLETE except one arm
// DWARF :1554 `void UpdateStreaming(OutputBuffer_PostPhysics *)`.
//
// Per-frame streamer pump, in the console's order: clear last frame's wanted bits, run the
// pool-empty handshake with the GUI, pump the base streamer, latch "all loaded" for the replay
// serialiser, then append the streamer's own GameData request queue into this frame's output
// buffer.
//
// That last step is what reaches the resource system: the base streamer does not post its own
// requests, Update() only fills mGDRequestInterface's queue. The console's source operand is
// `this + 469872` == &mStreamer + 24, which lands on mGDRequestInterface because the base's
// preceding fields are the vtable pointer plus mpTargetEntryList / mpPotentialList /
// mpCurrentEntryList / miStreamListLength / miPotentialListLength, 6 x 4 on a 32-bit target
// (BrnBaseStreamer.h:233-:238). Reached by name below, so no console offset survives.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateStreaming( BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput )
{
    // 0x82748864: the console inlines this loop (`lbz / clrrwi r9,r9,2 / stb`, bounded by
    // muNumAssets); the DWARF names it ClearAssetList. De-inlined.
    mStreamer.ClearAssetList();

    // The pool-empty handshake (0x82748898..0x827489AC). meEmptyTrafficPoolState is the
    // module's half of a GUI conversation: tell the HUD when the pool empties (1 -> 2, payload
    // true) and when it fills again (3 -> 0, payload false). States 0 and 3 also re-state the
    // target list, which keeps assets resident while the pool drains and refills.
    switch ( meEmptyTrafficPoolState )
    {
    case E_EMPTYTRAFFICPOOLSTATE_IDLE:
        // jumptable case 0 @0x82748980.
        AddVehiclesToTargetList();
        break;

    case E_EMPTYTRAFFICPOOLSTATE_EMPTYING:
        // jumptable case 1 @0x827488D0. The inlined scan is "every mauLoadStates[i] == 0",
        // i.e. AreAllAssetsUnloaded, not AreAllAssetsLoaded; the DWARF lists both names for
        // this function, one per arm. An empty catalogue counts as all-unloaded (the console
        // branches past the loop straight to `v9 = 1`).
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
        // jumptable case 2 @0x827489AC -- empty on the console. The pool stays empty until
        // something else moves the state on.
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

    // The pump (0x827489AC..0x827489F0). During replay playback the console replaces Update's
    // bonus-asset list with the recorded one:
    //     if (*(this + 0x72520)) {
    //         luNumBonus = *(sub_82707090(this + 0x724C0) + 150);   // count byte
    //         lpauBonus  = sub_82707090(this + 0x724C0) + 151;      // the bytes after it
    //     }
    //     mStreamer.Update(lpauBonus, luNumBonus);
    {
        // GATE -- same two un-modelled members as the replay leg above (+0x72520 latch,
        // +0x724C0 serialiser); sub_82707090 is an unnamed serialiser accessor.
        //
        // The (0, 0) below is not a placeholder: it is the console's own value on every
        // non-replay frame (both locals start zero, only the latched arm changes them,
        // 0x827489B0). Live gameplay is unchanged; replay playback loses the recorded set.
        // DELETE WHEN: the two members are modelled and the serialiser gets a header.
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
        // GATE -- the "all loaded" latch (0x827489F4..0x82748A1C), console
        // `if (*(this+0x72520) && !*(this+0x72521)) *(this+0x72521) = AreAllAssetsLoaded();`.
        // Both bytes are in the same un-modelled :776/:777 window. The second is set to one by
        // Construct and cleared by EnterReplay @0x827081D8 and LeaveReplay @0x82708248, i.e. a
        // "replay assets have arrived" latch re-armed at each replay boundary. Dead on every
        // live-gameplay frame. DELETE WHEN: the two members are modelled.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "UpdateStreaming replay assets-arrived latch (module +0x72521, set from "
            "AreAllAssetsLoaded under the +0x72520 replay latch) -- same un-modelled "
            ":776/:777 window members. Dead on every non-replay frame" );
    }

    // 0x82748A20..0x82748A30: carry the streamer's requests into this frame's output buffer.
    // This is the only statement that uses lpOutput, which is why the parameter stays.
    //
    // The return value is discarded on purpose: Append returns bool and the console leaves it
    // in r3 at the tail, but the DWARF types this function `void` (:1554), and a tail-position
    // value is not a return type. The overflow case it would report is Append's own assert.
    lpOutput->GetResourceRequestInterface()->mRequestQueue.Append(
        mStreamer.GetGameDataRequestInterface()->mRequestQueue );
}

}
