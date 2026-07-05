#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStreamBase (operator<< dump)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"       // CgsCore::SPrintf (resource-id hex)

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

// @ 0x82803888 -- operator<<(CgsDev::StrStreamBase&, const VaultArray&): the debug dump of the
// whole vault array. Streams a header, then per slot a line describing empty/occupied, the 64-bit
// resource id (8 hex bytes), the live ref count, and -- for occupied slots -- whether it backs a
// streamed vault and that vault's stream index.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 inlines StrStreamBase::operator<<(s32)'s
// print-mode juggling at each integer insertion; that same operator is reconstructed in the
// CgsStrStream TU, so streaming an s32 via `lStream << value` reproduces the inlined sequence.
// Slot fields are read at raw X360 byte offsets (resourceId @+0x00 8B, miRefCount @+0x0C,
// miStreamedVaultIndex @+0x10; slot stride 24) -- the committed VaultSlot is missing mpVault @+0x08
// (16 vs true 24 bytes), so the dump reaches the count fields by offset rather than by member.
// The occupied-slot ContainsStreamedVault() call mirrors the asm's bl on the raw slot pointer.
CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lStream, const VaultArray& lVaultArray)
{
    lStream << "\n\nCgsAttribSys::VaultArray:\n";

    const u8* lpSlotBytes = reinterpret_cast<const u8*>(lVaultArray.mpaSlots);
    for (s32 liSlot = 0; liSlot < lVaultArray.miNumSlots; ++liSlot)
    {
        const u8*        lpSlot       = lpSlotBytes + liSlot * 24;
        const VaultSlot* lpVaultSlot  = reinterpret_cast<const VaultSlot*>(lpSlot);
        const s32        liRefCount   = *reinterpret_cast<const s32*>(lpSlot + 0x0C);
        const u8*        lpResourceId = lpSlot + 0x00;   // 8 raw id bytes

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
            const s32 liStreamVaultIndex = *reinterpret_cast<const s32*>(lpSlot + 0x10);

            lStream << ": occupied, ResourceId = ";
            lStream << lacHex;
            lStream << ", RefCount = ";
            lStream << liRefCount;
            lStream << ", StreamedVault = ";
            lStream << (lpVaultSlot->ContainsStreamedVault() ? "true" : "false");
            lStream << ", StreamVaultIndex = ";
            lStream << liStreamVaultIndex;
        }
    }

    return lStream;
}
}
