#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsAttribSys
{
void VaultArray::Construct(Attrib::IGarbageCollector* lpGarbageCollector)
{
    if (lpGarbageCollector == nullptr)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpGarbageCollector != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysVaultArray.cpp",
            45);
        CgsDev::Assert::EndAssert();
    }

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
