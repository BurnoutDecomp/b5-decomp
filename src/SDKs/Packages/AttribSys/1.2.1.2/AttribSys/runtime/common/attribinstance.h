#pragma once

// AttribSys runtime — Attrib::Instance and the Collection it handles.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). An earlier SDK
// revision drifts from the X360 spine: a 64-bit Key and a different Instance layout
// (mOwner@0/mCollection@4/mLayoutPtr@8). The X360 build is the target, so this header
// carries the X360 shapes (32-bit Key; the 16-byte Instance the recovered bodies use)
// and declares ONLY the methods the X360 ledger attests as real functions — the rich
// generated-class accessor API is inlined away in X360.
// Method bodies live in attribinstance.cpp; Collection internals are owned by the SDK.
#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribhashmap.h" // Attrib::HashMap base

namespace Attrib
{
    typedef u32 Key;   // X360: 32-bit class/attribute key (an earlier SDK rev was 64-bit).

    class Class;       // full definition in vechashmap.h; RefSpec::GetClass returns it by pointer.
    class Vault;       // full definition in attribloadandgo.h; a collection's source vault.
    struct Node;       // schema node (full definition in attribute.h); GetNode/GetData use it.
    struct TypeDesc;   // schema type descriptor (full definition in attribarray.h).

    // ------------------------------------------------------------------------
    // The serialised CollectionLoadData (DWARF attribprivate.h:167, members
    // :178-186; 48-byte head -- CollectionExportPolicy::Initialize's `>= 0x30`
    // guard). mLayout is a 4-byte pointer SLOT the vault PtrN fixups resolve
    // (the world vault points it at the collection's payload block in the bin;
    // the committed serialised-resource PointerFromU32 convention).
    //
    // The head is followed by mTypesLen 64-bit TYPE KEYS (the entry loop indexes
    // them with the per-entry type index) and then mNumEntries 16-byte entries:
    //   {u64 mKey, u32 muValue, u16 muTypeIndex, u8 mu8Flags, u8 pad}
    // (load ctor @0x82809740: `addi r26,r28,0x30` = the type table, entries at
    // r28 + 8*(mTypesLen+6) with a 16-byte stride).
    // ------------------------------------------------------------------------
    struct CollectionLoadData
    {
        u64 mKey;            // +0x00  collection key
        u64 mClass;          // +0x08  owning class key
        u64 mParent;         // +0x10  parent collection key (0 = none)
        u32 mTableReserve;   // +0x18  attribute-table bucket reserve
        u32 mTableKeyShift;  // +0x1C  attribute-table rotate-hash shift
        u32 mNumEntries;     // +0x20
        u16 mNumTypes;       // +0x24  highest valid type index (the load assert's bound)
        u16 mTypesLen;       // +0x26  length of the trailing type-key table
        u32 mLayout;         // +0x28  layout/payload slot (fixed up)
        u32 mPad;            // +0x2C

        // One serialised attribute entry.
        struct Entry
        {
            u64 mKey;         // +0x00  attribute key
            u32 muValue;      // +0x08  value word (offset into the layout block / inline)
            u16 muTypeIndex;  // +0x0C  index into the trailing type-key table
            u8  mu8Flags;     // +0x0E  node flag byte
            u8  mu8Pad;       // +0x0F
        };

