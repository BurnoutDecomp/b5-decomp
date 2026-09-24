#ifndef ATTRIBSYS_ENUMS_E_MATERIAL_TYPE_H
#define ATTRIBSYS_ENUMS_E_MATERIAL_TYPE_H

namespace AttribSys
{
namespace Enums
{
namespace eMaterialType
{

// DecFIGS eMaterialType.h:12. Authored flag values, one bit per collision-sound material.
// The collision sound logic ORs them into InputCollision::maMaterial and the crash bins
// (crashbin mMaterialA/B, via BinLookupCache) test them with bitwise ANDs. A 32-bit enum:
// every ARTIST builder widens it to the u64 material with `extsw` (e.g. 0x826D3BA4).
enum eMaterialType
{
    Nothing          = 1,
    PlayerCar        = 2,
    AiCar            = 4,
    TrafficCar       = 8,
    World            = 16,
    BodyPartSmall    = 32,
    BodyPartLarge    = 64,
    Mirrors          = 128,
    GlassSmall       = 256,
    GlassLarge       = 512,
    Wheels           = 1024,
    Lights           = 2048,
    Crane            = 4096,
    Mixer            = 8192,
    Tipper           = 16384,
    NumberPlate      = 32768,
    Seats            = 65536,
    RoofRacks        = 131072,
    Extinguisher     = 262144,
    Ladder           = 524288,
    Body             = 1048576,
    Suspension       = 2097152,
    Exhaust          = 4194304,
    TrafficCarMedium = 8388608,
    TrafficCarLarge  = 16777216,
};

const int KI_NUM_ENUMS = 25;          // eMaterialType.h:40
const int KI_MAX_VALUE = 16777216;    // eMaterialType.h:41

} // namespace eMaterialType
} // namespace Enums
} // namespace AttribSys

#endif // ATTRIBSYS_ENUMS_E_MATERIAL_TYPE_H
