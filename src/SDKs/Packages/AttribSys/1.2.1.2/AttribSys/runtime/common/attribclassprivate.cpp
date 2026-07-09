#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // HashMapTablePolicy::Free
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h"           // AttribListNode/Base + delete-queue seam

// AttribSys runtime -- Attrib::ClassPrivate bodies, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2):
//   Attrib::ClassPrivate::ClassPrivate  @ 0x8280EC80
//   Attrib::ClassPrivate::Release       @ 0x8280C370
//   Attrib::ClassPrivate::~ClassPrivate @ 0x8280F4D8
//
// The cross-TU helpers ClassPrivate depends on (the layout HashMap ctor/adder/releaser,
// the class-registry table, the collection table, the DatabasePrivate garbage queue, the
// Vault destructor, and the database-private accessor) live in their own AttribSys TUs.
// Until those land they are free-function trap stubs here, exactly as the sibling
// attribute.cpp / attribinstance.cpp reconstructions stub their own not-yet-landed
// helpers. HashMapTablePolicy::Free is the committed helper and is used directly.

namespace Attrib
{
    // ---- cross-TU trap stubs (own ledger TUs) -------------------------------
    void HashMap_Construct(void* lpHashMap, unsigned int luCount, u8 lu8KeyShift, u8 lu8Dynamic)
    {
        (void)lpHashMap; (void)luCount; (void)lu8KeyShift; (void)lu8Dynamic;
        __debugbreak();
    }

    void HashMap_Add(void* lpHashMap, u64 luKey, u64 luValue, u16 lu16Type,
                     int liArg4, unsigned int luFlags, char lcArg6, int liArg7)
    {
        (void)lpHashMap; (void)luKey; (void)luValue; (void)lu16Type;
        (void)liArg4; (void)luFlags; (void)lcArg6; (void)liArg7;
        __debugbreak();
    }

    int HashMap_Release(void* lpHashMap)
    {
        (void)lpHashMap;
        __debugbreak();
        return 0;
    }

    void ClassTable_Add(u16* lpTable, u32 luKey, void* lpClass)
    {
        (void)lpTable; (void)luKey; (void)lpClass;
        __debugbreak();
    }

    unsigned int ClassTable_Find(u16* lpTable, u32 luKey)
    {
        (void)lpTable; (void)luKey;
        __debugbreak();
        return 0;
    }

    void ClassTable_EraseAt(u16* lpTable, unsigned int luIndex)
    {
        (void)lpTable; (void)luIndex;
        __debugbreak();
    }

    void CollectionTable_Rebu()
    {
        __debugbreak();
    }

    void CollectionTable_Clear(void* lpTable)
    {
        (void)lpTable;
        __debugbreak();
    }

    // Attrib::DatabasePrivate::QueueForDelete<Attrib::Class> @ 0x8280BF78. The Class twin of the
    // Collection delete-queue push (attribcollection.cpp). Identical ring logic; the only
    // difference is the referenced-check: a Class carries its shared refcount indirectly -- the
    // X360 loads the ClassPrivate pointer at Class+0x08 and reads the u16 refcount at
    // ClassPrivate+0x18 (Class is opaque here, so this stays a byte-offset reach, per the
    // AttribSys convention). Reached from Attrib::ClassPrivate::Release on the final drop.
    void* DatabasePrivate_QueueClassForDelete(void* lpClass, void* lpGarbageList)
    {
        AttribListBase* lpQueue = reinterpret_cast<AttribListBase*>(lpGarbageList);

        // X360: refcount = *(u16*)( *(void**)(class + 0x08) + 0x18 ).
        bool lbNullOrReferenced = (lpClass == NULL);
        if (!lbNullOrReferenced)
        {
            const void* lpClassPrivate =
                *reinterpret_cast<void* const*>(reinterpret_cast<const u8*>(lpClass) + 0x08);
            const u16 lu16RefCount =
                *reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(lpClassPrivate) + 0x18);
            lbNullOrReferenced = (lu16RefCount != 0);
        }
        CGS_ASSERT(!lbNullOrReferenced,
                   "Cannot queue NULL or referenced object for delete.");

        // Already queued? (walk the ring; the X360 leaves r3 = the object, unused by the caller.)
        for (AttribListNode* lpNode = lpQueue->mNode.mpNext;
             lpNode != &lpQueue->mNode; lpNode = lpNode->mpNext)
        {
            if (lpNode->mpValue == lpClass)
                return lpClass;
        }

