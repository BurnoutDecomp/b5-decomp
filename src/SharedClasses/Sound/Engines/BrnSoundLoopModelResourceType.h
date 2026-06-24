#ifndef BRN_SOUND_LOOP_MODEL_RESOURCE_TYPE_H
#define BRN_SOUND_LOOP_MODEL_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
// LoopModelData (header + relocation walkers) now lives in its DWARF home; the
// previous inline forward-stub here was promoted there. The FixUp(int)/FixDown(int)
// worker signatures the wrapper below calls are preserved unchanged.
#include "SharedClasses/Sound/Engines/BrnSoundLoopModelData.h"

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{
class LoopModelResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}
}
}

#endif
