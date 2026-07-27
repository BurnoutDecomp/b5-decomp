#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"     // GetEaStlAllocator()
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"  // AttribSysPackageAllocator::Free(void*,s32,const char*)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/export/attribexportmanager.h" // Class/CollectionExportPolicy
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"            // Attrib::Database (CollectGarbage member + sThis)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h" // Attrib::DatabasePrivate + LoadData views
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h"    // ClassLoadData + ClassPrivate + ClassStaticDesc seam
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"       // Attrib::Vault (Export / policy dispatch)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h"          // Attrib::Alloc
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"          // StringToKey64 (type-key globals)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // shared census-free

#include <cstring>   // memcpy (AttribVectorReserve) / strlen (type-key hashing)
#include <new>       // placement new (DatabasePrivate over the Alloc'd block)

namespace Attrib
{
    // The generated codegen entry points the registry bring-up rides
    // (GameSource/AttribSys/Generated/codegen.cpp): the shared default-data
    // area, and -- once the generated tables are recovered -- the per-type
    // handler lookup. Declared here against that TU.
    void* DefaultDataArea(u32 luSize);

    namespace
    {
        // TypeDesc::NameToType (codegen @0x821F0150) is `StringToKey(name)` --
        // the full 64-bit hash on this spine (the serialised Definition mType
        // fields carry hash64 of the type names). Routed through the real
        // lookup8 hash (attribhash64.cpp) here because the codegen placeholder
        // TU still models a 32-bit key path.
        u64 TypeDesc_NameToType64(const char* lpcName)
        {
            return StringToKey64(lpcName, static_cast<u32>(strlen(lpcName)),
                                 KU_ATTRIB_STRING_TO_KEY_SEED);
        }

        // TypeDesc::Lookup (codegen @0x821F00E8) -- the generated type-handler
        // binary search. [FLAG] the PC generated tables (codegen.cpp) are empty
        // placeholders, so the lookup yields no handler; identical observable
        // result to running the generated search over empty tables. Recover the
        // generated tables in the codegen TU to light type handlers up
        // (RefSpec retain/release etc.).
        void* TypeDesc_LookupHandler(u64 luType)
        {
            (void)luType;
            return NULL;
        }
    }
}

// =============================================================================
// AttribSys database container helpers -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// These are the eastl::list / eastl::vector<AttribSysPackageAllocator> methods the X360
// emits under truncated symbols; DWARF (EASTL/list.h, EASTL/vector.h) resolves them.
//
// The list frees route through the STATIC EASTL AttribSys package allocator that the
// memory manager hands out (GetEaStlAllocator()), reproducing the inlined
// sbHasLinearAllocator assert (in the accessor) + mbHasAllocator assert + heap-forward +
// miFreeTotal += 12 (the tagged 3-arg Free) in the correct order. GetEaStlAllocator() and
// the 3-arg tagged Free are declared-only cross-TU seams (bodied in their own homes); they
// link fine under the compile-only gate. The vector helpers DoAllocate/DoFree are likewise
// separate cross-TU bl targets (own TUs).
// =============================================================================