        // Allocate a node holding the class and link it at the tail (insert-before-sentinel).
        AttribListNode* lpNewNode = AttribListAllocateNode(lpQueue, &lpClass);
        lpNewNode->mpNext              = &lpQueue->mNode;
        lpNewNode->mpPrev              = lpQueue->mNode.mpPrev;
        lpQueue->mNode.mpPrev->mpNext  = lpNewNode;
        lpQueue->mNode.mpPrev          = lpNewNode;
        return lpNewNode;
    }

    // Attrib::Vault scalar deleting destructor -- real body now landed in the Vault home TU
    // (attribloadandgo.cpp @ 0x8280F098). ~ClassPrivate drops the source Vault's last
    // live-class reference here, so forward to the real deleting destructor rather than trap.
    // Vault is only forward-declared in this TU (attribclassprivate.h), so declare the free
    // helper locally and pass the block through as an (incomplete) Vault*.
    void* Vault_ScalarDeletingDtor(Vault* lpVault, int liDeleteFlag);

    void Vault_Destroy(void* lpVault, char lcDeleting)
    {
        Vault_ScalarDeletingDtor(reinterpret_cast<Vault*>(lpVault), lcDeleting);
    }

    void* GetDatabasePrivate()
    {
        __debugbreak();
        return NULL;
    }
}

// ============================================================================
// Attrib::ClassPrivate::ClassPrivate @ 0x8280EC80
// ============================================================================
// Store-for-store from the X360. Builds the private impl of an attribute Class:
// copies the 8-byte Class base out of the load descriptor, installs the self
// back-pointer, constructs the layout HashMap, zero-inits the collection table,
// seeds the definition/layout metadata, bumps the source Vault's live-class count,
// pushes every in-layout searchable definition into the layout HashMap, and finally
// registers the Class with the process attribute database's class table.
Attrib::ClassPrivate::ClassPrivate(const ClassLoadData& lrLoad, Vault* lpSource)
{
    u8* lpThis = reinterpret_cast<u8*>(this);
    const u8* lpLoad = reinterpret_cast<const u8*>(&lrLoad);

    // Copy the 8-byte Class base (mKey + mpPrivates) out of the load descriptor, then
    // point the self back-reference (this+8) at ourselves.
    *reinterpret_cast<u64*>(lpThis) = *reinterpret_cast<const u64*>(lpLoad);
    *reinterpret_cast<ClassPrivate**>(lpThis + 8) = this;

    // mLayoutTable @ +0x10 -- HashMap seeded from the load descriptor's layout
    // (count @+0x22, key-shift @+0x20), dynamic=1.
    HashMap_Construct(lpThis + 0x10,
                      *reinterpret_cast<const u16*>(lpLoad + 0x22),
                      static_cast<u8>(*reinterpret_cast<const u16*>(lpLoad + 0x20)),
                      1);

    // mCollections @ +0x1C -- zero the table header in place.
    *reinterpret_cast<u32*>(lpThis + 0x1C) = 0;
    *reinterpret_cast<u16*>(lpThis + 0x20) = 0;
    *reinterpret_cast<u16*>(lpThis + 0x22) = 0;
    *reinterpret_cast<u16*>(lpThis + 0x24) = 0;
    *reinterpret_cast<u16*>(lpThis + 0x26) = 0;

    const u32 luNumDefinitions = *reinterpret_cast<const u32*>(lpLoad + 0x08);
    if (luNumDefinitions != 0)
        CollectionTable_Rebu();

    *reinterpret_cast<u16*>(lpThis + 0x28) =
        static_cast<u16>(*reinterpret_cast<const u32*>(lpLoad + 0x1C)); // mLayoutSize
    *reinterpret_cast<u16*>(lpThis + 0x2A) =
        static_cast<u16>(*reinterpret_cast<const u32*>(lpLoad + 0x0C)); // mNumDefinitions
    *reinterpret_cast<void**>(lpThis + 0x2C) =
        *reinterpret_cast<void* const*>(lpLoad + 0x10);                 // mpDefinitions
    *reinterpret_cast<Vault**>(lpThis + 0x30) = lpSource;               // mpSource

    // mLayoutTable collection refcount (u16 @ +0x18) -- overflow guard + AddRef.
    u16* lpuRefCount = reinterpret_cast<u16*>(lpThis + 0x18);
    CGS_ASSERT(*lpuRefCount != 0xFFFF, "Exceeded collection refcount maximum!\n");
    ++*lpuRefCount;
    ++*reinterpret_cast<u32*>(*reinterpret_cast<u8**>(lpThis + 0x30) + 0x10); // mSource live-class count

    // Push every in-layout, searchable definition into the layout HashMap. The X360
    // re-reads the self back-pointer (this+8) each iteration.
    ClassPrivate* lpSelf = *reinterpret_cast<ClassPrivate**>(lpThis + 8);
    u8* lpSelfBytes = reinterpret_cast<u8*>(lpSelf);
    if (*reinterpret_cast<u16*>(lpSelfBytes + 0x2A) != 0)
    {
        u32 luByteOffset = 0;
        u32 luIndex = 0;
        do
        {
            u8* lpDef = *reinterpret_cast<u8**>(lpSelfBytes + 0x2C) + luByteOffset;
            const u8 lu8Flags = *reinterpret_cast<const u8*>(lpDef + 0x16);
            if ((lu8Flags & 2) != 0 && (lu8Flags & 8) == 0)
            {
                const u32 luAddFlags =
                    (static_cast<u32>(lu8Flags & 1) << 1) | ((lu8Flags & 2) != 0 ? 0x10u : 0x20u);
                HashMap_Add(lpSelfBytes + 0x10,
                            *reinterpret_cast<u64*>(lpDef + 0x00),
                            *reinterpret_cast<u64*>(lpDef + 0x08),
                            *reinterpret_cast<u16*>(lpDef + 0x10),
                            0,
                            luAddFlags,
                            1,
                            0);
            }
            ++luIndex;
            luByteOffset += 0x18;
        }
        while (luIndex < *reinterpret_cast<u16*>(lpSelfBytes + 0x2A));
    }

    // Register this Class with the process attribute database's class table (the X360
    // reads off_83011BC4->mPrivates(+4), asserting the database is initialized first).
    u8* lpPrivates = reinterpret_cast<u8*>(GetDatabasePrivate());
    ClassTable_Add(reinterpret_cast<u16*>(lpPrivates + 8),
                   *reinterpret_cast<u32*>(lpThis),
                   this);
}

