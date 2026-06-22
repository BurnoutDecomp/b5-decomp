// Embed check for CgsResource::PlayerCarColoursResourceType Serialise/FixUp/FixDown:
//   CgsResource::PlayerCarColoursResourceType::FixDown   @ 0x8267E090
//   CgsResource::PlayerCarColoursResourceType::FixUp     @ 0x8267E058
//   CgsResource::PlayerCarColoursResourceType::Serialise @ 0x8267DF80
//
// GetTypeID/GetSerialisedResourceDescriptor are sibling virtuals owned by other
// recon passes; the type is NOT instantiated. The bodied funcs are invoked via
// explicit qualified (non-virtual) calls on a placement reference.

#include "SharedClasses/Graphics/PlayerCarColoursResourceType.h"
#include "rw/rwcore_structs.h"

using namespace CgsResource;

void ExercisePlayerCarColours(const PlayerCarColoursResourceType& lrType, void* lpResource,
                              const rw::Resource& lrRes, const ResourceDescriptor& lrDesc)
{
    lrType.PlayerCarColoursResourceType::FixDown(lpResource, lrRes);
    lrType.PlayerCarColoursResourceType::FixUp(lpResource, lrRes);
    (void)lrType.PlayerCarColoursResourceType::Serialise(lpResource, lrRes, lrDesc);
}
