#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // FreeWithCensusIf
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/export/attribexportmanager.h" // Attrib::ExportManager

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

// GetExportData @ 0x82803420. The payload pointer of exported data block luIndex.
// The X360 asserts the index against the loaded export count (+0x44 = mNumExports)
// then returns mExportData[luIndex].mpData (the DataBlock array has an 8-byte stride;
// only the first word -- the payload pointer -- is read).
void* Attrib::Vault::GetExportData(unsigned int luIndex) const
{
    CGS_ASSERT(luIndex < mNumExports, "Attrib::Vault given bad index.");
    return mExportData[luIndex].GetData();
}

// ResolveDependency @ 0x82803338. Bind a resolved dependency's payload into the
// vault. Dependency data blocks live in the shared mDepData array AFTER the vault's
// own block at [0], so dependency luIndex maps to mDepData[luIndex + 1] (the X360
// computes mDepData + 8*luIndex + 8). If that block was not already fully resolved
// (a live payload AND a non-zero kind) the resolved-dependency counter advances.
// The store itself is DataBlock::Set, which re-checks the 24-bit size limit.
void Attrib::Vault::ResolveDependency(unsigned int luIndex, void* lpData,
                                      unsigned int luSize, u8 lbIsAsset)
{
    CGS_ASSERT((luIndex + 1) < mNumDependencies, "Attrib::Vault invalid dependency index.");

    DataBlock& lrBlock = mDepData[luIndex + 1];

    const bool lbAlreadyResolved = (lrBlock.GetData() != nullptr) && (lrBlock.GetKind() != 0);
    if (!lbAlreadyResolved)
        ++mResolvedCount;

    CGS_ASSERT(lbIsAsset != 0, "Kind of zero is not allowed");

    lrBlock.Set(lpData, luSize, lbIsAsset);
}

// ExportManager::AddExportPolicy @ 0x82803138. Append one (type -> policy) row to the
// Database's reserved, TypeID-sorted policy table. The X360 asserts the reserved table
// is not already full, writes {type, policy} into the next free slot, then bumps the
// live count. The slot is taken from the OLD count before it is incremented (store-for-
// store with the X360, which computes &table[count] then stores count+1 first).
void Attrib::ExportManager::AddExportPolicy(TypeID luType, IExportPolicy* lpPolicy)
{
    CGS_ASSERT(mNumPolicies < mMaxPolicies,
               "ExportManager::AddExportPolicy -- insufficient entries reserved in policy table.");

    ExportPolicyPair& lrSlot = mpPolicies[mNumPolicies];
    ++mNumPolicies;

    lrSlot.mType   = luType;
    lrSlot.mPolicy = lpPolicy;
}

// ExportManager::PrepareToDeinitialize @ 0x828031A8. Fan the per-policy pre-deinitialize
// hook out to every registered policy in the (sorted) policy table. The X360 walks the live
// table [0, mNumPolicies) and, for each row, virtual-dispatches through the policy's vtable
// (slot 7) with the vault; there is no per-row null guard. The vault is passed straight
// through untouched.
void Attrib::ExportManager::PrepareToDeinitialize(const Vault& lrVault)
{
    for (unsigned int luIndex = 0; luIndex < mNumPolicies; ++luIndex)
        mpPolicies[luIndex].mPolicy->PrepareToDeinitialize(lrVault);
}

// ~Vault @ 0x8280EF38. Destroy a loaded vault. If it is still live (initialized and not yet
// deinitialized) it is torn down first (Deinitialize). Then the destructor asserts every
// export block was cleared by that deinitialize -- a leftover live payload means a dangling
// reference -- before returning the two shared block arrays (the DataBlocks and their AssetIDs)
// to the AttribSys package allocator. Both arrays hold (mNumDependencies + mNumAllocExports)
// eight-byte entries, so each free is 8*(that count) bytes; the census-decrement + NULL-guarded
// package-allocator free the X360 inlines is the shared FreeWithCensusIf helper (census updated
// unconditionally, inner free skipped when the block or size is zero), keeping the two live-byte
// counters defined exactly once. Diagnostic tags are the X360 rodata, verbatim.
Attrib::Vault::~Vault()
{
    if (mInited && !mDeinited)
        Deinitialize();

    // Dangling-reference guard: after deinitialize every export DataBlock must have had its
    // payload pointer cleared. (Empty export set trivially satisfies this.)
    bool lbExportsCleared = true;
    for (unsigned int luIndex = 0; luIndex < mNumExports; ++luIndex)
    {
        if (mExportData[luIndex].GetData() != nullptr)
        {
            lbExportsCleared = false;
            break;
        }
    }
    CGS_ASSERT(lbExportsCleared,
               "Attrib::Vault destructor failed to clear exports after deinitialize; likely problem with dangling references.");

    // Both shared arrays were allocated 8 bytes per (dependency + reserved-export) slot by the
    // ctor; free them back with the same byte count so the shared byte census balances.
    const size_t lnBlockBytes = static_cast<size_t>(8u * (mNumDependencies + mNumAllocExports));
    Attrib::HashMapTablePolicy::FreeWithCensusIf(mDepData, lnBlockBytes, "Attrib::DataBlocks");
    Attrib::HashMapTablePolicy::FreeWithCensusIf(mDepIDs,  lnBlockBytes, "Attrib::AssetIDs");
}
