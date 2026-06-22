// Embed check for CgsResource::IdListResourceType FixDown/FixUp:
//   CgsResource::IdListResourceType::FixDown @ 0x828F75D8
//   CgsResource::IdListResourceType::FixUp   @ 0x828F75F8
//
// GetTypeID/GetSerialisedResourceDescriptor are sibling virtuals owned by other
// recon passes, so the type is NOT instantiated (its vtable is incomplete).
// The bodied fixups are invoked through explicit qualified (non-virtual) calls on
// a placement reference, so no vtable is required.

#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include "rw/rwcore_structs.h"

using namespace CgsResource;

void ExerciseIdListFix(const IdListResourceType& lrType, void* lpResource, const rw::Resource& lrRes)
{
    lrType.IdListResourceType::FixDown(lpResource, lrRes);
    lrType.IdListResourceType::FixUp(lpResource, lrRes);
}