namespace Attrib
{

// @ 0x828054B0 -- eastl::list<const Attrib::Class*, AttribSysPackageAllocator>::DoClear()
// (DWARF list.h:290). Walk the ring from the first real node (mNode.mpNext) back to the
// sentinel (&mNode), freeing each node through the static EASTL AttribSys package
// allocator. Called by Attrib::DatabasePrivate::~DatabasePrivate.
AttribListBase* AttribListClearNodes(AttribListBase* lpList)
{
    AttribListNode* lpSentinel = &lpList->mNode;
    for (AttribListNode* lpNode = lpList->mNode.mpNext; lpNode != lpSentinel; )
    {
        AttribListNode* lpDoomed = lpNode;
        lpNode = lpNode->mpNext;

        CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Free(
            lpDoomed, KI_ATTRIB_LIST_NODE_SIZE, NULL);
    }
    return lpList;
}

// @ 0x82803FC8 -- eastl::list<const Attrib::Class*, AttribSysPackageAllocator>::DoFreeNode
// (DWARF list.h:284). Hand one detached node back to the static EASTL AttribSys package
// allocator. X360 receives (a1 = list this [unused], a2 = node); only the node is forwarded
// to the same inlined free as DoClear. Unconditional (no null-guard). Called by
// Attrib::DatabasePrivate::CollectGarbageBag<Attrib::Class>.
void AttribListFreeNode(void* /*lpUnusedList*/, void* lpNode)
{
    CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Free(
        lpNode, KI_ATTRIB_LIST_NODE_SIZE, NULL);
}

// @ 0x82809228 / 0x828091A8 -- eastl::list<const Class*/Collection*,
// AttribSysPackageAllocator>::DoInsertValue's allocate-node half (one behaviour,
// two pointer-type instantiations): carve one list node from the static EASTL
// AttribSys package allocator, store the value, and hand it back for the caller
// to link into the ring (the QueueForDelete push sites). The X360 node is 12
// bytes; sizeof-based here for the widened x64 node, mirroring the DoClear/
// DoFreeNode accounting.
AttribListNode* AttribListAllocateNode(AttribListBase* /*lpList*/, void* const* lppValue)
{
    AttribListNode* lpNode = static_cast<AttribListNode*>(
        CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Malloc(
            sizeof(AttribListNode), 0));
    lpNode->mpNext = NULL;
    lpNode->mpPrev = NULL;
    lpNode->mpValue = *lppValue;
    return lpNode;
}

// @ 0x8280C258 -- eastl::vector<const Attrib::TypeDesc*, AttribSysPackageAllocator>::reserve(n)
// (DWARF vector.h:126; instantiated for Attrib::TypeDescPtrVec). If n exceeds the current
// capacity ((mpCapacityEnd - mpBegin) elements), allocate an n-element buffer, copy the live
// [mpBegin, mpEnd) bytes across, free the old buffer (by its old element capacity), then
// repoint {mpBegin, mpCapacityEnd, mpEnd}. Otherwise a no-op. Called by
// Attrib::DatabasePrivate::DatabasePrivate.
AttribVectorBase* AttribVectorReserve(AttribVectorBase* lpVector, unsigned int luCapacity)
{
    // NB the X360 divides the byte spans by the 4-byte element size (srawi ,2);
    // the elements are pointers, so the x64 build divides by the widened
    // sizeof(void*) to keep the same element math.
    AttribVectorBase* lpSelf     = lpVector;
    void**            lpOldBegin = lpVector->mpBegin;

    const s32 liCurrentCapacityElems =
        static_cast<s32>(lpVector->mpCapacityEnd - lpVector->mpBegin);

    if (luCapacity > static_cast<unsigned int>(liCurrentCapacityElems))
    {
        void** lpOldEnd = lpVector->mpEnd;

        void** lpNewBuffer =
            static_cast<void**>(AttribVectorAllocate(lpVector, luCapacity));

        memcpy(lpNewBuffer, lpOldBegin,
               static_cast<size_t>(reinterpret_cast<u8*>(lpOldEnd) -
                                   reinterpret_cast<u8*>(lpOldBegin)));

        // Free the old buffer by its still-live element capacity (recomputed from
        // begin/capacityEnd @ 0x8280C29C-0x8280C2B0).
        const s32 liOldCapacityElems =
            static_cast<s32>(lpVector->mpCapacityEnd - lpVector->mpBegin);
        AttribVectorFree(lpSelf, liOldCapacityElems);

        // Live-element count recovered from old begin/end captured (r9/r10) after the
        // free but before the mpBegin store.
        void** lpPrevBegin = lpSelf->mpBegin;
        void** lpPrevEnd   = lpSelf->mpEnd;
        const s32 liLiveElems = static_cast<s32>(lpPrevEnd - lpPrevBegin);

        lpSelf->mpBegin       = lpNewBuffer;
        lpSelf->mpCapacityEnd = lpNewBuffer + luCapacity;
        lpSelf->mpEnd         = lpNewBuffer + liLiveElems;
    }
    return lpSelf;
}

// The cross-TU vector storage pair the reserve path calls (their X360 homes are
// the eastl::vector template instantiations): DoAllocate @ 0x8280C288 carves
// luCount pointer slots from the AttribSys package allocator via the generic
// hook; DoFree @ 0x82805348 hands the old buffer back through the shared
// census-free (byte counts are element-size-based, widened on x64).
void* AttribVectorAllocate(AttribVectorBase* /*lpVector*/, unsigned int luCount)
{
    return Attrib::Alloc(sizeof(void*) * luCount, NULL);
}

AttribVectorBase* AttribVectorFree(AttribVectorBase* lpVector, s32 liOldCapacityElems)
{
    Attrib::HashMapTablePolicy::FreeWithCensusIf(
        lpVector->mpBegin, sizeof(void*) * static_cast<size_t>(liOldCapacityElems), NULL);
    return lpVector;
}

// =============================================================================
// Export-policy "should never happen" guards -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (AttribSys v1.2.1.2, attribdatabase.cpp). A Class or Collection is never entered into the
// export table, so the Class/Collection export policies' per-export Clean / Deinitialize
// hooks (and the Collection's IsReferenced) are dead paths that only fire an assert if ever
// reached. Each X360 body is exactly an unconditional Begin/Fire/EndAssert of a verbatim
// rodata message (== CGS_ASSERT(false, msg)); the parameters are untouched. The remaining
// virtuals declared on these policies (IsExported/Initialize/AnyReferences/PrepareToClean/
// PrepareToDeinitialize, and the Class IsReferenced) carry real per-attribute logic and are
// reconstructed in their own AttribSys TUs.
// =============================================================================

// @ 0x82805620 (attribdatabase.cpp:274). Classes hold no export entries, so a per-export
// Clean is a programming error.
void ClassExportPolicy::Clean(Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "Classes should not have export entries!\n");
}

