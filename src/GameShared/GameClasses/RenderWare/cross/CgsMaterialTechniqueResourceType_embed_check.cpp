// Embed check for CgsResource::MaterialTechniqueResourceType::GetSerialisedResourceDescriptor:
//   CgsResource::MaterialTechniqueResourceType::GetSerialisedResourceDescriptor @ 0x828A97D8
//
// The bodied func is invoked through an explicit qualified (non-virtual) call on a
// placement reference; sibling virtuals are owned by their own recon passes.

#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.h"
#include "rw/rwcore_structs.h"
#include "types.hpp"

using namespace CgsResource;

static_assert(sizeof(ResourceDescriptor) == 40, "X360 serialised descriptor is 5*(size,align)");

void ExerciseMaterialTechniqueDesc(const MaterialTechniqueResourceType& lrType, const void* lpResource)
{
    ResourceDescriptor lDesc = lrType.MaterialTechniqueResourceType::GetSerialisedResourceDescriptor(lpResource);
    (void)lDesc;
}
