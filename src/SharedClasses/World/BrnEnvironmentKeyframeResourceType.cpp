#include "SharedClasses/World/BrnEnvironmentKeyframeResourceType.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{
namespace EnvironmentSettings
{

// The environment-settings keyframe resource layout is not yet committed.
// FLAG: minimal flagged slice — only the two fields this FixUp touches are
// modelled. miVersion (dword 0) is the on-disk keyframe version; muFixUpField
// (byte offset 128 == dword index 32) is a runtime field the loader clears.
// The intervening 124 bytes and the meaning of muFixUpField are deferred.
struct KeyframeResource
{
    s32 miVersion;        // +0   serialised keyframe version
    u8  maReserved[124];  // +4 .. +127  opaque (deferred layout)
    u32 muFixUpField;     // +128 runtime field, cleared on FixUp
};

// FLAG: value 8 taken from the X360 `*a2 == 8` version check.
static const s32 KI_KEYFRAME_VERSION = 8;

// Resource registry type id for the environment-settings keyframe resource
// (0x10012). Recovered verbatim from GetTypeID @ 0x82676380.
uint32_t KeyframeResourceType::GetTypeID() const
{
    return 65554;
}

// FixUp @ 0x82678C40. The X360 signature is FixUp(this, lpResource); the
// rw::Resource& is part of the virtual contract but this handler performs no
// relocation, so lrResource is unused here.
// Behaviour: assert the on-disk version matches the current keyframe version,
// then unconditionally clear the runtime field at +128 (a2[32] = 0 runs in
// BOTH the matching and the failed-assert X360 branches).
void KeyframeResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    (void)lrResource;

    KeyframeResource* lpKeyframe = static_cast<KeyframeResource*>(lpResource);

    CGS_ASSERT(lpKeyframe->miVersion == KI_KEYFRAME_VERSION,
               "Incorrect version for Environment Settings Keyframe; get latest code/tools and rebuild data \n");

    lpKeyframe->muFixUpField = 0;
}

}
}
