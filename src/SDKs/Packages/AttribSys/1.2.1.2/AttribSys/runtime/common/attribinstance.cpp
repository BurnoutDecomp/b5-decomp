#include "attribinstance.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   Attrib::Instance::Change              @ 0x8280D1A8
//   Attrib::Instance::ChangeWithDefault   @ 0x8280D258
//   Attrib::Instance::Get                 @ 0x828081B0
//   Attrib::Instance::GetAttributePointer @ 0x82805880
//   Attrib::Instance::GetClass            @ 0x82802F18
//   Attrib::Instance::GetCollection       @ 0x82802F40
//   Attrib::Instance::Instance            @ 0x82802DB8   (Collection*, owner)
//   Attrib::Instance::Instance            @ 0x8280A248   (const RefSpec&, owner)
//   Attrib::Instance::~Instance           @ 0x8280D100
//
// AttribSys SDK (no PC binary / open source present -> reconstructed from the
// console build). An Instance is a ref-counted handle onto an Attrib::Collection.
// Construction/destruction maintain the collection refcount; Change swaps the
// referenced collection; the accessors forward into the collection. Collection
// internals are owned by the SDK and accessed by their recovered field offsets.

namespace Attrib
{
    // ------------------------------------------------------------------------
    // Collection refcount helpers. These two are NOT separate functions on the
    // console -- they ARE Attrib::Collection::AddRef @0x828028E0 and
    // Attrib::Collection::Release @0x8280C2E8, both of which are reconstructed by
    // name in attribcollection.cpp. Verified from the one-line caller:
    //
    //   Attrib::Instance::~Instance @0x8280D100
    //     0x8280D100  lwz  r3, 0(r3)        ; mpCollection
    //     0x8280D104  cmplwi cr6, r3, 0
    //     0x8280D108  beqlr cr6
    //     0x8280D10C  li   r4, 0            ; <- the "flags" argument ...
    //     0x8280D110  b    Attrib__Collection__Release
    //
    // ...and Collection::Release's own body (@0x8280C2E8) never reads r4 -- it is
    // `mr r30, r3` / `bl HashMap::Release` / queue-for-delete. The second argument
    // is a dead register stage that Hex-Rays surfaces as a parameter; the callers
    // stage 0 or -1 indifferently. So the faithful body of these free-function
    // seams is a straight forward to the real members, and liFlags is (void)'d.
    //
    // ⚠️ These were `__debugbreak()` traps until 2026-07-31. That was load-bearing
    // by accident: Attrib::FindCollection was a `return 0` stub, so mpCollection was
    // always null and ~Instance never reached the trap. The moment FindCollection
    // started resolving real collections (same date) every Instance destructor would
    // have hit the breakpoint. Do not re-trap these.
    // ------------------------------------------------------------------------
    Collection* Collection_AddRef(Collection* lpCollection, int /*liFlags*/)
    {
        return lpCollection ? lpCollection->AddRef() : nullptr;
    }

    int Collection_Release(Collection* lpCollection, int /*liFlags*/)
    {
        return lpCollection ? lpCollection->Release() : 0;
    }

    // Still un-landed (own TU); trap stub. Types are declared in attribinstance.h
    // (reconstructed header), not forked locally here.
    // (Collection_Get and Collection_GetData were two more of these until 2026-07-31.
    // Collection_Get is now real in attribute.cpp; Collection_GetData is GONE -- its only
    // caller was the phantom no-argument GetAttributePointer, and the real
    // Collection::GetData(key, index) is bodied by name in attribcollection.cpp.)
    int         RefSpec_GetCollectionWithDefault(int*) { __debugbreak(); return 0; }

    // Attrib::Instance (and Collection / AttributeValue) are declared in attribinstance.h.

    // Attrib::Instance::Unmodify @ 0x8280D118
    // Drop a "modified" instance back onto its parent/default collection. When the modified
    // flag (muFlags bit0) is set, swap the referenced collection for its parent (Collection
    // +0x0C), re-cache the parent's attribute-data block (or null when there is no parent),
    // AddRef the new collection, release the old one, and clear the modified flag.
    void Instance::Unmodify()
    {
        if ((muFlags & 1u) != 0)
        {
            Collection* lpOld    = mpCollection;
            Collection* lpParent = mpCollection->mpParent;
            mpCollection = lpParent;
            if (lpParent)
            {
                mpAttributeData = lpParent->mpData;
                Collection_AddRef(mpCollection, 0);
            }
            else
            {
                mpAttributeData = nullptr;
            }
            Collection_Release(lpOld, 0);
            muFlags &= ~1u;
        }
    }

