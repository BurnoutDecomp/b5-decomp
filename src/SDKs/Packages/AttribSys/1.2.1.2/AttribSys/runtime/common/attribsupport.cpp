// SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribsupport.cpp
//
// AttribSys runtime -- the "support" TU. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (AttribSys v1.2.1.2):
//   Attrib::RefSpec::operator=                @ 0x8280DFB0
//   Attrib::RefSpec::Clean                    @ 0x8280DB60
//   Attrib::RefSpec::GetClass                 @ 0x828084B0
//   Attrib::RefSpec::GetCollection            @ 0x82808530
//   Attrib::RefSpec::GetCollectionWithDefault @ 0x828085C0
//   Attrib::Private::GetLength                @ 0x82803558
//   Attrib::AssertOnClassCheck                @ 0x828034D0
//   Attrib::ITypeHandler::~ITypeHandler       @ 0x828035F0
//
// Attrib::FindCollection @ 0x82808378 is deliberately NOT reconstructed here: the committed
// canonical declaration Attrib::FindCollection(int liKey, void* lpOwner = nullptr) --
// which the generated Attrib::Gen::* ctors bind against (several passing a void* owner as
// the second argument, e.g. shotgroup / cameradefaults) -- is irreconcilable with the true
// X360 signature FindCollection(Attribute::Key classKey, Attribute::Key collectionKey)
// (DWARF attribsupport.cpp:35; the 0x82808378 body uses the second argument as a collection
// key: Class::Ta(mCollections @ mpPrivates+0x1C, a2)). Implementing the true two-key body
// under the committed void* signature would misuse the owner pointer as a key (wrong for the
// single-argument callers); correcting the signature breaks the committed consumers' compile
// gate. That reconciliation spans attrib_findcollection.h + 57 generated ctors and is out of
// scope for this TU, so this function is left for a coordinated decl+consumer pass.

#include <cstdint> // uintptr_t

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"     // RefSpec, Collection, Collection_AddRef/Release, AssertOnClassCheck
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"        // ITypeHandler
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h" // GetDatabasePrivate
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"                 // Class, VecHashMap_Attrib_Class_TablePolicy_0_16
#include "GameSource/AttribSys/Generated/attrib_private.h"                                // Attrib::Private
#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT + CgsDev::Assert::*
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                   // CgsCore::SPrintf

