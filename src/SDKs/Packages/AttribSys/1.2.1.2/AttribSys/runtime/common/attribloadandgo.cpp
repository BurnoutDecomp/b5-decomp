#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // FreeWithCensusIf
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/export/attribexportmanager.h" // Attrib::ExportManager
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"          // Attrib::Database (Initialize/Deinitialize GC pass)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h" // Attrib::Alloc

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
    // The per-policy hook is non-const (DWARF IExportPolicy:82 takes Vault&);
    // the X360 manager hands the same vault pointer straight through this const
    // boundary.
    for (unsigned int luIndex = 0; luIndex < mNumPolicies; ++luIndex)
        mpPolicies[luIndex].mPolicy->PrepareToDeinitialize(const_cast<Vault&>(lrVault));
}

// ---------------------------------------------------------------------------
// The .vlt chunk fourCCs, read as native u32 words. The serialised container
// stores them so this compare works on either endianness of the DATA once the
// blob matches the host (the LE port flips them like any u32; BPR's shipped LE
// vaults prove the convention -- 'Vers' arrives as 'sreV' bytes).
// ---------------------------------------------------------------------------
static const u32 KU_CHUNK_VERS = 0x56657273u;   // 'Vers'
static const u32 KU_CHUNK_DEPN = 0x4465704Eu;   // 'DepN'
static const u32 KU_CHUNK_EXPN = 0x4578704Eu;   // 'ExpN'
static const u32 KU_CHUNK_PTRN = 0x5074724Eu;   // 'PtrN'

// Vault ctor @ 0x8280A2E8. Construct the in-memory view of one serialised .vlt
// image: walk the chunk stream to find Vers/DepN/ExpN/PtrN (StrN/DatN are not
// captured -- DatN is only ever reached through the ExpN entry offsets), count
// the export slots (the serialised seed plus one per policy-exported entry),
// carve the shared DataBlock/AssetID arrays (dependencies first, exports after),
// copy the dependency ids out of DepN, and bind the vault's own image as
// dependency block [0] (kind = lbType), which counts as the first resolved
// dependency.
Attrib::Vault::Vault(ExportManager& lExportMgr, AssetID lAssetId, void* lpData,
                     unsigned int luSize, u8 lbType, IGarbageCollector* lpGC)
    : mExportMgr(lExportMgr)
{
    (void)lAssetId;   // the X360 ctor zeroes mUserID regardless (SetUserID re-stamps it)

    mVersion = 0;
    mUserID = 0;
    mRefCount = 1;
    mGC = lpGC;
    mDependencies = NULL;
    mDepData = NULL;
    mDepIDs = NULL;
    mNumDependencies = 0;
    mResolvedCount = 0;
    mPointers = NULL;
    mTransientData = NULL;
    mExports = NULL;
    mExportData = NULL;
    mExportIDs = NULL;
    mNumExports = 0;
    mNumAllocExports = 0;
    mNumLoadedExports = 0;
    mInited = false;
    mDeinited = false;

    // ---- chunk walk (cursor advances by each chunk's whole size) ----
    u8* lpCursor = static_cast<u8*>(lpData);
    u8* lpEnd = lpCursor + luSize;
    while (lpCursor < lpEnd)
    {
        ChunkBlock* lpChunk = reinterpret_cast<ChunkBlock*>(lpCursor);
        switch (lpChunk->muFourCC)
        {
            case KU_CHUNK_VERS:
                mVersion = *reinterpret_cast<const u64*>(lpCursor + 8);   // serialized .vlt blob: Vers payload after the 8-byte chunk header
                break;
            case KU_CHUNK_DEPN:
                mDependencies = reinterpret_cast<DependencyNode*>(lpChunk);
                break;
            case KU_CHUNK_EXPN:
                mExports = reinterpret_cast<ExportNode*>(lpChunk);
                break;
            case KU_CHUNK_PTRN:
                mPointers = lpChunk;
                break;
            default:   // StrN / DatN: not captured
                break;
        }
        lpCursor += lpChunk->muSize;
    }

    CGS_ASSERT(mDependencies != NULL && mPointers != NULL && mExports != NULL,
               "Attrib::Vault could not find required chunk.");

    mTransientData = static_cast<u8*>(lpData);
    mNumDependencies = mDependencies->muNumDependencies;

    // Export-slot count: the serialised seed, plus one per entry whose type has
    // a registered policy that answers IsExported (the Database entry does; the
    // Class/Collection policies' IsExported is the folded `return false`).
    mNumAllocExports = mExports->muBaseAllocExports;
    mNumLoadedExports = mExports->muNumEntries;
    if (mNumAllocExports != 0 || mNumLoadedExports != 0)
    {
        const ExportEntry* lpEntries = mExports->GetEntries();
        for (unsigned int luIndex = 0; luIndex < mNumLoadedExports; ++luIndex)
        {
            const unsigned int luPolicy =
                mExportMgr.GetExportPolicyIndex(lpEntries[luIndex].muTypeId);
            if (luPolicy < mExportMgr.GetNumPolicies())
            {
                IExportPolicy* lpPolicy = mExportMgr.GetPair(luPolicy).mPolicy;
                if (lpPolicy != NULL && lpPolicy->IsExported(lpEntries[luIndex].muTypeId))
                    ++mNumAllocExports;
            }
        }
    }

    // Shared block arrays: dependencies first, the export span right after.
    // (X360 8-byte DataBlock / 8-byte AssetID strides; the x64 DataBlock widens,
    // so the carve is sizeof-based rather than the literal 8.)
    const unsigned int luTotalBlocks = mNumAllocExports + mNumDependencies;
    mDepData = static_cast<DataBlock*>(
        Attrib::Alloc(sizeof(DataBlock) * luTotalBlocks, "Attrib::DataBlocks"));
    mExportData = mDepData + mNumDependencies;
    for (unsigned int luIndex = 0; luIndex < luTotalBlocks; ++luIndex)
    {
        // X360 zeroes each block's payload pointer and packed kind|size word in
        // place; value-initialization is that same two-field zero.
        mDepData[luIndex] = DataBlock();
    }

    mDepIDs = static_cast<AssetID*>(
        Attrib::Alloc(sizeof(AssetID) * luTotalBlocks, "Attrib::AssetIDs"));
    mExportIDs = mDepIDs + mNumDependencies;
    const AssetID* lpSerialisedIds = mDependencies->GetIds();
    for (unsigned int luIndex = 0; luIndex < mNumDependencies; ++luIndex)
        mDepIDs[luIndex] = lpSerialisedIds[luIndex];

    // Bind the vault's own serialised image as dependency block [0].
    CGS_ASSERT(lbType != 0, "Kind of zero is not allowed");
    mDepData[0].Set(lpData, luSize, lbType);
    ++mResolvedCount;
}