    // Attrib::RefSpec::RefSpec (copy) @ 0x82803560
    // Memberwise-copy the two keys and the resolved collection pointer; if a collection was
    // resolved, AddRef it by bumping its shared refcount (asserting it has not saturated).
    RefSpec::RefSpec(const RefSpec& lrOther)
        : mClassKey(lrOther.mClassKey)
        , mCollectionKey(lrOther.mCollectionKey)
        , mpCollectionPtr(lrOther.mpCollectionPtr)
    {
        if (mpCollectionPtr)
        {
            Collection* lpCollection = const_cast<Collection*>(mpCollectionPtr);
            CGS_ASSERT(lpCollection->muRefCount != 0xFFFF,
                       "Exceeded collection refcount maximum!\n");
            ++lpCollection->muRefCount;
        }
    }

    Instance::Instance(Collection* lpCollection, void* lpOwner)
        : mpCollection(lpCollection), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
        if (lpCollection)
        {
            // X360 `lwz r11,0x20(r30); if (!r11) instance[0xC] = 1` -- a collection
            // with NO source vault carries no default layout to fall back on.
            if (lpCollection->mpSource == nullptr)
                muFlags = 1;
            mpAttributeData = lpCollection->mpData;
            CGS_ASSERT(lpCollection->muRefCount != 0xFFFF, "Exceeded collection refcount maximum!\n");
            ++lpCollection->muRefCount;
        }
    }

    // ------------------------------------------------------------------------
    // Attrib::Instance::Instance(const RefSpec&, void*) @ 0x8280A248
    // ------------------------------------------------------------------------
    // The X360 body, in full:
    //     mr   r31,r3 / mr r3,r4 / mr r29,r5
    //     bl   Attrib__RefSpec__GetCollection      ; r30 = the resolved collection
    //     stw  r29,8(r31)  ; mpOwner
    //     stw  r30,0(r31)  ; mpCollection
    //     stw  r11,4(r31)  ; mpAttributeData = 0
    //     stw  r11,0xC(r31); muFlags         = 0
    //     beq  -> return                          ; null collection: nothing more to do
    //     lwz  r11,0x20(r30) ; if (!collection->mpSource) muFlags = 1
    //     lwz  r11,0x1C(r30) ; mpAttributeData = collection->mpData
    //     lhz  r11,8(r30)    ; bounded ++collection->muRefCount  (attribhashmap.h:622)
    // From 0x8280A264 onward that is INSTRUCTION-FOR-INSTRUCTION the Collection* ctor
    // @0x82802DCC, so this delegates rather than restating it -- the only thing this
    // overload adds is the RefSpec resolve in front.
    //
    // ⚠️ The resolve DOUBLE-COUNTS on purpose, and that is the console's behaviour, not a
    // leak: RefSpec::GetCollection AddRefs when it caches a freshly-resolved collection in
    // the ref, and then this ctor AddRefs again for the handle. ~Instance drops the handle's
    // reference; the ref keeps its own until RefSpec::Clean(). Do not "fix" one of the two.
    //
    // The two const_casts are spelling artefacts of this tree's own drift, not semantics:
    // DWARF attribsys.h:753 declares `const Attrib::Collection* GetCollection() const` (the
    // cache member is mutable in the real SDK) and attribsys.h:534 types mCollection as
    // `const Attrib::Collection*`. The committed header carries the non-const spellings of
    // both, so the resolve has to be laundered through them here.
    Instance::Instance(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(const_cast<Collection*>(const_cast<RefSpec&>(lrRefSpec).GetCollection()),
                   lpOwner)
    {
    }

    // Copy construct. NOT an X360 out-of-line symbol (the console inlines it everywhere);
    // the PS3 DWARF attests the SDK declares it at attribinstance.cpp:37. The body is
    // DERIVED from the refcount invariant ~Instance @0x8280D100 enforces -- it releases
    // mpCollection unconditionally, so a copy that shared the pointer without taking a
    // reference would give one AddRef and two Releases. Shape and the bounded-AddRef
    // spelling follow the attested sibling RefSpec::RefSpec(const RefSpec&) @0x82803560
    // just above. mpAttributeData is copied rather than re-derived: it is already the
    // source's `collection->mpData` (or its DefaultDataArea override, which re-deriving
    // would throw away).
    Instance::Instance(const Instance& lrOther)
        : mpCollection(lrOther.mpCollection)
        , mpAttributeData(lrOther.mpAttributeData)
        , mpOwner(lrOther.mpOwner)
        , muFlags(lrOther.muFlags)
    {
        if (mpCollection)
        {
            CGS_ASSERT(mpCollection->muRefCount != 0xFFFF,
                       "Exceeded collection refcount maximum!\n");
            ++mpCollection->muRefCount;
        }
    }

    // Attrib::Instance::operator= @ 0x8280DE08
    //     mr r30,r4 / mr r31,r3
    //     lwz r4,0(r30)          ; rhs.mpCollection
    //     bl  Attrib__Instance__Change
    //     lwz r11,8(r30) / stw r11,8(r31)     ; mpOwner = rhs.mpOwner
    //     lwz r11,0xC(r30) / stw r11,0xC(r31) ; muFlags = rhs.muFlags
    //     mr  r3,r31             ; return *this
    // Change() does the whole refcount transition (release the old collection, take a
    // reference on the new one, re-cache mpAttributeData), which is why nothing here
    // touches +4. Note the flag word is copied AFTER Change, so it OVERWRITES the bit0
    // Change just recomputed from the new collection's source vault -- reproduce that;
    // for a right-hand side built over the same collection the two agree anyway.
    Instance& Instance::operator=(const Instance& lrOther)
    {
        Change(lrOther.mpCollection);
        mpOwner = lrOther.mpOwner;
        muFlags = lrOther.muFlags;
        return *this;
    }

    Instance::~Instance()
    {
        if (mpCollection)
            Collection_Release(mpCollection, 0);
    }

    Collection* Instance::Change(Collection* lpNewCollection)
    {
        Collection* lResult = mpCollection;
        if (lpNewCollection != mpCollection)
        {
            Unmodify();
            if (mpCollection)
                Collection_Release(mpCollection, 0);

            bool lbHasDefault = false;
            mpCollection = lpNewCollection;
            if (lpNewCollection)
            {
                lResult = reinterpret_cast<Collection*>(Collection_AddRef(lpNewCollection, 0));
                mpAttributeData = mpCollection->mpData;
                lbHasDefault = (mpCollection->mpSource == nullptr);
            }

            if (!lbHasDefault)
                muFlags &= ~1u;
            else
                muFlags |= 1u;
        }
        return lResult;
    }

    Collection* Instance::ChangeWithDefault(int* lpRefSpec)
    {
        int liCollection = RefSpec_GetCollectionWithDefault(lpRefSpec);
        return Change(reinterpret_cast<Collection*>(liCollection));
    }

    // @0x828081B0 -- resolve luAttributeKey against the instance whose address arrives in
    // lpName and copy the resulting 16-byte Attrib::Attribute cursor into pOut (the X360
    // returns the cursor by value through the hidden sret pointer in r3, which is why the
    // out parameter leads and `this` rides in r4). An instance with no collection yields a
    // zeroed cursor.
    void* Instance::Get(AttributeValue* pOut, int* lpName, u64 luAttributeKey)
    {
        Instance* lpInstance = reinterpret_cast<Instance*>(lpName);
        AttributeValue lScratch;
        AttributeValue* lpValue;
        if (lpInstance->mpCollection)
        {
            lpValue = reinterpret_cast<AttributeValue*>(
                Collection_Get(&lScratch, lpInstance->mpCollection, lpInstance, luAttributeKey));
        }
        else
        {
            std::memset(&lScratch, 0, sizeof(lScratch));
            lpValue = &lScratch;
        }
        *pOut = *lpValue;
        return pOut;
    }

    // @0x82805880 -- `lwz r3,0(r3); if (!r3) return 0; b Attrib__Collection__GetData`.
    // The branch is a TAIL CALL, so r4 (the 64-bit attribute key) and r5 (the element
    // index) reach Collection::GetData @0x82804FD0 untouched. This is the ONE
    // GetAttributePointer symbol in the image; the no-argument spelling that used to sit
    // beside it was Hex-Rays' view of this same body and is retired.
    void* Instance::GetAttributePointer(u64 luAttributeKey, u32 luIndex) const
    {
        if (!mpCollection)
            return nullptr;
        return mpCollection->GetData(luAttributeKey, luIndex);
    }

    int Instance::GetClass() const
    {
        if (!mpCollection)
            return 0;
        // X360 reads the owning class's leading key word (Class +0). Kept as the raw
        // leading-word read the recovered body performs; Attrib::Class is only
        // forward-declared in this TU.
        const int* lpClass = reinterpret_cast<const int*>(mpCollection->mpClass);
        return lpClass ? *lpClass : 0;
    }

    u64 Instance::GetCollection() const
    {
        return mpCollection ? mpCollection->mKey : 0;
    }
}
