#ifndef BRN_STATIC_SOUND_MAP_RESOURCE_TYPE_H
#define BRN_STATIC_SOUND_MAP_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnSound
{
namespace World
{
// Resource-type handler for the StaticSoundMap resource (registry id 0x10016).
// Only GetTypeID + FixUp are defined in this TU; the other overrides
// (GetSerialisedResourceDescriptor / GetImportCount / GetImportPointer) are
// separate TUs and are NOT declared here — the base virtuals are non-pure, so
// this class stays concrete with just these two.
struct StaticSoundMapResourceType : public CgsResource::Type
{
    uint32_t GetTypeID() const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};

}
}

#endif
