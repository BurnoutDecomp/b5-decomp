// ============================================================================
// BrnTrafficEntityModule_wT1_03.cpp  --  wave T1 round 2, cluster R2C, partfile 3
//
// Bodies that belong to the BrnTrafficEntityModule.cpp ledger TU but not to the
// Prepare/LoadData partfile (BrnTrafficEntityModule_wQ7_02.cpp) or the spawn-leg partfile
// (BrnTrafficEntityModule_wT1_01.cpp).
//
// CONTAINS:
//   * BrnTraffic::TrafficPhysicsInfo::Construct  @0x82751E88  (14 insns)  *** PARTIAL ***
//   * BrnTraffic::TrafficPhysicsInfo::Destruct   @0x82751EE8  (2 insns)   *** COMPLETE ***
//
// ⭐ 2026-08-21 (wave T1 ROUND 3, closure item 4): Construct's owner-index store is now REAL
// and Destruct is landed. R2C's `sth r4, 0x100A(r3)` gate was a LAYOUT gap, and the layout
// patch it spelled out has been applied to BrnTrafficEntityModule.h
// (`u8 muPad205; u16 muOwningVehicleIndex;` after muContactSideFlags :204). Exactly ONE gate
// survives in this file -- the `stb 0, 0(this)` zero byte at record +0x00 -- and one grep for
// LogMissingLeg proves it.
//
// WHY THIS FILE EXISTS AT ALL -- the round-1 park it closes:
//   "TrafficPhysicsInfo::Construct(s32) -- 25 records in both Reset and Construct. Declared
//    at BrnTrafficEntityModule.h:214 and bodied nowhere. THIS IS THE LIVE INSTANCE OF
//    RECURRING-BUG CLASS (a): the record embeds EventQueue<DetachedPartRenderEvent,20>
//    mDetachedPartQueue, which therefore has NO Construct on this build."
//
// BOTH HALVES OF THAT PARK TURN OUT TO BE WRONG, and the second one matters:
//
//   (1) THE X360 BODY EXISTS AND IS NOT A CATCH-ALL MISATTRIBUTION. It is at 0x82751E88 --
//       progress/identity.json lists it under `BrnTraffic::TrafficPhysicsInfo::Construct`
//       with has_pseudocode true, and .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82751E88.json
//       is a real 14-instruction leaf. It sits immediately before Destruct @0x82751EE8 and
//       StaticTrafficParam::Construct @0x82751EF8, i.e. in the middle of the traffic
//       constructor run. Three callers xref it: TrafficEntityModule::Construct @0x82740220,
//       TrafficEntityModule::Reset @0x8272CDA0, and -- the one that pins its argument --
//       TrafficEntityModule::RecordTrafficVehicleIsPhysical @0x82720EC0.
//
//   (2) ⭐ THE CONSOLE'S Construct DOES NOT CALL EventQueue::Construct. It writes ONE BYTE of
//       zero at record +0x00 (`stb r11, 0(r3)` with r11 == 0) and nothing else in the queue's
//       span. So "the embedded EventQueue has no Construct on this build" is NOT a defect this
//       function was going to close -- the SHIPPED BINARY does not construct it here either,
//       and whoever binds mDetachedPartQueue.mpEvents does it somewhere else (the only
//       remaining candidates are Construct @0x82740220's own tail and the 102,800-byte
//       maTrafficPhysicsInfoList memset that precedes the 25 Constructs). Recurring bug class
//       (a) is therefore STILL OPEN for that queue and is NOT closed by landing this function.
//       Named again in this file's park list so it is not lost.
//
// SOURCES: X360 ARTIST asm+pseudocode (0x82751E88, 0x82751EE8, 0x82720EC0, 0x82708D48) for
// behaviour; DecFIGS DWARF (BrnTrafficEntityModule.h:156-:224) for shape. Feb-2007 has NO
// TrafficPhysicsInfo at all -- physical traffic is post-Feb code -- so rung 3 is empty here
// and rung 1 arbitrates alone.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

// (No <cstdlib> here: this file carries no BRN_TRAFFIC_DIAG probe -- its one bring-up
// diagnostic is the LogMissingLeg one-shot below, which needs only CgsLog. The sibling
// partfiles own the [T1-stream] / [T1-prepare] / [T1-scene] getenv probes.)