// @ 0x82805660 (attribdatabase.cpp:292). Same guard on the per-export Deinitialize hook.
void ClassExportPolicy::Deinitialize(Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "Classes should not have export entries!\n");
}

// @ 0x828056A0 (attribdatabase.cpp:341). A Collection is never placed in the export table,
// so IsReferenced can only be reached in error; it asserts and reports "not referenced".
bool CollectionExportPolicy::IsReferenced(const Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "Collections should not be in export table!\n");
    return false;
}

// @ 0x828056E0 (attribdatabase.cpp:366). Collections carry no export entries either; the
// per-export Clean hook is a dead path. (Shares the Class rodata message.)
void CollectionExportPolicy::Clean(Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "Classes should not have export entries!\n");
}

// @ 0x82805720 (attribdatabase.cpp:391). Same guard on the Collection per-export
// Deinitialize hook.
void CollectionExportPolicy::Deinitialize(Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "Classes should not have export entries!\n");
}

// =============================================================================
// Attrib::Database::CollectGarbage @ 0x8280E5E0 -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (AttribSys v1.2.1.2). Drain the database's two deferred-delete rings (collections, then
// classes) until neither still holds a queued node, then -- if the database was flagged to
// self-destruct on the last vault release -- destroy the singleton. Called by
// Attrib::Vault::Deinitialize, CgsAttribSys::VaultSlot::DoUnload,
// Attrib::DatabaseExportPolicy::Deinitialize and CgsAttribSys::AttribSysModule::Update.
//
// Wave note (2026-07-12): this was previously blocked on the un-homed DatabasePrivate layout
// and the CollectGarbageBag<> templates. DatabasePrivate is now homed (attribdatabaseprivate.h)
// and CollectGarbageBag<Attrib::Collection> is homed as the DatabasePrivate_CollectCollection
// GarbageBag free-function seam (attribdatabaseprivate.cpp); its Class twin is declared here as
// the matching cross-TU seam (its body lands with the Class garbage-bag TU), exactly as the
// committed AttribSys seam convention handles not-yet-landed helpers.
// =============================================================================

// gDatabaseSelfDestruct (X360 byte_83011BB2; DWARF attribdatabase.cpp:111). Set by
// Attrib::DatabaseExportPolicy::Deinitialize on the final vault drop and consumed + cleared
// here once the garbage bags are empty, so the deferred collection destroys the database
// singleton. Defined in this TU (its X360 home).
bool gDatabaseSelfDestruct = false;

// Attrib::DatabasePrivate::CollectGarbageBag<Attrib::Collection> @ 0x8280E3C0 (homed in
// attribdatabaseprivate.cpp) and its Attrib::DatabasePrivate::CollectGarbageBag<Attrib::Class>
// twin (bodied in the Class garbage-bag TU). Both drain one intrusive eastl::list ring; declared
// here as cross-TU seams (resolved at link), matching the DatabasePrivate_* seam convention.
AttribListBase* DatabasePrivate_CollectCollectionGarbageBag(AttribListBase* lpGarbageList);
AttribListBase* DatabasePrivate_CollectClassGarbageBag(AttribListBase* lpGarbageList);

