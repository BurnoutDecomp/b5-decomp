#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsAttribSys
{
void VaultArray::Construct(Attrib::IGarbageCollector* lpGarbageCollector)
{
    CGS_ASSERT(lpGarbageCollector != nullptr, "lpGarbageCollector != NULL");

    mpaSlots = nullptr;
    mpGarbageCollector = lpGarbageCollector;
    // The X360 build inlines StreamedVaultAllocator::Construct (it is not a
    // separate symbol); restored here as the logical call. Its body — the
    // free-list/bit-array initialisation — is the allocator's own TU.
    mVaultAllocator.Construct();
    // Publish the allocator into the VaultSlot registry. The X360 build inlines
    // VaultSlot::SetVaultAllocator to a direct store of the static pointer
    // (dword_83011B60 = &this->mVaultAllocator); restored here as the logical call.
    VaultSlot::SetVaultAllocator(&mVaultAllocator);
    miNumSlots = 0;
    mbPrepared = false;
}
}
