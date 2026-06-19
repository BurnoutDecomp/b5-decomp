#include "SharedClasses/World/BrnWheelGraphicsSpecResourceType.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWheel
{

// The wheel graphics-spec resource layout is not yet committed.
// FLAG: minimal flagged slice — only the one field this FixUp touches is
// modelled. miVersion (dword 0) is the on-disk graphics-spec version; the
// remainder of the GraphicsSpec layout is deferred.
struct GraphicsSpec
{
    s32 miVersion;   // +0   serialised wheel graphics-spec version
};

// FLAG: value 1 taken from the X360 `*a2 != 1` version check.
static const s32 KI_WHEEL_GRAPHICS_SPEC_VERSION = 1;

// Resource registry type id for the wheel graphics-spec resource (0x1000A).
// Recovered verbatim from GetTypeID @ 0x82676490.
static const uint32_t KU_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_ID = 65546;

uint32_t GraphicsSpecResourceType::GetTypeID() const
{
    return KU_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_ID;
}

// FixUp @ 0x82678CF0. The X360 signature is FixUp(this, lpResource); the
// rw::Resource& is part of the virtual contract but this handler performs no
// relocation (version-check only — no field clear, no pointer fix-up), so
// lrResource is unused here.
void GraphicsSpecResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    (void)lrResource;

    const GraphicsSpec* lpGraphicsSpec = static_cast<const GraphicsSpec*>(lpResource);

    CGS_ASSERT(lpGraphicsSpec->miVersion == KI_WHEEL_GRAPHICS_SPEC_VERSION,
               "Incorrect version for wheel Graphics Spec.  Get latest code/tools and rebuild data.  If that doesn't work, grab Keef! \n");
}

}