// Initialize @ 0x8280A660. Commit a fully resolved vault into the live attribute
// database: (1) apply the PtrN fixup records, (2) hand every serialised export
// to its type's policy (the policies build the DatabasePrivate / ClassPrivate /
// Collection objects), then (3) release the vault's own image block (the GC
// callback; the serialised .vlt is no longer needed once the fixed-up payloads
// have been consumed) and drop the consumed chunk pointers.
void Attrib::Vault::Initialize()
{
    CGS_ASSERT(!HasUnresolvedDependency(),
               "Attrib::Vault has unresolved dependency on call to initialize.");
    CGS_ASSERT(!mInited, "Attrib::Vault initialized more than once.");

    // ---- (1) PtrN fixups: records after the chunk header, until an unknown type ----
    u8* lpCurrentBase = NULL;
    unsigned int luCurrentSize = 0;
    const PointerNode* lpRecord =
        reinterpret_cast<const PointerNode*>(reinterpret_cast<u8*>(mPointers) + 8);
    for (bool lbDone = false; !lbDone; ++lpRecord)
    {
        switch (lpRecord->muType)
        {
            case 1:
                // Zero the slot in the current block.
                *reinterpret_cast<u32*>(lpCurrentBase + lpRecord->muSlotOffset) = 0;
                break;

            case 2:
                // Select the current block (a resolved dependency's data).
                CGS_ASSERT(lpRecord->muDepIndex < mNumDependencies,
                           "Attrib::Vault dependency reference has invalid index.");
                lpCurrentBase = static_cast<u8*>(mDepData[lpRecord->muDepIndex].GetData());
                luCurrentSize = mDepData[lpRecord->muDepIndex].GetSize();
                break;

            case 3:
            {
                // Pointer fixup: *(current + slot) = dep[idx].data + dataOffset.
                // The serialised slot is 4 bytes (X360 stwx); the PC build keeps
                // the on-disk 32-bit slot and stores the truncated pointer (the
                // committed PointerFromU32 low-4GB convention every serialised
                // resource consumer uses).
                CGS_ASSERT(lpRecord->muDepIndex < mNumDependencies,
                           "Attrib::Vault dependency reference has invalid index.");
                CGS_ASSERT(lpRecord->muSlotOffset <= luCurrentSize - 4,
                           "Attrib::Vault has invalid fixup offset.");
                u8* lpTarget = static_cast<u8*>(mDepData[lpRecord->muDepIndex].GetData()) +
                               lpRecord->muDataOffset;
                *reinterpret_cast<u32*>(lpCurrentBase + lpRecord->muSlotOffset) =
                    static_cast<u32>(reinterpret_cast<uintptr_t>(lpTarget));
                break;
            }

            case 4:
            {
                // Cross-vault export import: the dependency's data is another
                // Attrib::Vault; find its export whose id's low doubleword-half
                // matches the record (the X360 compares the 32-bit word at BE +4
                // == the truncated id) and store that export's payload pointer.
                CGS_ASSERT(lpRecord->muDepIndex < mNumDependencies,
                           "Attrib::Vault dependency reference has invalid index.");
                CGS_ASSERT(lpRecord->muSlotOffset <= luCurrentSize - 4,
                           "Attrib::Vault has invalid fixup offset.");
                const Vault* lpDepVault =
                    static_cast<const Vault*>(mDepData[lpRecord->muDepIndex].GetData());
                const unsigned int luDepExports = lpDepVault->mNumExports;
                unsigned int luFound = 0;
                while (luFound < luDepExports &&
                       static_cast<u32>(lpDepVault->mExportIDs[luFound]) !=
                           static_cast<u32>(lpRecord->muDataOffset))
                    ++luFound;
                u32 luValue = 0;
                if (luFound < luDepExports)
                {
                    luValue = static_cast<u32>(reinterpret_cast<uintptr_t>(
                        lpDepVault->GetExportData(luFound)));
                }
                *reinterpret_cast<u32*>(lpCurrentBase + lpRecord->muSlotOffset) = luValue;
                break;
            }

            default:
                lbDone = true;   // the type-0 terminator record
                break;
        }
    }

    // ---- (2) initialize every serialised export through its policy ----
    // The X360 captures the entry cursor, then drops mExports before the loop.
    const unsigned int luNumEntries = mNumLoadedExports;
    const ExportEntry* lpEntries = mExports->GetEntries();
    mExports = NULL;
    for (unsigned int luIndex = 0; luIndex < luNumEntries; ++luIndex)
    {
        const ExportEntry& lrEntry = lpEntries[luIndex];
        const unsigned int luPolicy = mExportMgr.GetExportPolicyIndex(lrEntry.muTypeId);
        if (luPolicy < mExportMgr.GetNumPolicies())
        {
            IExportPolicy* lpPolicy = mExportMgr.GetPair(luPolicy).mPolicy;
            if (lpPolicy != NULL)
            {
                lpPolicy->Initialize(*this, lrEntry.muTypeId, lrEntry.muExportId,
                                     mTransientData + lrEntry.muOffset, lrEntry.muSize);
            }
        }
    }

    // ---- (3) release the vault's own image block; drop the consumed chunks ----
    mDepData[0].ReleaseAsset(mDepIDs[0], mGC);
    mDependencies = NULL;
    mPointers = NULL;
    mInited = true;
}