namespace BrnTraffic
{
namespace
{
    // Same PARTIAL-pattern one-shot leg gate the sibling partfiles in this directory use.
    // [DIAG] NOT IN THE X360 BINARY.
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
                << "[T1-phys] TrafficPhysicsInfo leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }
}

// ----------------------------------------------------------------------------
// BrnTraffic::TrafficPhysicsInfo::Construct  @ 0x82751E88   (14 insns)   *** PARTIAL ***
//
// DWARF :214  `void Construct(int32_t)`.
//
// Seeds one physical-traffic scratch record. The argument is the OWNING VEHICLE INDEX, not a
// slot index: RecordTrafficVehicleIsPhysical @0x82720EC0 picks a free bit out of
// maTrafficPhysicsInfoListBits, computes `v34 = 4112 * slot + this`, and calls
//     TrafficPhysicsInfo::Construct( v34 + 360976, a2 )
// where `a2` is its own `luVehicle` parameter (asserted `< 0x258` == KU_MAX_TOTAL_TRAFFIC ==
// 600 at BrnTrafficEntityModule.h:2459). Destruct @0x82751EE8 writes -1 to the same field, so
// it is a "which vehicle owns this slot, 0xFFFF for none" back-pointer.
//
// THE ASM, store for store (r3 == this, r4 == the vehicle index, r11 == 0,
// f0 == flt_82001CC0 == 0.0f -- PROVEN zero by wave T1 cluster C5 via
// DepthOfField::SetParams @0x821F1AC8, whose `lfBlurriness >= 0.0f` assert uses that exact
// address as its RHS; recurring bug class (c) checked and closed):
//
//   0x82751E9C  stfs f0, 0xFCC(r3)      4044   mfStuckTimeFront      = 0.0f
//   0x82751EA0  stfs f0, 0xFD0(r3)      4048   mfStuckTimeBack       = 0.0f
//   0x82751EA4  stfs f0, 0xFD4(r3)      4052   mfStuckTimerDebounce  = 0.0f
//   0x82751EB4  stfs f0, 0xFD8(r3)      4056   mfTimeNotDriving      = 0.0f
//   0x82751EA8  stfs f0, 0xFDC(r3)      4060   mfSteeringDirection   = 0.0f
//   0x82751EAC  stfs f0, 0xFE0(r3)      4064   mfDrivingDirection    = 0.0f
//   0x82751EB0  stb  r11, 0(r3)            0   <inside mDetachedPartQueue -- GATED, below>
//   0x82751EC0  stb  r11, 0xFE4(r3)     4068   miNumLightLocators    = 0
//   0x82751EB8  stb  r11, 0xFE5(r3)     4069   mbIsDeforming         = false
//   0x82751EBC  stb  r11, 0xFE6(r3)     4070   mbIsFatallyCrashing   = false
//   0x82751EC4  stb  r11, 0xFE7(r3)     4071   mu8RenderDamageFlags  = 0
//   0x82751ED0  stw  r11, 0(r10)++ x8   4072   mafGlassPaneFractureAmounts[0..7] = 0
//   0x82751EC8  stb  r11, 0x1008(r3)    4104   muContactSideFlags    = 0
//   0x82751EDC  sth  r4, 0x100A(r3)     4106   <the owner index -- GATED, below>
//
// THE TAIL MAPPING IS FORCED, not fitted. Working backwards from the attested stride
// (RecordTrafficVehicleIsPhysical's `4112 * v24`, and 463776 - 360976 == 102800 == 25 * 4112
// from UpdateSerialiser @0x8272DA80), the DWARF's declaration order over the last eight
// members tiles the last 68 bytes with ZERO slack, and TWO of the slots are independently
// pinned by a second function: UpdateVehicleStuckTimers @0x82708D48 calls
//     UpdateVehicleStuckSideTime( *(record + 4104), 1, ..., record + 4044, 2.0f, 0.1f )
//     UpdateVehicleStuckSideTime( *(record + 4104), 2, ..., record + 4048, 2.0f, 0.1f )
// i.e. +4104 IS muContactSideFlags and +4044/+4048 ARE mfStuckTimeFront/mfStuckTimeBack.
// RecordTrafficVehicleIsPhysical pins two more: right after this call it writes its two float
// arguments to record + 4060 and record + 4064 -- the steering/driving direction pair.
//
// ⚠️ NOT ZEROED, DELIBERATELY: mDetachedPartQueue's real contents, mvRoadTestNormal_
// HeightAboveRoad, maSkinningOffsets_Scratch, maWheelTransforms, maLightLocatorPositions,
// maLightTagPointTypes and mabWheelExists. The console leaves all of them alone (the record
// is bulk-cleared once, by its owner, before the 25 Constructs run). Do not "complete" this.
// ----------------------------------------------------------------------------
void TrafficPhysicsInfo::Construct( s32 liOwningVehicleIndex )
{

    {
        // GATED LEG 1 -- `stb r11, 0(r3)`, the single zero BYTE at record +0x00.
        //
        // Offset 0 is the first byte of mDetachedPartQueue, whose base
        // CgsModule::BaseEventQueue<T> lays out { T* mpEvents; s32 miMaxLength; s32 miLength; }
        // (DecFIGS CgsBaseEventQueue.h -- the same order for all seven instantiations dumped
        // there). On the big-endian X360 that byte is the MOST SIGNIFICANT byte of the 4-byte
        // mpEvents, which is not a member this tree can name: on the host mpEvents is 8 bytes,
        // so "the top byte of the pointer" is not even the same storage.
        //
        // TWO READINGS, and NEITHER is attestable from this function alone:
        //   (a) the shipped TrafficPhysicsInfo has a LEADING member the DecFIGS DWARF does not
        //       (physical traffic is post-Feb-2007, post-FIGS-merge-window code), and the byte
        //       is that member;
        //   (b) it really is a partial store into mpEvents.
        // (b) is very unlikely to be source-level intent, which makes (a) the live hypothesis
        // -- and (a) cannot be confirmed without a reader, which this cluster did not find.
        //
        // WRITING EITHER WOULD BE FABRICATION, and writing (b) would additionally corrupt an
        // 8-byte host pointer. Skipped, loudly.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "Construct's `stb 0, 0(this)` -- the one zero byte at record +0x00. It lands on the "
            "MOST SIGNIFICANT byte of the console's 4-byte mDetachedPartQueue.mpEvents, which on "
            "this LP64 host is not the same storage, and no reader was found that names a "
            "leading member the DecFIGS DWARF lacks. UNBLOCKED BY: one xref walk for a reader of "
            "TrafficPhysicsInfo +0x00..+0x03" );
    }