void Database::CollectGarbage()
{
    // this->mPrivates (X360 this+4). The two deferred-delete rings are the self-referential
    // eastl::list sentinels the DatabasePrivate ctor installs (X360 +0x6C collections /
    // +0x8C classes) -- the same rings QueueForDelete<Collection>/<Class> push onto. Named
    // members now that DatabasePrivate's x64 layout is real (attribdatabaseprivate.h).
    DatabasePrivate* lpPrivates = const_cast<DatabasePrivate*>(&mPrivates);
    AttribListBase* lpCollectionGarbage = &lpPrivates->mGarbageCollections;
    AttribListBase* lpClassGarbage      = &lpPrivates->mGarbageClasses;

    // Drain both bags until neither ring holds a queued node. The X360 walks each ring counting
    // its length, but only ever tests the count against zero -- i.e. whether the ring is empty.
    while (lpCollectionGarbage->mNode.mpNext != &lpCollectionGarbage->mNode ||
           lpClassGarbage->mNode.mpNext != &lpClassGarbage->mNode)
    {
        DatabasePrivate_CollectCollectionGarbageBag(lpCollectionGarbage);
        DatabasePrivate_CollectClassGarbageBag(lpClassGarbage);
    }

    if (gDatabaseSelfDestruct)
    {
        // Flagged for teardown: destroy the database singleton now the bags are empty. The X360
        // inlines the IsInitialized() assert (attribsys.h:649) before the deleting-destructor
        // virtual call (**sThis)(sThis, 1); the virtual `delete` below reproduces that call.
        Database* lpDatabase = sThis;
        if (lpDatabase == NULL)
        {
            CGS_ASSERT(false, "Attribute database not initialized.");
            lpDatabase = sThis;
        }
        if (lpDatabase != NULL)
            delete lpDatabase;

        sThis = NULL;
        gDatabaseSelfDestruct = false;
    }
}

// =============================================================================
// The export-policy singletons + granularity type keys (attribdatabase.cpp:396-405)
// and the export-policy bring-up (Attrib::Database::GetExportPolicies @0x8280DC70),
// the three policies' Initialize legs, and the DatabasePrivate registry ctor
// (@0x8280C598) -- reconstructed from BURNOUT_X360_ARTIST.XEX (attrib-sdk wave
// 2026-07-27).
// =============================================================================

// The granularity type keys (DWARF attribdatabase.cpp:396/399/402). The X360
// static initializers hash the LoadData type names (values verified against the
// serialised schema/world-vault ExpN entries: 0x0B38846845E9C175 /
// 0x2A7895AC4A876152 / 0xAD303B8F42B3307E).
TypeID gDatabaseType = StringToKey64("Attrib::DatabaseLoadData",
                                     sizeof("Attrib::DatabaseLoadData") - 1,
                                     KU_ATTRIB_STRING_TO_KEY_SEED);
TypeID gClassType = StringToKey64("Attrib::ClassLoadData",
                                  sizeof("Attrib::ClassLoadData") - 1,
                                  KU_ATTRIB_STRING_TO_KEY_SEED);
TypeID gCollectionType = StringToKey64("Attrib::CollectionLoadData",
                                       sizeof("Attrib::CollectionLoadData") - 1,
                                       KU_ATTRIB_STRING_TO_KEY_SEED);

// The policy singletons + the process export-policy table (attribdatabase.cpp:
// 397/400/403/405; X360 dword_83011BD0/BD4/BD8 + off_83011BDC).
DatabaseExportPolicy*   gDatabaseExportPolicy   = NULL;
ClassExportPolicy*      gClassExportPolicy      = NULL;
CollectionExportPolicy* gCollectionExportPolicy = NULL;
ExportManager*          gExportPolicies         = NULL;

// ---- the trivial policy virtuals the X360 linker ICF-folded ---------------
// DatabaseExportPolicy::IsExported == the folded `return true` (0x82C296C8):
// the database payload always claims an export slot.
bool DatabaseExportPolicy::IsExported(const TypeID&) { return true; }
// Class/Collection IsExported == the folded `return false` (0x827E2F38): class
// and collection payloads never occupy vault export slots (their Initialize
// builds registry objects instead).
bool ClassExportPolicy::IsExported(const TypeID&) { return false; }
bool CollectionExportPolicy::IsExported(const TypeID&) { return false; }
// DatabaseExportPolicy AnyReferences/IsReferenced-fold + empty clean hooks
// (vtable slots [3]/[5]/[6]/[7] == the folded `return false` / empty body).
bool DatabaseExportPolicy::AnyReferences(const Vault&) { return false; }
void DatabaseExportPolicy::PrepareToClean(Vault&) {}
void DatabaseExportPolicy::Clean(Vault&, const TypeID&, const ExportID&) {}
void DatabaseExportPolicy::PrepareToDeinitialize(Vault&) {}
// ClassExportPolicy::IsReferenced == the folded `return false` (0x827E2F38).
bool ClassExportPolicy::IsReferenced(const Vault&, const TypeID&, const ExportID&)
{
    return false;
}

