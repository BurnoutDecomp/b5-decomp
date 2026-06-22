// Embed check for CgsResource::MassiveLookupTableResourceType::GetSerialisedResourceDescriptor:
//   CgsResource::MassiveLookupTableResourceType::GetSerialisedResourceDescriptor @ 0x8267D818
//
// The bodied func is invoked through an explicit qualified (non-virtual) call on a
// placement reference; sibling virtuals are owned by their own recon passes.

#include "SharedClasses/Massive/Resources/MassiveLookupTableResourceType.h"
#include "rw/rwcore_structs.h"
#include "types.hpp"

using namespace CgsResource;

static_assert(sizeof(ResourceDescriptor) == 40, "X360 serialised descriptor is 5*(size,align)");

void ExerciseMassiveLookupTableDesc(const MassiveLookupTableResourceType& lrType, const void* lpResource)
{
    ResourceDescriptor lDesc = lrType.MassiveLookupTableResourceType::GetSerialisedResourceDescriptor(lpResource);
    (void)lDesc;
}
