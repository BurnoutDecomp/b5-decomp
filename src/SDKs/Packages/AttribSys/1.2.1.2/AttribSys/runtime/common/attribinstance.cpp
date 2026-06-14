#include "types.hpp"
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
    // Partial Attrib::Collection layout (the fields the Instance touches).
    struct Collection
    {
        u8     mPad0[8];
        u16    muRefCount;     // +8
        u8     mPad1[6];       // +10
        void*  mpSubCollection;// +16
        u8     mPad2[4];       // +20
        int*   mpClass;        // +24
        void*  mpData;         // +28
        u32    muHasNoDefault; // +32
    };

    // An "attribute" value is the 16-byte record copied out by Get.
    struct AttributeValue { u32 mauWords[4]; };

    // Collection helpers (own TUs); trap stubs until they land.
    Collection* Collection_AddRef(Collection* lpCollection, int liFlags);
    int         Collection_Release(Collection* lpCollection, int liFlags);
    void*       Collection_Get(void* lpOut, int liKey, int* lpName, int liArg);
    void*       Collection_GetData(Collection* lpCollection);
    int         RefSpec_GetCollectionWithDefault(int* lpRefSpec);
    Collection* Collection_AddRef(Collection*, int) { __debugbreak(); return nullptr; }
    int         Collection_Release(Collection*, int) { __debugbreak(); return 0; }
    void*       Collection_Get(void*, int, int*, int) { __debugbreak(); return nullptr; }
    void*       Collection_GetData(Collection*) { __debugbreak(); return nullptr; }
    int         RefSpec_GetCollectionWithDefault(int*) { __debugbreak(); return 0; }

    class Instance
    {
    public:
        Instance(Collection* lpCollection, void* lpOwner);
        ~Instance();
        Collection* Change(Collection* lpNewCollection);
        Collection* ChangeWithDefault(int* lpRefSpec);
        void*       Get(AttributeValue* pOut, int* lpName, int liArg);
        void*       GetAttributePointer();
        int         GetClass() const;
        void*       GetCollection() const;

    private:
        void Unmodify();

        Collection* mpCollection;     // +0
        void*       mpAttributeData;  // +4
        void*       mpOwner;          // +8
        u32         muFlags;          // +12
    };

    void Instance::Unmodify() { __debugbreak(); }

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
