#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStreamBase (operator<< dump)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"       // CgsCore::SPrintf (resource-id hex)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"    // LinearMalloc::Malloc (Prepare's slot carve)

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

// @ 0x82805CB0 -- reserve liMaxNumVaults slots from the AttribSys linear allocator and
// ready the streamed-vault allocator. Asserts single-Prepare, a positive slot count and
// the allocator; carves 24 bytes per slot (the X360 stride; the x64 slot is wider --
// semantic parity by sizeof), Constructs each slot free, Prepares the embedded
// StreamedVaultAllocator, then raises mbPrepared.
void VaultArray::Prepare(s32 liMaxNumVaults, CgsMemory::LinearMalloc* lpLinearAllocator)
{
    CGS_ASSERT(!mbPrepared, "Trying to call Prepare() more than once");   // .cpp:75
    CGS_ASSERT(liMaxNumVaults > 0, "liNumSlots > 0");                     // .cpp:76
    CGS_ASSERT(lpLinearAllocator != nullptr, "lpAllocator != NULL");      // .cpp:77

    miNumSlots = liMaxNumVaults;
    mpaSlots   = static_cast<VaultSlot*>(
        lpLinearAllocator->Malloc(sizeof(VaultSlot) * static_cast<size_t>(liMaxNumVaults)));
    CGS_ASSERT(mpaSlots != nullptr, "mpaSlots != NULL");                  // .cpp:90

    for (s32 liSlot = 0; liSlot < miNumSlots; ++liSlot)
        mpaSlots[liSlot].Construct(lpLinearAllocator);

    mVaultAllocator.Prepare(lpLinearAllocator);
    mbPrepared = true;
}

// @ 0x82803888 -- operator<<(CgsDev::StrStreamBase&, const VaultArray&): the debug dump of the
// whole vault array. Streams a header, then per slot a line describing empty/occupied, the 64-bit
// resource id (8 hex bytes), the live ref count, and -- for occupied slots -- whether it backs a
// streamed vault and that vault's stream index.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 inlines StrStreamBase::operator<<(s32)'s
// print-mode juggling at each integer insertion; that same operator is reconstructed in the
// CgsStrStream TU, so streaming an s32 via `lStream << value` reproduces the inlined sequence.
// The X360 loads the slot fields raw (resourceId @+0x00 8B, miRefCount @+0x0C,
// miStreamedVaultIndex @+0x10; stride 24); with the FULL VaultSlot layout now committed the
// dump reads the same fields by NAME (the x64 gate's semantic-parity form).
// The occupied-slot ContainsStreamedVault() call mirrors the asm's bl on the slot pointer.
CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lStream, const VaultArray& lVaultArray)
{
    lStream << "\n\nCgsAttribSys::VaultArray:\n";

    for (s32 liSlot = 0; liSlot < lVaultArray.miNumSlots; ++liSlot)
    {
        const VaultSlot& lrSlot      = lVaultArray.mpaSlots[liSlot];
        const s32        liRefCount  = lrSlot.GetRefCount();
        const u8*        lpResourceId =
            reinterpret_cast<const u8*>(&lrSlot.GetResourceId());   // 8 raw id bytes

        lStream << "\nSlot ";
        lStream << liSlot;

        char lacHex[32];
        CgsCore::SPrintf(lacHex, 32, "%02x%02x%02x%02x%02x%02x%02x%02x",
                         lpResourceId[0], lpResourceId[1], lpResourceId[2], lpResourceId[3],
                         lpResourceId[4], lpResourceId[5], lpResourceId[6], lpResourceId[7]);

        if (liRefCount <= 0)
        {
            lStream << ": empty, ResourceId = ";
            lStream << lacHex;
            lStream << ", RefCount = ";
            lStream << liRefCount;
        }
        else
        {
            lStream << ": occupied, ResourceId = ";
            lStream << lacHex;
            lStream << ", RefCount = ";
            lStream << liRefCount;
            lStream << ", StreamedVault = ";
            lStream << (lrSlot.ContainsStreamedVault() ? "true" : "false");
            lStream << ", StreamVaultIndex = ";
            lStream << lrSlot.GetStreamedVaultIndex();
        }
    }

    return lStream;
}

// @ 0x828036A8 -- find the slot for the request's vault. Walk the array once:
// remember the FIRST free slot; if an occupied slot already holds this resource
// (the X360 compares the id's low 32-bit word -- the slot id word @BE +4), that
// slot wins immediately (the caller ref-count-bumps it). Otherwise the first
// free slot; asserts when the array is unprepared or exhausted.
s32 VaultArray::GetFreeSlotIndex(CgsResource::ResourceHandle lVaultResHandle) const
{
    CGS_ASSERT(mbPrepared,
               "Trying to access a vault slot before Prepare() has been called");   // .cpp:126

    s32 liFirstFree = -1;
    for (s32 liSlot = 0; liSlot < GetNumSlots(); ++liSlot)
    {
        const VaultSlot& lrSlot = mpaSlots[liSlot];
        if (lrSlot.GetRefCount() <= 0 && liFirstFree < 0)
            liFirstFree = liSlot;

        // The X360 compares only the low 32-bit halves of the two 64-bit ids
        // (lwz of the BE +4 words); reproduced as the truncated compare.
        if (lrSlot.GetRefCount() > 0 &&
            static_cast<u32>(lrSlot.GetResourceId().GetHash()) ==
                static_cast<u32>(lVaultResHandle.GetResourceId().GetHash()))
        {
            return liSlot;
        }
    }

    CGS_ASSERT(liFirstFree >= 0, "Ran out of free vault slots");                     // .cpp:147
    return liFirstFree;
}
}