// ============================================================================
// Attrib::ClassPrivate::Release @ 0x8280C370
// ============================================================================
// Drop one reference on the layout HashMap; once it hits zero, queue this Class for
// deferred deletion on the database's garbage list. Returns the HashMap release result
// (a byte-wide bool widened to int, exactly as the X360 threads r3 through).
int Attrib::ClassPrivate::Release()
{
    u8* lpThis = reinterpret_cast<u8*>(this);
    const int liReleased = static_cast<u8>(HashMap_Release(lpThis + 0x10));
    if (liReleased != 0)
    {
        u8* lpPrivates = reinterpret_cast<u8*>(GetDatabasePrivate());
        return static_cast<int>(reinterpret_cast<intptr_t>(
            DatabasePrivate_QueueClassForDelete(this, lpPrivates + 0x8C)));
    }
    return liReleased;
}

// ============================================================================
// Attrib::ClassPrivate::~ClassPrivate @ 0x8280F4D8
// ============================================================================
// Tear down the private Class impl: clear the layout HashMap's count, drop the source
// Vault's live-class refcount (deleting the Vault when it reaches zero), unregister the
// Class from the database class table, clear the collection table, then release the
// layout HashMap's bucket buffer.
Attrib::ClassPrivate::~ClassPrivate()
{
    u8* lpThis = reinterpret_cast<u8*>(this);

    // Clear mLayoutTable's count (u16 @ +0x16).
    *reinterpret_cast<u16*>(lpThis + 0x16) = 0;

    // Drop the source Vault's live-class refcount; delete it when it hits zero.
    u8* lpSource = *reinterpret_cast<u8**>(lpThis + 0x30);
    const u32 luNewRefs = *reinterpret_cast<u32*>(lpSource + 0x10) - 1;
    *reinterpret_cast<u32*>(lpSource + 0x10) = luNewRefs;
    if (luNewRefs == 0)
        Vault_Destroy(lpSource, 1);

    // Unregister the Class from the database class table.
    u8* lpPrivates = reinterpret_cast<u8*>(GetDatabasePrivate());
    u16* lpTable = reinterpret_cast<u16*>(lpPrivates + 8);
    const unsigned int luIndex = ClassTable_Find(lpTable, *reinterpret_cast<u32*>(lpThis));
    ClassTable_EraseAt(lpTable, luIndex);

    // Clear the collection table (mCollections @ +0x1C).
    CollectionTable_Clear(lpThis + 0x1C);

    CGS_ASSERT(*reinterpret_cast<u16*>(lpThis + 0x16) == 0,
               "Attrib::HashMap not empty when destroyed.");

    // Release the layout HashMap's bucket buffer. Free size = ROL4(capacity-shift, 4).
    void* lpBuckets = *reinterpret_cast<void**>(lpThis + 0x10);
    if (lpBuckets != nullptr)
    {
        const u32 lu32Shift = *reinterpret_cast<u16*>(lpThis + 0x14);
        const u32 luSize = (lu32Shift << 4) | (lu32Shift >> 28);
        HashMapTablePolicy::Free(lpBuckets, luSize);
    }
}
