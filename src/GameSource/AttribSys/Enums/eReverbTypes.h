#ifndef ATTRIBSYS_ENUMS_E_REVERB_TYPES_H
#define ATTRIBSYS_ENUMS_E_REVERB_TYPES_H

namespace AttribSys
{
namespace Enums
{
namespace eReverbTypes
{

// The environmental reverb presets. The first six follow the sound-enclosure region
// types; the image's own name table spells every value.
enum eReverbTypes
{
    ReverbTypeNone                = 0,
    ReverbTypeTunnel              = 1,
    ReverbTypeOverpass            = 2,
    ReverbTypeBridge              = 3,
    ReverbTypeWarehouse           = 4,
    ReverbTypeLargeOverheadObject = 5,
    ReverbTypeNarrowAlley         = 6,
    ReverbTypeUrban               = 7,
    ReverbTypeRural               = 8,
    ReverbTypeImpactTime          = 9,
    ReverbTypeSuperSloMo          = 10,
    ReverbTypeCount               = 11,
};

const int KI_NUM_ENUMS = 12;

} // namespace eReverbTypes
} // namespace Enums
} // namespace AttribSys

#endif // ATTRIBSYS_ENUMS_E_REVERB_TYPES_H