// ---- the deferred heavy clean/teardown virtuals (own TUs; unreached on the
//      register path this wave lands) --------------------------------------
bool DatabaseExportPolicy::IsReferenced(const Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "DatabaseExportPolicy::IsReferenced @0x82807EC0: deferred TU");
    return false;
}
void DatabaseExportPolicy::Deinitialize(Vault&, const TypeID&, const ExportID&)
{
    CGS_ASSERT(false, "DatabaseExportPolicy::Deinitialize @0x8280F5D0: deferred TU");
}
bool ClassExportPolicy::AnyReferences(const Vault&)
{
    CGS_ASSERT(false, "ClassExportPolicy::AnyReferences @0x8280B2F0: deferred TU");
    return false;
}
void ClassExportPolicy::PrepareToClean(Vault&)
{
    CGS_ASSERT(false, "ClassExportPolicy::PrepareToClean @0x8280B450: deferred TU");
}
void ClassExportPolicy::PrepareToDeinitialize(Vault&)
{
    CGS_ASSERT(false, "ClassExportPolicy::PrepareToDeinitialize @0x8280CB78: deferred TU");
}
bool CollectionExportPolicy::AnyReferences(const Vault&)
{
    CGS_ASSERT(false, "CollectionExportPolicy::AnyReferences @0x8280B728: deferred TU");
    return false;
}
void CollectionExportPolicy::PrepareToClean(Vault&)
{
    CGS_ASSERT(false, "CollectionExportPolicy::PrepareToClean @0x8280B9F8: deferred TU");
}
void CollectionExportPolicy::PrepareToDeinitialize(Vault&)
{
    CGS_ASSERT(false, "CollectionExportPolicy::PrepareToDeinitialize @0x8280CD80: deferred TU");
}

// Attrib::ClassStaticDesc::GetStatic (generated code, codegen area). The PC
// generated static-class tables are not recovered yet, so the search finds
// nothing. [FLAG] no schema class carries static data (the schema PtrN has no
// mStaticData fixups), so the copy step is a data-attested no-op; recover the
// generated table with the codegen TU when a static-bearing vault appears.
const ClassStaticDesc* ClassStaticDesc::GetTable(unsigned int& lruCount)
{
    lruCount = 0;
    return NULL;
}
const ClassStaticDesc* ClassStaticDesc::GetStatic(::Attribute::Key luKey)
{
    (void)luKey;
    unsigned int luCount = 0;
    const ClassStaticDesc* lpTable = GetTable(luCount);
    (void)lpTable;
    return NULL;
}

// @ 0x8280DC70 -- Attrib::Database::GetExportPolicies. First call builds the
// process export-policy set: the 3-row ExportManager, the three policy
// singletons, one AddExportPolicy per granularity key, then a sort of the pair
// table by TypeID (the X360 runs the eastl quick_sort instantiation; three rows
// == a couple of swaps). NB `this` is UNTOUCHED -- the X360 site works even
// when called through the not-yet-initialized database singleton (RegisterSchema
// runs it before the DatabasePrivate exists).
ExportManager& Database::GetExportPolicies()
{
    if (gExportPolicies == NULL)
    {
        void* lpManagerBlock = Attrib::Alloc(sizeof(ExportManager), NULL);
        gExportPolicies = lpManagerBlock != NULL
                              ? new (lpManagerBlock) ExportManager(3u)
                              : NULL;

        void* lpBlock = Attrib::Alloc(sizeof(DatabaseExportPolicy), NULL);
        gDatabaseExportPolicy = lpBlock != NULL ? new (lpBlock) DatabaseExportPolicy() : NULL;
        lpBlock = Attrib::Alloc(sizeof(ClassExportPolicy), NULL);
        gClassExportPolicy = lpBlock != NULL ? new (lpBlock) ClassExportPolicy() : NULL;
        lpBlock = Attrib::Alloc(sizeof(CollectionExportPolicy), NULL);
        gCollectionExportPolicy = lpBlock != NULL ? new (lpBlock) CollectionExportPolicy() : NULL;

        gExportPolicies->AddExportPolicy(gDatabaseType, gDatabaseExportPolicy);
        gExportPolicies->AddExportPolicy(gClassType, gClassExportPolicy);
        gExportPolicies->AddExportPolicy(gCollectionType, gCollectionExportPolicy);
        gExportPolicies->SortPolicies();
    }
    return *gExportPolicies;
}

