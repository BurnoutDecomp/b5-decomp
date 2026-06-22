#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"

// BrnPhysics::Deformation::StreamedDeformationSpec accessors.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Both are checked array element accessors;
// the asserts are non-gating tripwires (execution continues past a failed assert and the
// element address is returned regardless), matching the X360 control flow exactly.
namespace BrnPhysics
{
namespace Deformation
{
    // X360 @ 0x825B32D8 (Hex-Rays emitted the symbol with an empty short-name; identified as
    // GetGlassPaneSpec from BrnStreamedDeformationSpec.h:230 -- stride 112 == sizeof(GlassPaneSpec),
    // bound miNumGlassPanes, asserts at source lines 397/398).
    //   asm: cmpw liIndex, miNumGlassPanes (@+32); blt skip assert "liIndex < miNumGlassPanes" (:18D=397)
    //        cmpwi liIndex, 0; bge skip assert "liIndex >= 0" (:18E=398)
    //        result = 112 * liIndex + maGlassPaneData (@+28)
    const GlassPaneSpec* StreamedDeformationSpec::GetGlassPaneSpec(s32 liIndex) const
    {
        CGS_ASSERT(liIndex < miNumGlassPanes, "liIndex < miNumGlassPanes");   // BrnStreamedDeformationSpec.h:397
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");                             // BrnStreamedDeformationSpec.h:398
        return &maGlassPaneData[liIndex];
    }

    // X360 @ 0x822A0328 (GetWheelSpec, BrnStreamedDeformationSpec.h:256). eNumWheels == 4.
    //   asm: cmpwi liWheel, 4; blt skip assert "liWheel < eNumWheels" (:101=257)
    //        result = 48 * liWheel + this + 80   (&maWheelSpecs[liWheel])
    const WheelSpec* StreamedDeformationSpec::GetWheelSpec(s32 liWheel) const
    {
        CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");   // BrnStreamedDeformationSpec.h:257
        return &maWheelSpecs[liWheel];
    }
}
}
