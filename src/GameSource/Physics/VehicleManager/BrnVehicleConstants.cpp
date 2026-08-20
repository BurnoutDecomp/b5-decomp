#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // Breaker image/static initialisers:
    //   byte_82FB7DF0 = 0; dword_82F2A10C = 20;
    //   sub_82C5A4C0/sub_82C5A7D8/sub_82C5AAF0 splat flt_82001CC0 (0.0f)
    //   through all 32 roughness/grip/linear-drag entries.
    bool gbReadSurfaceProperties = false;
    s32  KI_NUM_USED_SURFACES = 20;

    VecFloat KAVF_SURFACE_ROUGHNESS[KI_MAX_NUM_SURFACES] = {};
    VecFloat KAVF_SURFACE_GRIP[KI_MAX_NUM_SURFACES] = {};
    VecFloat KAVF_SURFACE_LINEAR_DRAG[KI_MAX_NUM_SURFACES] = {};
    bool KAB_SURFACE_IS_WATER[KI_MAX_NUM_SURFACES] = {};
}
}
