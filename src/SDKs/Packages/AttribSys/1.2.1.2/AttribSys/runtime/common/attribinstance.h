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
        void*       mpSubCollection; // +0x10
        u8          mPad2[4];        // +0x14
        int*        mpClass;         // +0x18
        void*       mpData;          // +0x1C
        u32         muHasNoDefault;  // +0x20

        // Bump the shared refcount (asserts it has not saturated at 0xFFFF); returns this.
        Collection* AddRef();                       // @0x828028E0  (attribcollection.cpp)
        // Drop one shared reference via HashMap::Release; on the final drop, queue this
        // collection onto the attribute database's garbage list for deferred deletion.
        int         Release();                       // @0x8280C2E8  (attribcollection.cpp)
        // The real destructor (releases the attribute table this collection owns). Own
        // AttribSys ledger TU (todo); declared so the scalar-deleting-destructor thunk
        // (Collection_ScalarDeletingDtor @0x8280C510) links against it.
        ~Collection();
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
    void        AssertOnClassCheck(int liClass, int liExpectedClass, void* lpCollection);
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
        void*       GetCollection() const;

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