// @ 0x8280CAC8 -- DatabaseExportPolicy::Initialize: the schema's database export.
// Builds the DatabasePrivate registry over the serialised DatabaseLoadData,
// installs it as the process singleton, takes a vault reference, and registers
// the live registry as the vault's 'Attrib::Database' export.
void DatabaseExportPolicy::Initialize(Vault& lrVault, const TypeID& lrType,
                                      const ExportID& lrExport, void* lpData,
                                      unsigned int luSize)
{
    CGS_ASSERT(luSize >= 0x10, "Invalid DatabaseLoadData.");

    void* lpBlock = Attrib::Alloc(sizeof(DatabasePrivate), NULL);
    Database::sThis = lpBlock != NULL
        ? new (lpBlock) DatabasePrivate(*static_cast<const DatabaseLoadData*>(lpData))
        : NULL;

    lrVault.AddRef(0);
    lrVault.Export(lrType, lrExport, Database::sThis, 0);
}

// @ 0x8280EE38 -- ClassExportPolicy::Initialize: one schema class export. When
// the class is not already registered: copy any serialised static block over the
// generated static struct (none in the shipped schema), then build the
// ClassPrivate over the serialised ClassLoadData (it self-registers with the
// database class table).
void ClassExportPolicy::Initialize(Vault& lrVault, const TypeID& /*lrType*/,
                                   const ExportID& /*lrExport*/, void* lpData,
                                   unsigned int luSize)
{
    CGS_ASSERT(luSize >= 0x28, "Invalid ClassLoadData.");

    DatabasePrivate* lpPrivates = GetDatabasePrivate();
    const ClassLoadData* lpLoad = static_cast<const ClassLoadData*>(lpData);

    if (lpPrivates->mClasses.Find(lpLoad->mClass) == NULL)
    {
        if (lpLoad->mStaticData != 0 && lpLoad->mStaticSize != 0)
        {
            // The X360 looks the class up in the generated static-class table by
            // the key's low doubleword-half and copies the serialised static
            // block over the generated struct (clamped to the generated size).
            const ClassStaticDesc* lpStatic =
                ClassStaticDesc::GetStatic(static_cast<::Attribute::Key>(lpLoad->mClass));
            if (lpStatic != NULL)
            {
                unsigned int luCopy = lpLoad->mStaticSize;
                if (luCopy >= lpStatic->mSize)
                    luCopy = lpStatic->mSize;
                memcpy(lpStatic->mStruct, lpLoad->GetStaticData(), luCopy);
            }
        }

        void* lpBlock = Attrib::Alloc(sizeof(ClassPrivate), NULL);   // X360 Alloc(56)
        if (lpBlock != NULL)
            new (lpBlock) ClassPrivate(*lpLoad, &lrVault);
    }
}

// The serialised CollectionLoadData now lives beside Attrib::Collection itself
// (attribinstance.h) -- the Collection load ctor @0x82809740 is homed in
// attribcollection.cpp as of the collection-load wave, so the two travel together.

// @ 0x8280A180 -- CollectionExportPolicy::Initialize: one data-vault collection
// export (the world vault's boostparams collections). Requires the owning class
// to be registered; skips when the collection already exists; otherwise builds
// the Collection over the serialised CollectionLoadData.
void CollectionExportPolicy::Initialize(Vault& lrVault, const TypeID& /*lrType*/,
                                        const ExportID& /*lrExport*/, void* lpData,
                                        unsigned int luSize)
{
    CGS_ASSERT(luSize >= 0x30, "Invalid CollectionLoadData.");

    DatabasePrivate* lpPrivates = GetDatabasePrivate();
    const CollectionLoadData* lpLoad = static_cast<const CollectionLoadData*>(lpData);

    Attrib::Class* lpClass = lpPrivates->mClasses.Find(lpLoad->mClass);
    if (lpClass != NULL)
    {
        // The X360 probes the class's collection table for the key first.
        ClassPrivate* lpClassPrivate =
            reinterpret_cast<ClassPrivate*>(lpClass)->mpPrivates;
        if (lpClassPrivate->mCollections.Find(lpLoad->mKey) == NULL)
        {
            void* lpBlock = Attrib::Alloc(sizeof(Collection), NULL);   // X360 Alloc(40)
            if (lpBlock != NULL)
                new (lpBlock) Collection(*lpLoad, &lrVault);
        }
    }
}

