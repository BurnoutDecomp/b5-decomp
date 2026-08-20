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
//   Attrib::FindCollection                    @ 0x82808378   (landed 2026-07-31)
//
// Attrib::FindCollection @ 0x82808378 used to be deliberately skipped here, because the
// committed declaration `FindCollection(int liKey, void* lpOwner = nullptr)` was
// irreconcilable with the true X360 two-key signature. That reconciliation is DONE: the
// declaration in GameSource/AttribSys/Generated/attrib_findcollection.h now carries the
// asm-verified `(u64 luClassKey, u64 luCollectionKey)` shape, every generated ctor was
// re-pointed at it, and the body lives below. (The consumer count was 9 generated ctors +
// one hand-written caller, not the 57 this comment used to estimate.)

#include <cstdint> // uintptr_t

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"     // RefSpec, Collection, Collection_AddRef/Release, AssertOnClassCheck
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"        // ITypeHandler
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"                  // Database::IsInitialized
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h" // GetDatabasePrivate
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h" // DatabasePrivate::mClasses (by NAME, not +8)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"                 // Class, VecHashMap_Attrib_Class_TablePolicy_0_16
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"                         // Attrib::FindCollection (the canonical decl this TU bodies)
#include "GameSource/AttribSys/Generated/attrib_private.h"                                // Attrib::Private
#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT + CgsDev::Assert::*
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                   // CgsCore::SPrintf

namespace Attrib
{

// ===========================================================================
// Attrib::FindCollection @ 0x82808378
// ===========================================================================
// Resolve a collection by (class key, collection key) against the process attribute
// database. Two chained hash lookups:
//   1. the class registry -- DatabasePrivate::mClasses, which the X360 reaches as
//      `Database::sThis->mPrivates + 8` (the same seam RefSpec::GetClass below uses).
//      ⚠️ +8 IS A CONSOLE OFFSET; on x64 the member sits at +16. Read it BY NAME.
//   2. that class's collection table -- ClassPrivate::mCollections, which the X360
//      reaches as `Class::mpPrivates + 0x1C` and which Class::GetCollection already
//      wraps by name.
// NULL when either lookup misses. GetDatabasePrivate() carries the console's
// "Attribute database not initialized." assert (attribsys.h:649), so the null-sThis
// guard the X360 open-codes here is not repeated.
//
// NOTE the console does NOT AddRef the collection it hands back -- the caller's
// Attrib::Instance ctor takes the reference. Do not add one here.
Collection* FindCollection(u64 luClassKey, u64 luCollectionKey)
{
    // ⚠️ PC BOOT-ORDER GUARD -- a deliberate, narrow divergence from the X360 body, NOT a
    // stub. The console open-codes `if (!sThis) assert("Attribute database not
    // initialized.")` and then dereferences sThis regardless, because on console nothing
    // reaches this resolve before the database exists. On PC something does:
    //
    //     GameSource/Main/BrnMain.cpp:43   `static BrnGame::BrnGameModule gGameModule;`
    //
    // is a file-scope global, so its DYNAMIC INITIALIZER runs before main(). That builds
    // the whole module tree, including the director arbitrator's state container, which
    // default-constructs all ten states BY VALUE -- and ArbStateRaceIntro embeds an
    // Attrib::Gen::shotgroup (BrnArbStateRaceIntro.h:128), whose ctor lands here. At that
    // point Attrib::Database has not been constructed, CgsDev::Assert::Manager has no
    // renderer installed, and the console's assert path takes
    // CgsDev::Assert::Manager::HandleAssert+0x129 (`call qword ptr [rax+8]` with rax == 0)
    // straight into an access violation -- before the log file is even open.
    // (Measured 2026-07-31: exit 0xC0000005 at ~3.5 s, zero log lines, vs 417 lines and
    // still running for the same build with this guard.)
    //
    // Returning "no such collection" is the resolver's own honest answer for a database
    // that holds nothing yet, and it is what every caller already handles (Attrib::Instance
    // treats a null collection as "unresolved handle").
    // DELETE-WHEN: the module tree stops being constructed by a pre-main global.
    if (!Database::IsInitialized())
        return nullptr;

    // ⚠️ FIXED 2026-08-01 (Prepare wave): this used to be
    //     reinterpret_cast<VecHashMap...*>(reinterpret_cast<u8*>(GetDatabasePrivate()) + 8)
    // -- the CONSOLE byte offset of DatabasePrivate::mClasses. On x64 the Database base is
    // {vptr, mPrivates} == 16 bytes, not 8, so that pointed EIGHT BYTES SHORT: the table
    // header was read as {mPrivates-tail, mpTable, muTableSize...} shifted by one pointer,
    // giving a nonsense bucket count and a garbage bucket array. The observable symptom was
    // an unbounded storm of "Invalid node found during search; table invariant is broken"
    // (vechashmap.cpp:113) the moment anything actually resolved a collection -- which
    // nothing did until Prepare landed. Read the member by NAME (the x64-gate rule); the
    // sibling Class::GetCollectionTable already did.
    const Class* lpClass = GetDatabasePrivate()->mClasses.Find(luClassKey);
    if (!lpClass)
        return nullptr;

    return lpClass->GetCollection(luCollectionKey);
}

// @0x82808400 -- identical two-stage resolve to FindCollection, except the
// class collection lookup falls back to that class's default collection.
Collection* FindCollectionWithDefault(u64 luClassKey, u64 luCollectionKey)
{
    if (!Database::IsInitialized())
        return nullptr;

    const Class* lpClass = GetDatabasePrivate()->mClasses.Find(luClassKey);
    if (!lpClass)
        return nullptr;

    return lpClass->GetCollectionWithDefault(luCollectionKey);
}

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

    // Same x64-gate correction as FindCollection above: `privates + 8` is the CONSOLE
    // offset of mClasses and lands 8 bytes short on the host. Read it by name.
    return GetDatabasePrivate()->mClasses.Find(mClassKey);
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
// The SPrintf format string is the MEASURED rodata literal at 0x820D9088 (dumped whole
// via headless IDA, 2026-08-04; an earlier pass only saw IDA's display-truncated
// "Collection %08x%08x of incorrect class "... and invented a ", should be class" tail).
// Six %08x conversions, three hi/lo pairs, matching the six staged words in the asm
// (r6..r10 + one stack word).
static char sacAssertMessage[1024];

void AssertOnClassCheck(int liClass, int liExpectedClass, u64 luCollectionKey)
{
    const u64 luCollection = luCollectionKey;
    const u64 luClass = static_cast<u64>(static_cast<u32>(liClass));
    const u64 luExpected = static_cast<u64>(static_cast<u32>(liExpectedClass));

    CgsCore::SPrintf(sacAssertMessage, 1024,
        "Collection %08x%08x of incorrect class (%08x%08x) being used to create instance of type %08x%08x",
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
