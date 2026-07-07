#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // FreeWithCensusIf

// Attrib::Vault::DataBlock member functions, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2):
//
//   Set          @ 0x82803210  bind a payload block (data ptr + 24-bit size + 8-bit kind)
//   ReleaseAsset @ 0x82803298  fire the GC release callback (if the block owns a live asset)
//                              then clear the block

// ReleaseAsset @ 0x82803298. Fire the host GC callback only when this block still owns a live
// asset (mData set AND kind byte non-zero). Then clear the block (mpData = 0; packed kind|size = 0).
void Attrib::Vault::DataBlock::ReleaseAsset(Vault::AssetID lAssetId, IGarbageCollector* lpGC)
{
    const bool lbReleaseAsset = (mpData != nullptr) && (GetKind() != 0);
    if (lbReleaseAsset)
    {
        // X360: gc->vtable[1](kind, id, data, size). r6 (lpData) still holds mData from the
        // earlier lwz r6,0(r31), so the natural block being freed is mpData.
        lpGC->ReleaseData(GetKind(), lAssetId, mpData, GetSize());
    }

    mpData = nullptr;
    muKindAndSize = 0;
}

// Set @ 0x82803210. AttribSys packs the block size into 24 bits alongside an 8-bit kind tag,
// hard-limiting a single block to 0xFFFFFF bytes. (De-inlined SPrintf-into-assert-buffer +
// Begin/Fire/EndAssert collapse to one CGS_ASSERT; message text is X360 rodata, verbatim.)
void Attrib::Vault::DataBlock::Set(void* lpData, unsigned int luSize, u8 lu8Kind)
{
    CGS_ASSERT(luSize <= 0xFFFFFF,
               "AttribSys implementation limits file size to 24 MB (%d byte block encountered)");

    mpData = lpData;
    // Keep the existing kind byte while writing the 24-bit size (rlwimi), then overwrite the
    // kind byte (stb) -- store-for-store with the X360 pair.
    muKindAndSize = (muKindAndSize & 0xFF000000u) | (luSize & 0x00FFFFFFu);
    muKindAndSize = (muKindAndSize & 0x00FFFFFFu) | (static_cast<u32>(lu8Kind) << 24);
}

// @ 0x8280F098 -- Attrib::Vault scalar deleting destructor (MSVC's ??_G thunk). Called by
// ClassPrivate's live-class refcount teardown, CgsAttribSys::VaultSlot::DoUnload,
// Attrib::Collection::~Collection and DatabaseExportPolicy::Deinitialize. Runs the real
// ~Vault(), then -- when the low should-free bit of the deleting flag is set -- returns the
// 88-byte (0x58) vault to the AttribSys package allocator, with the shared live-byte census
// decremented and the peak refreshed, NULL-guarding the block exactly as the X360 emits
// (cmplwi r30,0 / beq before the GetAttribS()->Free(this,88,0)). Routed through the shared
// null-guarded census-free helper so the census counters (dword_83011BFC / dword_83011BF8)
// stay defined exactly once. Returns this. Store-for-store twin of the committed
// Collection_ScalarDeletingDtor (vechashmap.cpp @ 0x8280C510), size 88 vs 40.
void* Attrib::Vault_ScalarDeletingDtor(Attrib::Vault* lpVault, int liDeleteFlag)
{
    lpVault->~Vault();
    if ((liDeleteFlag & 1) != 0)
        Attrib::HashMapTablePolicy::FreeWithCensusIf(lpVault, 88, NULL);
    return lpVault;
}