// Deinitialize @ 0x8280E6F0. Tear the vault back out of the live database: fan
// the per-policy pre-deinitialize hook out, run each initialized export's policy
// Deinitialize (the export DataBlock's kind byte is the policy index Export()
// stamped), collect the database garbage, then release the remaining dependency
// blocks (the bin/asset payloads; block [0] was already released by Initialize).
void Attrib::Vault::Deinitialize()
{
    CGS_ASSERT(mInited && !mDeinited,
               "Attrib::Vault deinitializing unintialized or deinitialized vault");

    if (mRefCount != 0)
    {
        // X360 hands (mgr, vault) through the const boundary; the manager fans
        // the non-const per-policy hook out.
        const_cast<ExportManager&>(mExportMgr).PrepareToDeinitialize(*this);

        for (unsigned int luIndex = mNumExports; luIndex-- > 0; )
        {
            const u8 lu8Policy = mExportData[luIndex].GetKind();
            if (lu8Policy < mExportMgr.GetNumPolicies())
            {
                const ExportManager::ExportPolicyPair& lrPair = mExportMgr.GetPair(lu8Policy);
                if (lrPair.mPolicy != NULL)
                {
                    // The X360 stages a local {type} copy for the by-ref call.
                    const TypeID lType = lrPair.mType;
                    lrPair.mPolicy->Deinitialize(*this, lType, mExportIDs[luIndex]);
                }
            }
        }
    }

    CGS_ASSERT(Database::IsInitialized(), "Attribute database not initialized.");
    Database::Get().CollectGarbage();

    mDeinited = true;
    for (unsigned int luIndex = 1; luIndex < mNumDependencies; ++luIndex)
        mDepData[luIndex].ReleaseAsset(mDepIDs[luIndex], mGC);
}