// =============================================================================
// Attrib::DatabasePrivate + the registry containers.
// =============================================================================

// Attrib::ClassTable ctor (inlined into the DatabasePrivate ctor on the X360):
// zero the VecHashMap header, then size the bucket array when a reserve is given.
ClassTable::ClassTable(unsigned int luReserve)
{
    ZeroHeaderForConstruct();
    if (luReserve != 0)
        Reserve(luReserve);
}

// TypeDescPtrVec::Reserve -- the ctor's up-front capacity carve (the committed
// AttribVectorReserve helper over the shared control-block shape).
void TypeDescPtrVec::Reserve(unsigned int luCapacity)
{
    AttribVectorReserve(reinterpret_cast<AttribVectorBase*>(this), luCapacity);
}

// TypeDescPtrVec::PushBack -- the eastl::vector push the X360 inlines at the two
// ctor sites: grow through the committed AttribVector helpers when full, then
// append.
void TypeDescPtrVec::PushBack(const TypeDesc* lpDesc)
{
    if (mpEnd >= mpCapacityEnd)
    {
        // Grow via the committed reserve helper (doubling from the current
        // capacity; the ctor pre-reserves numTypes+1 so this is the safety net).
        AttribVectorBase* lpBase = reinterpret_cast<AttribVectorBase*>(this);
        const unsigned int luCapacity =
            static_cast<unsigned int>(mpCapacityEnd - mpBegin);
        AttribVectorReserve(lpBase, luCapacity != 0 ? 2u * luCapacity : 4u);
    }
    *mpEnd = lpDesc;
    ++mpEnd;
}

// TypeTable (the eastl::set<TypeDesc> seam) -- node-stable keyed insert/find
// with nodes from the AttribSys package allocator; see attribdatabaseprivate.h.
void TypeTable::Construct()
{
    mpRoot = NULL;
    muSize = 0;
}

TypeDesc* TypeTable::Insert(const TypeDesc& lrDesc)
{
    SetNode** lppSlot = &mpRoot;
    while (*lppSlot != NULL)
    {
        SetNode* lpNode = *lppSlot;
        if (lrDesc.mType < lpNode->mValue.mType)
            lppSlot = &lpNode->mpLeft;
        else if (lpNode->mValue.mType < lrDesc.mType)
            lppSlot = &lpNode->mpRight;
        else
            return &lpNode->mValue;   // unique-key set: keep the existing node
    }

    SetNode* lpNew = static_cast<SetNode*>(Attrib::Alloc(sizeof(SetNode), NULL));
    lpNew->mpLeft = NULL;
    lpNew->mpRight = NULL;
    lpNew->mValue = lrDesc;
    *lppSlot = lpNew;
    ++muSize;
    return &lpNew->mValue;
}

const TypeDesc* TypeTable::Find(u64 luType) const
{
    const SetNode* lpNode = mpRoot;
    while (lpNode != NULL)
    {
        if (luType < lpNode->mValue.mType)
            lpNode = lpNode->mpLeft;
        else if (lpNode->mValue.mType < luType)
            lpNode = lpNode->mpRight;
        else
            return &lpNode->mValue;
    }
    return NULL;
}