    // ---- the six timers / directions (0x82751E9C..0x82751EB4) ----------------------------
    mfStuckTimeFront      = 0.0f;
    mfStuckTimeBack       = 0.0f;
    mfStuckTimerDebounce  = 0.0f;
    mfTimeNotDriving      = 0.0f;
    mfSteeringDirection   = 0.0f;
    mfDrivingDirection    = 0.0f;

    // ---- the four state bytes (0x82751EB8..0x82751EC4) -----------------------------------
    miNumLightLocators    = 0;
    mbIsDeforming         = false;
    mbIsFatallyCrashing   = false;
    mu8RenderDamageFlags  = 0;

    // ---- the glass-pane fracture amounts (the `mtctr 8` loop @0x82751ED0) ----------------
    // The console stores integer zero with `stw`; same bit pattern, and the member is a float
    // array, so it is spelled as the float zero it is.
    for ( u32 luGlassPane = 0; luGlassPane < KU_NUM_GLASS_PANES; luGlassPane++ )
    {
        mafGlassPaneFractureAmounts[luGlassPane] = 0.0f;
    }

    // ---- the contact-side flags (0x82751EC8) ---------------------------------------------
    muContactSideFlags    = 0;

    // ---- the owner index (0x82751EDC `sth r4, 0x100A(r3)`) --------------------------------
    // ⭐⭐ UN-GATED 2026-08-21 (wave T1 round 3, closure item 4). R2C parked this store as a
    // LAYOUT gap, not an evidence gap: the evidence was already complete (Construct stores its
    // own s32 argument here as a HALFWORD; Destruct @0x82751EE8 is `li r11,-1 ; sth r11,
    // 0x100A(r3)` and nothing else; the caller asserts the argument < KU_MAX_TOTAL_TRAFFIC
    // (600); HandleExternalResponses @0x82732C68 is the reader) -- the blocker was that
    // BrnTrafficEntityModule.h had no member and was not that cluster's file.
    //
    // The member is now declared (BrnTrafficEntityModule.h, TrafficPhysicsInfo tail:
    // `u8 muPad205; u16 muOwningVehicleIndex;`), so the store is real. The console's `sth` of
    // a 32-bit register is the source-level narrowing of the s32 parameter to the u16 member;
    // it is spelled as an explicit cast rather than reproduced as a truncating store.
    //
    // ⚠️ ORDER NOTE FOR ANY FUTURE READER: this store had to land BEFORE the 25 call sites in
    // BrnTrafficEntityModule_wT1_01.cpp were un-gated. With the store missing, every record
    // would read owner index 0 (zero-initialised storage) instead of "no owner", i.e. vehicle
    // 0 would appear to own all 25 physical slots. Header first, body second, call sites last
    // -- that is the order this round used.
    muOwningVehicleIndex = static_cast< u16 >( liOwningVehicleIndex );
}

// ----------------------------------------------------------------------------
// TrafficPhysicsInfo::Destruct  @ 0x82751EE8   *** COMPLETE ***
//
// The console body is two instructions and a return:
//     0x82751EE8  li   r11, -1
//     0x82751EEC  sth  r11, 0x100A(r3)
// i.e. the entire teardown of a physical-traffic record is "mark the slot unowned". Nothing
// else in the 4112-byte record is touched -- the queue, the transforms, the timers and the
// glass-pane amounts are all left as they are, because the record is bulk-reused rather than
// released. Reproduced exactly; the 0xFFFF sentinel is spelled through the named constant on
// the declaration (KU16_NO_OWNING_VEHICLE) instead of the console's sign-extended -1, which
// is the same 16 bits.
//
// DWARF :218 attests the declaration (`void Destruct();`); the body is rung-1 only.
// ----------------------------------------------------------------------------
void TrafficPhysicsInfo::Destruct()
{
    muOwningVehicleIndex = KU16_NO_OWNING_VEHICLE;
}

}
