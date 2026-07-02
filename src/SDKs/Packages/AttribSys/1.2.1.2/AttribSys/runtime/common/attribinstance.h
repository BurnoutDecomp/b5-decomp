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

namespace Attrib
{
    typedef u32 Key;   // X360: 32-bit class/attribute key (an earlier SDK rev was 64-bit).

    // Partial Attrib::Collection layout (the fields Instance touches).
    struct Collection
    {
        u8     mPad0[8];
        u16    muRefCount;      // +8
        u8     mPad1[6];        // +10
        void*  mpSubCollection; // +16
        u8     mPad2[4];        // +20
        int*   mpClass;         // +24
        void*  mpData;          // +28
        u32    muHasNoDefault;  // +32
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