// Export @ 0x8280A988. Register one initialized export's live data into the next
// free slot: store the export id, resolve the type's policy index (it becomes
// the DataBlock kind byte -- 0xFF when the type has no policy -- so Deinitialize
// can dispatch), and bind {lpData, luSize} into the slot.
void Attrib::Vault::Export(const TypeID& lrType, const ExportID& lrExport,
                           void* lpData, unsigned int luSize)
{
    CGS_ASSERT(mNumExports < mNumAllocExports,
               "Attrib::Vault::Export called more times than IsExported returned true.");

    const unsigned int luIndex = mNumExports;
    ++mNumExports;
    mExportIDs[luIndex] = lrExport;

    const unsigned int luPolicy = mExportMgr.GetExportPolicyIndex(lrType);
    const u8 lu8Kind = (luPolicy >= mExportMgr.GetNumPolicies())
                           ? static_cast<u8>(0xFF)
                           : static_cast<u8>(luPolicy);
    mExportData[luIndex].Set(lpData, luSize, lu8Kind);
}

// ExportManager ctor (inlined by the X360 into Database::GetExportPolicies
// @0x8280DC70): reserve a fixed policy table. The reserve>254 guard's message is
// SPrintf'd into the assert buffer at the X360 site before the check.
Attrib::ExportManager::ExportManager(unsigned int luReserve)
{
    CGS_ASSERT(luReserve <= 254,
               "Attrib::ExportManager does not support reserving or adding more than 254 policies (%d requested).");
    mMaxPolicies = luReserve;
    mNumPolicies = 0;
    mpPolicies = static_cast<ExportPolicyPair*>(
        Attrib::Alloc(sizeof(ExportPolicyPair) * luReserve, "Attrib::ExportPolicyPair"));
}

// The pair ordering quick_sort/lower_bound use: ascending TypeID.
bool Attrib::ExportManager::ExportPolicyPair::operator<(const ExportPolicyPair& lrOther) const
{
    return mType < lrOther.mType;
}

// SortPolicies -- the eastl::quick_sort<ExportPolicyPair*> instantiation
// Database::GetExportPolicies runs over the (tiny) pair table; an insertion
// sort is the same observable ordering for the sorted-table contract.
void Attrib::ExportManager::SortPolicies()
{
    for (unsigned int luIndex = 1; luIndex < mNumPolicies; ++luIndex)
    {
        const ExportPolicyPair lPair = mpPolicies[luIndex];
        unsigned int luSlot = luIndex;
        while (luSlot > 0 && lPair < mpPolicies[luSlot - 1])
        {
            mpPolicies[luSlot] = mpPolicies[luSlot - 1];
            --luSlot;
        }
        mpPolicies[luSlot] = lPair;
    }
}

// GetExportPolicyIndex (DWARF attribloadandgo.h:112) -- the eastl::lower_bound
// instantiation (@0x82808FE8) the X360 inlines at every Vault dispatch site.
// NB the X360 sites use the raw lower_bound result with only a `< end` guard,
// NO key-equality re-check (Vault ctor @0x8280A308 / Initialize @0x8280A8xx /
// Export @0x8280AA0x) -- an absent type whose lower bound lands on a bigger key
// would dispatch that bigger key's policy. Reproduced verbatim; every type the
// shipped schema/vault data stores has an exact policy row, so the branch is
// value-identical on real data.
unsigned int Attrib::ExportManager::GetExportPolicyIndex(TypeID luType) const
{
    unsigned int luLow = 0;
    unsigned int luCount = mNumPolicies;
    while (luCount > 0)
    {
        const unsigned int luHalf = luCount >> 1;
        const unsigned int luProbe = luLow + luHalf;
        if (mpPolicies[luProbe].mType < luType)
        {
            luLow = luProbe + 1;
            luCount -= luHalf + 1;
        }
        else
        {
            luCount = luHalf;
        }
    }
    return luLow;   // == mNumPolicies when luType is beyond every row
}

// GetExportPolicy (DWARF attribloadandgo.h:109) -- the pointer form; NULL when
// the type has no exact policy row.
Attrib::IExportPolicy* Attrib::ExportManager::GetExportPolicy(TypeID luType) const
{
    const unsigned int luIndex = GetExportPolicyIndex(luType);
    return (luIndex < mNumPolicies && mpPolicies[luIndex].mType == luType)
               ? mpPolicies[luIndex].mPolicy
               : NULL;
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

    // Both shared arrays were allocated one slot per (dependency + reserved
    // export) by the ctor (8-byte X360 strides; the x64 DataBlock widens, so the
    // byte counts are sizeof-based to mirror the ctor's Alloc exactly and keep
    // the shared byte census balanced).
    const size_t lnBlockCount = static_cast<size_t>(mNumDependencies + mNumAllocExports);
    Attrib::HashMapTablePolicy::FreeWithCensusIf(mDepData, sizeof(DataBlock) * lnBlockCount,
                                                 "Attrib::DataBlocks");
    Attrib::HashMapTablePolicy::FreeWithCensusIf(mDepIDs, sizeof(AssetID) * lnBlockCount,
                                                 "Attrib::AssetIDs");
}
