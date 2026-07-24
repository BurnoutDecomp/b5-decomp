#ifndef BRN_WORLD_SHADER_LOD_INFO_H
#define BRN_WORLD_SHADER_LOD_INFO_H

// =============================================================================
// BrnWorld::ShaderLodInfo -- GameSource/World/BrnShaderLodInfo.h (DWARF home)
//
// Declaration shape from the DecFIGS DWARF (BrnShaderLodInfo.h:40); behaviour of
// the two techniques queries from their X360 inline expansions inside
// BrnWorld::WorldEntityModule::GenerateDispatchLists[ForEnvironmentMap]
// (@0x822D5AB0 / @0x822D7298): the world shader technique is picked by the
// camera's distance to the (radius-clamped) LOD-0 bounding sphere against the
// shader-LOD-1 near distance. Construct/Update (DWARF :43/:46) are the owning
// world-module TU's work and are not bodied here.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                       // Vector3/Vector3Plus/Matrix44Affine/VecFloat
#include "rw/math/vpu/vector3_operation.h"       // operator- / Magnitude
#include "rw/math/vpu/matrix44affine_operation.h" // TransformPoint

namespace BrnWorld
{

// DWARF BrnShaderLodInfo.h:27.
enum WorldShaderTechnique
{
    E_TECHNIQUE_INVALID = -1,
    E_TECHNIQUE_LOD0    = 0,
    E_TECHNIQUE_ZONLY   = 1,
    E_TECHNIQUE_LOD1    = 2
};

// DWARF BrnShaderLodInfo.h:40.
struct ShaderLodInfo
{
    void Construct();   // :43 (owning TU)
    void Update();      // :46 (owning TU)

    // :52 -- X360 inline expansion (GenerateDispatchLists @0x822D5AB0):
    // forced override first; otherwise LOD1 when the camera is beyond the
    // shader-LOD-1 near distance from the clamped bounding sphere.
    WorldShaderTechnique GetLodTechnique(
        Vector3Plus lCentreAndRadius,
        const Matrix44Affine& lrTransform,
        Vector3 lCameraPosition ) const
    {
        if ( miOverrideTechnique != E_TECHNIQUE_INVALID )
        {
            return static_cast<WorldShaderTechnique>( miOverrideTechnique );
        }

        if ( !mbUseShaderLod )
        {
            return E_TECHNIQUE_LOD0;
        }

        Vector3 lCentre =
            rw::math::vpu::TransformPoint( lrTransform, lCentreAndRadius.GetVector3() );

        f32 lfRadius = lCentreAndRadius.GetPlus();
        if ( lfRadius > mMaxBelievableRadius.x )
        {
            lfRadius = mMaxBelievableRadius.x;
        }

        const f32 lfDistance = rw::math::vpu::Magnitude( lCentre - lCameraPosition );

        return ( lfDistance - lfRadius >= mShaderLod1NearDistance.x )
                   ? E_TECHNIQUE_LOD1
                   : E_TECHNIQUE_LOD0;
    }

    // :55.
    WorldShaderTechnique GetEnvMapTechnique() const
    {
        return static_cast<WorldShaderTechnique>( miEnvMapTechnique );
    }

    // --- members (DWARF order) ----------------------------------------------
    // The two VecFloat members are broadcast SIMD scalars on the console; on PC
    // the scalar lives in every lane (readers use lane x).
    VecFloat               mShaderLod1NearDistance;   // :57
    VecFloat               mMaxBelievableRadius;      // :58
    f32                    mfShaderLod1NearDistance;  // :59
    s32                    miEnvMapTechnique;         // :60
    s32                    miOverrideTechnique;       // :61
    bool                   mbUseShaderLod;            // :62
};

} // namespace BrnWorld

#endif // BRN_WORLD_SHADER_LOD_INFO_H
