#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTrigger
{

// The X360 pointer table begins at 0x820A3A18 and contains 32 big-endian pointers. Each string
// below was read from the corresponding raw image pointer; capitalization, spaces, and the
// original "LargeOverheadobject" spelling are preserved. GenericRegion::GetTypeName @0x82354760
// indexes this table by meType.
const char* GenericRegion::KAPC_GENERIC_REGION_TYPE_STRINGS[GenericRegion::E_TYPE_COUNT] =
{
    "Junkyard",                  // 0  E_TYPE_JUNK_YARD
    "GasStation",                // 1  E_TYPE_GAS_STATION
    "BodyShop",                  // 2  E_TYPE_BODY_SHOP
    "PaintShop",                 // 3  E_TYPE_PAINT_SHOP
    "CarPark",                   // 4  E_TYPE_CAR_PARK
    "SignatureTakedown",         // 5  E_TYPE_SIGNATURE_TAKEDOWN
    "KillzoneTrigger",            // 6  E_TYPE_KILLZONE
    "Stunt",                     // 7  E_TYPE_JUMP
    "StuntProp",                 // 8  E_TYPE_SMASH
    "SignatureCrash",            // 9  E_TYPE_SIGNATURE_CRASH
    "SignatureCrashCamera",      // 10 E_TYPE_SIGNATURE_CRASH_CAMERA
    "RoadLimit",                 // 11 E_TYPE_ROAD_LIMIT
    "OverDriveBoost",            // 12 E_TYPE_OVERDRIVE_BOOST
    "OverDriveStrength",         // 13 E_TYPE_OVERDRIVE_STRENGTH
    "OverDriveSpeed",            // 14 E_TYPE_OVERDRIVE_SPEED
    "OverDriveControl",          // 15 E_TYPE_OVERDRIVE_CONTROL
    "TireShop",                  // 16 E_TYPE_TIRE_SHOP
    "TuningShop",                // 17 E_TYPE_TUNING_SHOP
    "PictureParadise",           // 18 E_TYPE_PICTURE_PARADISE
    "Tunnel",                    // 19 E_TYPE_TUNNEL
    "Overpass",                  // 20 E_TYPE_OVERPASS
    "Bridge",                    // 21 E_TYPE_BRIDGE
    "Warehouse",                 // 22 E_TYPE_WAREHOUSE
    "LargeOverheadobject",       // 23 E_TYPE_LARGE_OVERHEAD_OBJECT
    "NarrowAlley",               // 24 E_TYPE_NARROW_ALLEY
    "Pass Tunnel",               // 25 E_TYPE_PASS_TUNNEL
    "Pass Overpass",             // 26 E_TYPE_PASS_OVERPASS
    "Pass Bridge",               // 27 E_TYPE_PASS_BRIDGE
    "Pass Warehouse",            // 28 E_TYPE_PASS_WAREHOUSE
    "Pass LargeOverheadobject",  // 29 E_TYPE_PASS_LARGEOVERHEADOBJECT
    "Pass NarrowAlley",          // 30 E_TYPE_PASS_NARROWALLEY
    "Ramp",                      // 31 E_TYPE_RAMP
};

// X360 0x82354760. Maps the region's type enum to its human-readable name via
// the static KAPC_GENERIC_REGION_TYPE_STRINGS table. Asserts the type index is in
// range first (BrnGenericRegion.h:231: meType < E_TYPE_COUNT).
const char*
GenericRegion::GetTypeName() const
{
    CGS_ASSERT( meType < E_TYPE_COUNT, "meType < E_TYPE_COUNT" );
    return KAPC_GENERIC_REGION_TYPE_STRINGS[meType];
}

// ============================================================================
// ⭐ [drive-thru wave 2026-08-27] IsDriveThru() -- BODIED, and the header's FLAG is retired.
//
// That FLAG parked this deliberately: "the console tests meType against the drive-thru type SET,
// and that set is not attested by anything in this wave's evidence ... Guessing
// `meType <= E_TYPE_CAR_PARK` would be a fabrication ... parked for the owner of the
// DriveThruManager TU." This wave owns that TU, and the set IS attested -- the console inlines
// the test into DriveThruManager::Prepare @0x8236E3C0, right after its "lpGenericRegion != NULL"
// assert (cpp:213):
//     v13 = *(v12 + 54);                      // meType @ +0x36
//     if ( v13 == 3 || v13 == 2 || !*(v12 + 54) || v13 == 1
//          || (v15 = v13 != 4, v14 = 0, !v15) )
//         v14 = 1;
// i.e. an explicit five-way equality test against 3, 2, 0, 1 and 4 -- exactly
// PAINT_SHOP / BODY_SHOP / JUNK_YARD / GAS_STATION / CAR_PARK, and nothing else.
//
// ⓘ Written as the five-way test the compiler emitted, NOT as the `meType <= E_TYPE_CAR_PARK`
// range compare it happens to be equivalent to today. The two are only equivalent because those
// five enumerators are currently 0..4 and contiguous; a range compare would silently absorb any
// future enumerator that lands inside the range, which is precisely the fabrication the FLAG
// warned about. The set is the console's; keep it a set.
// ⓘ Independently corroborated by the consumer: TriggerQueryManager::ProcessPlayerTriggers
// @0x8239BF80 routes exactly these five case labels to DriveThruManager::HandleDriveThru.
// ============================================================================
bool
GenericRegion::IsDriveThru() const
{
    const Type leType = GetType();
    return leType == E_TYPE_JUNK_YARD
        || leType == E_TYPE_GAS_STATION
        || leType == E_TYPE_BODY_SHOP
        || leType == E_TYPE_PAINT_SHOP
        || leType == E_TYPE_CAR_PARK;
}

} // namespace BrnTrigger