        const u64* GetTypeKeys() const
        {
            return reinterpret_cast<const u64*>(this + 1);
        }
        const Entry* GetEntries() const
        {
            return reinterpret_cast<const Entry*>(GetTypeKeys() + mTypesLen);
        }
        void* GetLayout() const
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(mLayout));
        }
    };

    // Attrib::ScanForValidKey<Attrib::HashMap> @ 0x82803CC0 -- the key cursor the
    // collection walk is built on: the key of the first OCCUPIED bucket strictly
    // after luIndex, or 0 when the table has none left. (luIndex == 0xFFFFFFFF
    // starts the walk at bucket 0 -- the X360 `addi r4,r4,1` wrap.)
    u64 ScanForValidKey(const HashMap& lrMap, unsigned int luIndex);

    // Attrib::Collection -- a refcounted attribute table plus the collection metadata
    // (parent/default fallback, sub-collection, owning class, data area). It DERIVES from
    // Attrib::HashMap: the X360 passes a Collection* straight into HashMap::Release and the
    // +0x08 refcount ops, so the shared refcount (muRefCount @ +0x08) and the bucket table
    // are the HashMap base's; only the collection-specific metadata is added here. Byte
    // offsets in the comments are the X360-attested ones (the HashMap base occupies
    // 0x00..0x0B); member access is by name (semantic parity, not host byte-matching, since
    // the pointers widen on the host).
    struct Collection : public HashMap
    {
        Collection* mpParent;        // +0x0C  parent/default collection an unmodify falls back to
        // CORRECTED (2026-07-27, load-ctor asm @0x82809740 `ld r11,0(r28)` /
        // `std r11,0x10(r31)`): +0x10 is the collection's own 64-BIT KEY, not a
        // sub-collection pointer -- the load ctor stores CollectionLoadData::mKey
        // there and registers the collection into the class table under it, and
        // Instance::GetCollection @0x82802F40 returns it with a 64-bit `ld`.
        u64         mKey;            // +0x10  collection key
        Class*      mpClass;         // +0x18  owning class
        void*       mpData;          // +0x1C  this collection's attribute-data block
        // CORRECTED (same asm, `stw r30,0x20(r31)` with r30 = the ctor's Vault*
        // argument, immediately followed by `++vault->mRefCount`): +0x20 is the
        // SOURCE VAULT pointer (the mirror of ClassPrivate::mSource @+48), not a
        // "has no default" flag. Instance's ctor/Change test it for null -- a
        // collection with no source vault has no default layout to inherit.
        Vault*      mpSource;        // +0x20  vault this collection was loaded from

        // @ 0x82809740 -- the LOAD ctor: build a live collection from one serialised
        // CollectionLoadData export (the world vault's boostparams collections).
        // Body: attribcollection.cpp.
        Collection(const CollectionLoadData& lrLoad, Vault* lpSource);

        // Bump the shared refcount (asserts it has not saturated at 0xFFFF); returns this.
        Collection* AddRef();                       // @0x828028E0  (attribcollection.cpp)
        // Drop one shared reference via HashMap::Release; on the final drop, queue this
        // collection onto the attribute database's garbage list for deferred deletion.
        int         Release();                       // @0x8280C2E8  (attribcollection.cpp)
        // The real destructor (releases the attribute table this collection owns). Own
        // AttribSys ledger TU (todo); declared so the scalar-deleting-destructor thunk
        // (Collection_ScalarDeletingDtor @0x8280C510) links against it.
        ~Collection();

        // ---- attribute table access (bodies in attribcollection.cpp) -----------------
        // Resolve the schema Node for an attribute key, walking this collection's
        // parent/default chain and finally the owning class's shared layout table; reports
        // via lrpContainer which collection actually held the key (null when not found).
        // (Attrib::Node fully qualified: the inherited HashMap::Node would otherwise win.)
        // (WIDENED to the attested 64-bit attribute key: the X360 `mr r4,r29` into
        // HashMap::FindIndex carries the whole doubleword -- the file-scope
        // `typedef u32 Key` is an earlier-revision leftover.)
        Attrib::Node* GetNode(u64 luKey, const Collection*& lrpContainer) const; // @0x82804EF0
        // Raw data pointer for element luIndex of an attribute -- an array element for
        // array-typed attributes, or the instance-resolved pointer at index 0 otherwise.
        void* GetData(u64 luKey, unsigned int luIndex) const;               // @0x82804FD0
        // @ 0x828050D0 -- the key cursor: given the current key and whether the walk has
        // moved on to the owning class's shared layout table (lrbInLayoutTable, updated
        // in place), return the next attribute key visible through this collection, or 0
        // when the walk is finished. Walks this collection's own table first, then the
        // class layout table.
        u64  NextKey(u64 luKey, bool& lrbInLayoutTable) const;              // @0x828050D0
        // Tear the collection down: remove every attribute this collection owns and free
        // every inherited/laid-out attribute's backing data, asserting the count balances.
        void  Clear();                                                      // @0x8280AE60
        // Free one attribute's backing data: per-element type-handler release for arrays
        // then the whole-block free, or the single-value handler release + block free,
        // selected by the storage flags and the schema TypeDesc.
        void  FreeNodeData(bool lbIsArray, void* lpData, bool lbRequiresRelease,
                           const TypeDesc& lrTypeDesc) const;               // @0x8280A068
    };

    // The 16-byte attribute record Get copies out.
    struct AttributeValue { u32 mauWords[4]; };

    // A reference to another generated-class instance carried inside attribute data
    // (DWARF attribsys.h:735, members :762-764). X360 record: the 64-bit generated-
    // class key, the 64-bit collection key, then the resolved collection pointer --
    // 24 bytes with tail padding (Attrib::DefaultDataArea(0x18) is the null-element
    // fallback for RefSpec arrays, e.g. a shotgroup's ShotList). The class key is the
    // leading qword (BrnDirector::ShotSelector::GetCrashShot @0x822396F8 `ld 0(ref)` +
    // cmpld against the generated ClassKey constants).
    struct RefSpec
    {
        u64 GetClassKey() const { return mClassKey; }

        // GROWN for MomentPlayerStunt::Update @0x82272750 (the stunt moment builds
        // stack references to its "World_Signature_%i" ICE takes): raw-key
        // construction (the X360 composes {class key, StringToKey(guid)} in
        // registers with a null collection pointer), the refcounted assignment
        // (@0x8280DFB0 -- resolves/AddRefs the source's collection into the
        // destination), the resolved-ref drop (@0x8280DB60), and the
        // did-the-resolve-pin-a-collection check its callers Clean() on. The two
        // real bodies are DECLARATION-ONLY (the AttribSys runtime TUs).
        RefSpec() : mClassKey(0), mCollectionKey(0), mpCollectionPtr(0) {}
        RefSpec(u64 luClassKey, u64 luCollectionKey)
            : mClassKey(luClassKey), mCollectionKey(luCollectionKey), mpCollectionPtr(0) {}
        // Copy ctor @0x82803560 -- memberwise copy of the two keys + the resolved collection
        // pointer, then (if a collection was resolved) AddRef it by bumping its refcount,
        // asserting it has not saturated at 0xFFFF. Inlined in attribhashmap.h:622 on X360.
        RefSpec(const RefSpec& lrOther);              // @0x82803560
        RefSpec& operator=(const RefSpec& lrOther);   // @0x8280DFB0
        void Clean();                                  // @0x8280DB60
        bool HasResolvedCollection() const { return mpCollectionPtr != 0; }

        // GROWN for the AttribSys support TU (attribsupport.cpp). The three lazy resolvers
        // the X360 attests as real functions off this RefSpec:
        //   GetClass                 @0x828084B0 -- the ref's owning Attrib::Class: the class
        //                             held by an already-resolved collection (collection+0x18),
        //                             else looked up in the process database's class-registry
        //                             table by mClassKey (null when mClassKey is 0).
        //   GetCollection            @0x82808530 -- resolve+AddRef+cache the collection stored
        //                             under mCollectionKey in the class's table (null when
        //                             mCollectionKey is 0 or the class/collection is absent).
        //   GetCollectionWithDefault @0x828085C0 -- as GetCollection but with the class's
        //                             default-collection fallback (Class::GetCollectionWithDefault).
        const Class*      GetClass() const;                 // @0x828084B0
        const Collection* GetCollection();                  // @0x82808530
        const Collection* GetCollectionWithDefault();       // @0x828085C0

    private:
        u64               mClassKey;       // +0x00 (attribsys.h:762)
        u64               mCollectionKey;  // +0x08 (attribsys.h:763)
        const Collection* mpCollectionPtr; // +0x10 (attribsys.h:764)
    };

    // Collection helpers + generated-class helpers (each its own TU).
    Collection* Collection_AddRef(Collection* lpCollection, int liFlags);
    int         Collection_Release(Collection* lpCollection, int liFlags);
    void*       Collection_Get(void* lpOut, int liKey, int* lpName, int liArg);
    void*       Collection_GetData(Collection* lpCollection);
    int         RefSpec_GetCollectionWithDefault(int* lpRefSpec);
    // (third argument WIDENED to the collection KEY: Instance::GetCollection
    // @0x82802F40 hands over a 64-bit `ld` and the diagnostic formats it as a
    // %08x%08x hi/lo pair -- it was never a host pointer.)
    void        AssertOnClassCheck(int liClass, int liExpectedClass, u64 luCollectionKey);
    void*       DefaultDataArea(u32 luBytes);

    // A ref-counted handle onto a Collection. Construction/destruction maintain the
    // collection refcount; Change swaps the referenced collection; the accessors
    // forward into the collection. Data members are protected so the generated
    // Attrib::Gen::* classes can initialise their default layout (mpAttributeData).
    class Instance
    {
    public:
        Instance(Collection* lpCollection, void* lpOwner);
        ~Instance();
        Collection* Change(Collection* lpNewCollection);
        Collection* ChangeWithDefault(int* lpRefSpec);
        void*       Get(AttributeValue* pOut, int* lpName, int liArg);
        void*       GetAttributePointer();

        // Indexed array-attribute element lookup: the 64-bit attribute key (the
        // generated Hash:: constant widened with its type tag) + the element index.
        // REAL X360 symbol (Attrib::Instance::GetAttributePointer overload; called by
        // ShotSelector::GetCrashShot @0x82239894 with 0x7533C0E2_15246B49 = the
        // shotgroup ShotList key). Declaration-only (bodied with the AttribSys TUs).
        void*       GetAttributePointer(u64 luAttributeKey, u32 luIndex) const;
        int         GetClass() const;
        // @0x82802F40 -- the referenced collection's 64-bit KEY (`ld r3,0x10(r11)`),
        // 0 when the instance holds no collection. (Was modelled as a `void*`
        // sub-collection pointer; the load ctor pins +0x10 as Collection::mKey.)
        u64         GetCollection() const;

        // Raw-field accessors used by Attrib::Attribute (ctor @ 0x82805AF0 reads the
        // layout block; IsInherited @ 0x82803600 reads the resolved collection + flags).
        const Collection* GetResolvedCollection() const { return mpCollection; }
        void*             GetLayoutPointer() const { return mpAttributeData; }
        bool              IsModified() const { return (muFlags & 1u) != 0; }

    protected:
        Collection* mpCollection;     // +0
        void*       mpAttributeData;  // +4
        void*       mpOwner;          // +8
        u32         muFlags;          // +12

    private:
        void Unmodify();
    };
}
