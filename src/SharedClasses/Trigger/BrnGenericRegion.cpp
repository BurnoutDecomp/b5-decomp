#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTrigger
{

// off_820A3A18 in the X360 image: the static const char*[E_TYPE_COUNT] table GetTypeName()
// indexes with meType. The X360 string CONTENTS were not recovered by this TU's dossier; the
// names below are a provisional, enum-aligned best-effort (FLAG for the conductor: replace with
// the verbatim X360 strings when recovered). The table SIZE (E_TYPE_COUNT == 32) and the index
// path are authoritative and load-bearing; the literal text is not.
const char* GenericRegion::KAPC_GENERIC_REGION_TYPE_STRINGS[GenericRegion::E_TYPE_COUNT] =
{
    "JunkYard",                  // 0  E_TYPE_JUNK_YARD
    "GasStation",                // 1  E_TYPE_GAS_STATION
    "BodyShop",                  // 2  E_TYPE_BODY_SHOP
    "PaintShop",                 // 3  E_TYPE_PAINT_SHOP
    "CarPark",                   // 4  E_TYPE_CAR_PARK
    "SignatureTakedown",         // 5  E_TYPE_SIGNATURE_TAKEDOWN
    "Killzone",                  // 6  E_TYPE_KILLZONE
    "Jump",                      // 7  E_TYPE_JUMP
    "Smash",                     // 8  E_TYPE_SMASH
    "SignatureCrash",            // 9  E_TYPE_SIGNATURE_CRASH
    "SignatureCrashCamera",      // 10 E_TYPE_SIGNATURE_CRASH_CAMERA
    "RoadLimit",                 // 11 E_TYPE_ROAD_LIMIT
    "OverdriveBoost",            // 12 E_TYPE_OVERDRIVE_BOOST
    "OverdriveStrength",         // 13 E_TYPE_OVERDRIVE_STRENGTH
    "OverdriveSpeed",            // 14 E_TYPE_OVERDRIVE_SPEED
    "OverdriveControl",          // 15 E_TYPE_OVERDRIVE_CONTROL
    "TireShop",                  // 16 E_TYPE_TIRE_SHOP
    "TuningShop",                // 17 E_TYPE_TUNING_SHOP
    "PictureParadise",           // 18 E_TYPE_PICTURE_PARADISE
    "Tunnel",                    // 19 E_TYPE_TUNNEL
    "Overpass",                  // 20 E_TYPE_OVERPASS
    "Bridge",                    // 21 E_TYPE_BRIDGE
    "Warehouse",                 // 22 E_TYPE_WAREHOUSE
    "LargeOverheadObject",       // 23 E_TYPE_LARGE_OVERHEAD_OBJECT
    "NarrowAlley",               // 24 E_TYPE_NARROW_ALLEY
    "PassTunnel",                // 25 E_TYPE_PASS_TUNNEL
    "PassOverpass",              // 26 E_TYPE_PASS_OVERPASS
    "PassBridge",                // 27 E_TYPE_PASS_BRIDGE
    "PassWarehouse",             // 28 E_TYPE_PASS_WAREHOUSE
    "PassLargeOverheadObject",   // 29 E_TYPE_PASS_LARGEOVERHEADOBJECT
    "PassNarrowAlley",           // 30 E_TYPE_PASS_NARROWALLEY
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

} // namespace BrnTrigger
