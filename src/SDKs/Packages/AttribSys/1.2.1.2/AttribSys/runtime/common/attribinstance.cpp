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
//   Attrib::Instance::Instance            @ 0x82802DB8
//   Attrib::Instance::~Instance           @ 0x8280D100
//
// AttribSys SDK (no PC binary / open source present -> reconstructed from the
// console build). An Instance is a ref-counted handle onto an Attrib::Collection.
// Construction/destruction maintain the collection refcount; Change swaps the
// referenced collection; the accessors forward into the collection. Collection
// internals are owned by the SDK and accessed by their recovered field offsets.

namespace Attrib
{
    // Collection helpers (own TUs); trap stubs until they land. Types are declared
    // in attribinstance.h (reconstructed header), not forked locally here.
    Collection* Collection_AddRef(Collection*, int) { __debugbreak(); return nullptr; }
    int         Collection_Release(Collection*, int) { __debugbreak(); return 0; }
    void*       Collection_Get(void*, int, int*, int) { __debugbreak(); return nullptr; }
    void*       Collection_GetData(Collection*) { __debugbreak(); return nullptr; }
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
            if (!lpCollection->muHasNoDefault)
                muFlags = 1;
            mpAttributeData = lpCollection->mpData;
            CGS_ASSERT(lpCollection->muRefCount != 0xFFFF, "Exceeded collection refcount maximum!\n");
            ++lpCollection->muRefCount;
        }
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
                lbHasDefault = (mpCollection->muHasNoDefault == 0);
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

    void* Instance::Get(AttributeValue* pOut, int* lpName, int liArg)
    {
        AttributeValue lScratch;
        AttributeValue* lpValue;
        if (*lpName)
        {
            lpValue = reinterpret_cast<AttributeValue*>(
                Collection_Get(&lScratch, *lpName, lpName, liArg));
        }
        else
        {
            std::memset(&lScratch, 0, sizeof(lScratch));
            lpValue = &lScratch;
        }
        *pOut = *lpValue;
        return pOut;
    }

    void* Instance::GetAttributePointer()
    {
        return mpCollection ? Collection_GetData(mpCollection) : nullptr;
    }

    int Instance::GetClass() const
    {
        if (!mpCollection)
            return 0;
        int* lpClass = mpCollection->mpClass;
        return lpClass ? *lpClass : 0;
    }

    void* Instance::GetCollection() const
    {
        return mpCollection ? mpCollection->mpSubCollection : nullptr;
    }
}
