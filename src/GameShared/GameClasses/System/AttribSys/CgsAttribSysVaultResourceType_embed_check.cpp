// Embed check for CgsResource::AttribSysVaultResourceType::GetSerialisedResourceDescriptor:
//   CgsResource::AttribSysVaultResourceType::GetSerialisedResourceDescriptor @ 0x828EBA48
//
// GetTypeID/FixUp/GetImportCount are sibling virtuals owned by other recon passes, so the
// type is NOT instantiated; the bodied func is invoked through an explicit qualified
// (non-virtual) call on a placement reference, so no complete vtable is required.

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultResourceType.h"
#include "rw/rwcore_structs.h"
#include "types.hpp"

using namespace CgsResource;

// rw::BaseResourceDescriptors<5> = five {m_size,m_alignment} pairs = 40 bytes (10 u32 words),
// the X360 serialised descriptor the body fills word-by-word.
static_assert(sizeof(ResourceDescriptor) == 40, "X360 serialised descriptor is 5*(size,align)");
static_assert(sizeof(rw::BaseResourceDescriptor) == 8, "descriptor entry is {size,align}");

void ExerciseAttribSysVaultDesc(const AttribSysVaultResourceType& lrType, const void* lpResource)
{
    ResourceDescriptor lDesc = lrType.AttribSysVaultResourceType::GetSerialisedResourceDescriptor(lpResource);
    (void)lDesc;
}
