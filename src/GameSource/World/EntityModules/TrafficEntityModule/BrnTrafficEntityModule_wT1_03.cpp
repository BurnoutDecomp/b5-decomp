// ============================================================================
// BrnTrafficEntityModule_wT1_03.cpp -- part of the BrnTrafficEntityModule.cpp ledger TU.
//
//   * BrnTraffic::TrafficPhysicsInfo::Construct @0x82751E88 (14 insns) PARTIAL
//   * BrnTraffic::TrafficPhysicsInfo::Destruct  @0x82751EE8 (2 insns)  COMPLETE
//
// OPEN PARK: nothing found so far binds mDetachedPartQueue.mpEvents. The console's
// Construct writes one zero byte inside the queue's span and nothing else, so the
// embedded EventQueue<DetachedPartRenderEvent,20> is constructed elsewhere if at all.
// Candidates: TrafficEntityModule::Construct @0x82740220's tail, and the 102,800-byte
// maTrafficPhysicsInfoList memset that runs before the 25 Construct calls.
//
// Shape from DecFIGS DWARF (BrnTrafficEntityModule.h:156-:224). Feb-2007 has no
// TrafficPhysicsInfo, so the X360 asm arbitrates behaviour alone.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

namespace BrnTraffic
{
namespace
{
    // One-shot leg gate, same pattern as the sibling partfiles. [DIAG] NOT IN THE X360 BINARY.
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
// BrnTraffic::TrafficPhysicsInfo::Construct  @ 0x82751E88  (14 insns)  PARTIAL
// DWARF BrnTrafficEntityModule.h:214  `void Construct(int32_t)`.
//
// Seeds one physical-traffic scratch record. The argument is the OWNING VEHICLE INDEX, not a
// slot index: RecordTrafficVehicleIsPhysical @0x82720EC0 picks a free bit out of
// maTrafficPhysicsInfoListBits, computes `record = this + 360976 + 4112 * slot`, and passes
// its own `luVehicle` (asserted < KU_MAX_TOTAL_TRAFFIC == 600). Destruct writes -1 to the
// same field, so it is a "which vehicle owns this slot, 0xFFFF for none" back-pointer.
//
// LAYOUT ATTESTATION -- the stores, with the offsets the C++ cannot show
// (r3 == this, r4 == the vehicle index, r11 == 0, f0 == flt_82001CC0 == 0.0f):
//
//   0x82751E9C  stfs f0, 0xFCC(r3)      4044   mfStuckTimeFront      = 0.0f
//   0x82751EA0  stfs f0, 0xFD0(r3)      4048   mfStuckTimeBack       = 0.0f
//   0x82751EA4  stfs f0, 0xFD4(r3)      4052   mfStuckTimerDebounce  = 0.0f
//   0x82751EB4  stfs f0, 0xFD8(r3)      4056   mfTimeNotDriving      = 0.0f
//   0x82751EA8  stfs f0, 0xFDC(r3)      4060   mfSteeringDirection   = 0.0f
//   0x82751EAC  stfs f0, 0xFE0(r3)      4064   mfDrivingDirection    = 0.0f
//   0x82751EB0  stb  r11, 0(r3)            0   mDetachedPartQueue.muNumParts = 0
//   0x82751EC0  stb  r11, 0xFE4(r3)     4068   miNumLightLocators    = 0
//   0x82751EB8  stb  r11, 0xFE5(r3)     4069   mbIsDeforming         = false
//   0x82751EBC  stb  r11, 0xFE6(r3)     4070   mbIsFatallyCrashing   = false
//   0x82751EC4  stb  r11, 0xFE7(r3)     4071   mu8RenderDamageFlags  = 0
//   0x82751ED0  stw  r11, 0(r10)++ x8   4072   mafGlassPaneFractureAmounts[0..7] = 0
//   0x82751EC8  stb  r11, 0x1008(r3)    4104   muContactSideFlags    = 0
//   0x82751EDC  sth  r4, 0x100A(r3)     4106   <the owner index -- GATED, below>
//
// The record stride is 4112 bytes (RecordTrafficVehicleIsPhysical, and 463776 - 360976 ==
// 102800 == 25 * 4112 from UpdateSerialiser @0x8272DA80); the DWARF order over the last eight
// members tiles the last 68 bytes with no slack. Four slots are pinned independently:
// UpdateVehicleStuckTimers @0x82708D48 passes +4104 as the contact-side flags and +4044/+4048
// as the front/back stuck times, and RecordTrafficVehicleIsPhysical writes its two float
// arguments to +4060 and +4064 (the steering/driving direction pair).
//
// NOT ZEROED, DELIBERATELY: mDetachedPartQueue's contents, mvRoadTestNormal_HeightAboveRoad,
// maSkinningOffsets_Scratch, maWheelTransforms, maLightLocatorPositions, maLightTagPointTypes,
// mabWheelExists. The owner bulk-clears the record once before the 25 Constructs run, so the
// console leaves them alone. Do not "complete" this.
// ----------------------------------------------------------------------------
void TrafficPhysicsInfo::Construct( s32 liOwningVehicleIndex )
{
    // `stb r11, 0(r3)` @0x82751EB0 -- the detached-part record COUNT. RESOLVED 2026-09-02
    // (traffic-deformation wave): the xref walk the old gate asked for found the readers and
    // the writer. ProcessDeformationData @0x8271DEB0 uses this byte as the slot index
    // (`lbz r29, 0(info)`, asserted < KU_MAX_DETACHED_PARTS_PER_VEHICLE), zeroes it for all
    // 25 records every frame @0x8271E984, and `stb ++count` @0x8271EB10; RenderTrafficCar
    // reads it as the record count (pseudocode :1319). It was never the top byte of an
    // EventQueue pointer -- ARTIST's record is the compact struct the header now declares.
    mDetachedPartQueue.muNumParts = 0;

    mfStuckTimeFront      = 0.0f;
    mfStuckTimeBack       = 0.0f;
    mfStuckTimerDebounce  = 0.0f;
    mfTimeNotDriving      = 0.0f;
    mfSteeringDirection   = 0.0f;
    mfDrivingDirection    = 0.0f;

    miNumLightLocators    = 0;
    mbIsDeforming         = false;
    mbIsFatallyCrashing   = false;
    mu8RenderDamageFlags  = 0;

    // The `mtctr 8` loop @0x82751ED0 stores integer zero with `stw`; same bit pattern as the
    // float zero this member array takes.
    for ( u32 luGlassPane = 0; luGlassPane < KU_NUM_GLASS_PANES; luGlassPane++ )
    {
        mafGlassPaneFractureAmounts[luGlassPane] = 0.0f;
    }

    muContactSideFlags    = 0;

    // `sth r4, 0x100A(r3)` @0x82751EDC narrows the s32 argument to the u16 member; spelled as
    // an explicit cast rather than a truncating store. Reader: HandleExternalResponses
    // @0x82732C68.
    muOwningVehicleIndex = static_cast< u16 >( liOwningVehicleIndex );
}

// ----------------------------------------------------------------------------
// TrafficPhysicsInfo::Destruct  @ 0x82751EE8  COMPLETE
// DWARF BrnTrafficEntityModule.h:218  `void Destruct();`.
//
// The whole console body is `li r11, -1 ; sth r11, 0x100A(r3)`: teardown just marks the slot
// unowned. Nothing else in the 4112-byte record is touched, because the record is reused
// rather than released. KU16_NO_OWNING_VEHICLE is the same 16 bits as the console's -1.
// ----------------------------------------------------------------------------
void TrafficPhysicsInfo::Destruct()
{
    muOwningVehicleIndex = KU16_NO_OWNING_VEHICLE;
}

}