namespace Attrib
{

// ===========================================================================
// Attrib::RefSpec::operator= @ 0x8280DFB0
// ===========================================================================
// Release the destination's currently-resolved collection, copy the source's {class key,
// collection key, resolved-collection pointer}, then AddRef the newly-shared collection --
// the X360 folds Collection::AddRef into a bounded ++refcount (with the overflow assert).
RefSpec& RefSpec::operator=(const RefSpec& lrOther)
{
    if (mpCollectionPtr)
    {
        Collection_Release(const_cast<Collection*>(mpCollectionPtr), -1);
        mpCollectionPtr = nullptr;
    }

    mClassKey = lrOther.mClassKey;
    mCollectionKey = lrOther.mCollectionKey;
    mpCollectionPtr = lrOther.mpCollectionPtr;

    if (mpCollectionPtr)
    {
        Collection* lpCollection = const_cast<Collection*>(mpCollectionPtr);
        CGS_ASSERT(lpCollection->muRefCount != 0xFFFF, "Exceeded collection refcount maximum!\n");
        ++lpCollection->muRefCount;
    }
    return *this;
}

// ===========================================================================
// Attrib::RefSpec::Clean @ 0x8280DB60
// ===========================================================================
// Release the resolved collection (if any) and forget it.
void RefSpec::Clean()
{
    if (mpCollectionPtr)
    {
        Collection_Release(const_cast<Collection*>(mpCollectionPtr), -1);
        mpCollectionPtr = nullptr;
    }
}

// ===========================================================================
// Attrib::RefSpec::GetClass @ 0x828084B0
// ===========================================================================
// If a collection is already resolved, return the class it belongs to (collection+0x18).
// Otherwise -- when a class key is set -- resolve it against the process attribute
// database's class-registry table (DatabasePrivate+8 == VecHashMap<Key,Class,...,16u>);
// null when no class key is set.
const Class* RefSpec::GetClass() const
{
    if (mpCollectionPtr)
        return reinterpret_cast<const Class*>(mpCollectionPtr->mpClass);

    if (mClassKey == 0)
        return nullptr;

    u8* lpPrivates = reinterpret_cast<u8*>(GetDatabasePrivate());
    VecHashMap_Attrib_Class_TablePolicy_0_16* lpClassTable =
        reinterpret_cast<VecHashMap_Attrib_Class_TablePolicy_0_16*>(lpPrivates + 8);
    return lpClassTable->Find(mClassKey);
}

// ===========================================================================
// Attrib::RefSpec::GetCollection @ 0x82808530
// ===========================================================================
// Lazily resolve (and AddRef + cache) the referenced collection: look the class up, then
// find the collection stored under mCollectionKey in that class's collection table. Null
// when no collection key is set or the class/collection is absent.
const Collection* RefSpec::GetCollection()
{
    if (mCollectionKey == 0)
        return nullptr;

    if (!mpCollectionPtr)
    {
        const Class* lpClass = GetClass();
        if (!lpClass)
            return nullptr;

        Collection* lpCollection = lpClass->GetCollection(mCollectionKey);
        mpCollectionPtr = lpCollection;
        if (lpCollection)
            Collection_AddRef(lpCollection, -1);
    }
    return mpCollectionPtr;
}

// ===========================================================================
// Attrib::RefSpec::GetCollectionWithDefault @ 0x828085C0
// ===========================================================================
// As GetCollection, but resolves the collection *with the class's default-collection
// fallback* (Attrib::Class::GetCollectionWithDefault) so a missing key still yields the
// class default. AddRefs + caches the resolved collection. (Unlike GetCollection, the X360
// does not early-out on a zero collection key here -- it relies on GetClass returning null.)
const Collection* RefSpec::GetCollectionWithDefault()
{
    if (!mpCollectionPtr)
    {
        const Class* lpClass = GetClass();
        if (!lpClass)
            return nullptr;

        Collection* lpCollection = lpClass->GetCollectionWithDefault(mCollectionKey);
        mpCollectionPtr = lpCollection;
        if (lpCollection)
            Collection_AddRef(lpCollection, -1);
    }
    return mpCollectionPtr;
}

// ===========================================================================
// Attrib::Private::GetLength @ 0x82803558
// ===========================================================================
// The live element count of the variable-length array attribute this 8-byte header
// fronts (X360 `lhz r3, 2(r3)`). Attrib::Private IS the Attrib::Array header
// (attribarray.h) seen through the generated accessors' opaque 8-byte spelling, so
// the count is read BY NAME through that type rather than by raw byte offset.
unsigned int Private::GetLength() const
{
    return reinterpret_cast<const Array*>(mData)->muNumElements;
}

// ===========================================================================
// Attrib::AssertOnClassCheck @ 0x828034D0
// ===========================================================================
// Format a diagnostic naming the collection and the mismatched class keys, then fire the
// AttribSys class-check assert. The X360 formats each of the three values as a %08x%08x
// hi/lo pair (they ride in 64-bit registers) in {collection, class, expected} order, and
// -- per the asm (cmpld/bne at 0x82803528) -- fires the assert when liClass equals
// liExpectedClass. The message buffer is the X360 file-scope scratch (byte_8300FEC8).
//
// NOTE: the SPrintf format string is truncated in the recovered rodata to
// "Collection %08x%08x of incorrect class " -- the asm proves six %08x conversions across
// the three values, so the trailing ", should be class %08x%08x" is reconstructed to that
// specifier count. The message text is diagnostic-only and does not affect behavior.
static char sacAssertMessage[1024];

void AssertOnClassCheck(int liClass, int liExpectedClass, u64 luCollectionKey)
{
    const u64 luCollection = luCollectionKey;
    const u64 luClass = static_cast<u64>(static_cast<u32>(liClass));
    const u64 luExpected = static_cast<u64>(static_cast<u32>(liExpectedClass));

    CgsCore::SPrintf(sacAssertMessage, 1024,
        "Collection %08x%08x of incorrect class %08x%08x, should be class %08x%08x",
        static_cast<u32>(luCollection >> 32), static_cast<u32>(luCollection),
        static_cast<u32>(luClass >> 32), static_cast<u32>(luClass),
        static_cast<u32>(luExpected >> 32), static_cast<u32>(luExpected));

    if (liClass == liExpectedClass)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(sacAssertMessage,
            "..\\..\\..\\SDKs\\Packages\\AttribSys\\1.2.1.2\\AttribSys/runtime/common/attribsupport.cpp",
            114);
        CgsDev::Assert::EndAssert();
    }
}

// ===========================================================================
// Attrib::ITypeHandler::~ITypeHandler @ 0x828035F0
// ===========================================================================
// The base virtual destructor: the X360 body only re-installs the ITypeHandler vtable
// pointer into the object, which the compiler emits implicitly for a virtual destructor;
// the reconstructed body is therefore empty.
ITypeHandler::~ITypeHandler()
{
}

} // namespace Attrib