// @ 0x8280C598 -- DatabasePrivate ctor: build the registry from the schema's
// serialised DatabaseLoadData. Registers the class table (reserved to the
// schema's class count), the by-index compiled-type vector (numTypes + 1 for
// the NULL type), the keyed type set (the NULL type, then one TypeDesc per
// schema typename: key = hash64 of the name via TypeDesc::NameToType, byte size
// from the load record's size table, handler from the generated
// TypeDesc::Lookup), the two garbage rings, and primes the default data area.
Attrib::DatabasePrivate::DatabasePrivate(const DatabaseLoadData& lrLoad)
    : Database(*this)
    , mClasses(lrLoad.mNumClasses)
{
    mCompiledTypes.mpBegin = NULL;
    mCompiledTypes.mpEnd = NULL;
    mCompiledTypes.mpCapacityEnd = NULL;
    mTypes.Construct();

    // Garbage rings: self-linked sentinels.
    mGarbageCollections.mNode.mpNext = &mGarbageCollections.mNode;
    mGarbageCollections.mNode.mpPrev = &mGarbageCollections.mNode;
    mGarbageClasses.mNode.mpNext = &mGarbageClasses.mNode;
    mGarbageClasses.mNode.mpPrev = &mGarbageClasses.mNode;

    // The X360 body re-Reserves the class table to the load count (the ctor
    // seed already built it; Reserve asserts the table is not fixed-alloc).
    mClasses.Reserve(lrLoad.mNumClasses);

    mNumCompiledTypes = lrLoad.mNumTypes + 1;
    mCompiledTypes.Reserve(mNumCompiledTypes);

    // Prime the shared default-data area for the largest schema type payload.
    Attrib::DefaultDataArea(lrLoad.mDefaultDataSize);

    // The NULL type: index 0, empty name, size 0, no handler.
    TypeDesc lNullDesc;
    lNullDesc.mType = 0;
    lNullDesc.mName = "";
    lNullDesc.mSize = 0;
    lNullDesc.mIndex = 0;
    lNullDesc.mHandler = NULL;
    mCompiledTypes.PushBack(mTypes.Insert(lNullDesc));

    // One TypeDesc per schema typename (NUL-string walk; sizes parallel).
    const u32* lpSizes = lrLoad.GetTypeSizes();
    const char* lpcName = lrLoad.GetTypenames();
    for (unsigned int luIndex = 0; luIndex < lrLoad.mNumTypes; ++luIndex)
    {
        TypeDesc lDesc;
        lDesc.mType = TypeDesc_NameToType64(lpcName);
        lDesc.mName = lpcName;
        lDesc.mSize = lpSizes[luIndex];
        lDesc.mIndex = mCompiledTypes.Size();
        lDesc.mHandler = static_cast<ITypeHandler*>(TypeDesc_LookupHandler(lDesc.mType));
        mCompiledTypes.PushBack(mTypes.Insert(lDesc));

        while (*lpcName != '\0')
            ++lpcName;
        ++lpcName;
    }
}

// @ 0x82808118 -- Attrib::Database::GetTypeDesc: the keyed type-registry lookup
// (also resolves the generated handler via TypeDesc::Lookup on the X360 -- the
// stored row already carries it here). A miss falls back to the NULL type row
// (mCompiledTypes[0], the sentinel the DatabasePrivate ctor seeds first).
const TypeDesc& Database::GetTypeDesc(u64 luType) const
{
    const DatabasePrivate* lpPrivates = &mPrivates;
    const TypeDesc* lpDesc = lpPrivates->mTypes.Find(luType);
    if (lpDesc == NULL)
        lpDesc = lpPrivates->mCompiledTypes.mpBegin[0];
    return *lpDesc;
}

// Attrib::DatabasePrivate::CollectGarbageBag<Attrib::Class> @ 0x8280F328 -- the
// Class twin of the committed Collection bag drain (attribdatabaseprivate.cpp):
// destroy each queued class whose shared layout-table refcount dropped to zero,
// return its object to the package allocator, then unlink + free the node.
AttribListBase* DatabasePrivate_CollectClassGarbageBag(AttribListBase* lpGarbageList)
{
    while (lpGarbageList->mNode.mpNext != &lpGarbageList->mNode)
    {
        AttribListNode* lpNode = lpGarbageList->mNode.mpNext;
        ClassPrivate* lpClass = reinterpret_cast<ClassPrivate*>(lpNode->mpValue);

        CGS_ASSERT(lpClass != NULL, "NULL object found in garbage bag.");

        if (lpClass->mLayoutTable.GetRefCount() == 0)
        {
            lpClass->~ClassPrivate();
            // Unconditional census-and-free of the class object (X360 56 bytes;
            // sizeof-based for the widened x64 layout).
            HashMapTablePolicy::FreeWithCensus(lpClass, sizeof(ClassPrivate), NULL);
        }

        lpNode->mpPrev->mpNext = lpNode->mpNext;
        lpNode->mpNext->mpPrev = lpNode->mpPrev;
        AttribListFreeNode(lpGarbageList, lpNode);
    }
    return lpGarbageList;
}

// GetDatabasePrivate -- the typed private-impl accessor every registry site
// inlines on the X360 (off_83011BC4 + the initialization assert + ->mPrivates).
// The live registry IS its own privates (the ctor's Database(*this) base), so
// the accessor hands the singleton back as a DatabasePrivate.
DatabasePrivate* GetDatabasePrivate()
{
    Database* lpDatabase = Database::sThis;
    if (lpDatabase == NULL)
    {
        CGS_ASSERT(false, "Attribute database not initialized.");
        lpDatabase = Database::sThis;
    }
    return const_cast<DatabasePrivate*>(&lpDatabase->mPrivates);
}

} // namespace Attrib
