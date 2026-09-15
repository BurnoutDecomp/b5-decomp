#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"       // TypeDesc, ITypeHandler
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"    // RefSpec
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h"      // Attrib::Alloc
#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   Attrib::Attrib_RefSpec_TypeHandler::Release @ 0x821F0300
//   Attrib::Attrib_RefSpec_TypeHandler::Retain  @ 0x821F02F8
//   Attrib::DefaultDataArea                      @ 0x821F0048
//   Attrib::TypeDesc::Lookup                     @ 0x821F00E8
//   Attrib::TypeDesc::NameToType                 @ 0x821F0150
//
// Generated AttribSys glue. The RefSpec handler retains/releases ref-spec values;
// DefaultDataArea hands back the shared zero-initialised default block for any type
// up to the largest generated size; TypeDesc::Lookup is the generated implicit
// binary-search over the sorted type-key table; NameToType hashes a name to a key.

namespace Attrib
{
    // ---------------------------------------------------------------------------------------
    // The generated type-handler registry, read off the image (2026-09-15, b5-decomp issue #30's
    // rival-creation crash): TypeDesc::Lookup @0x821F00E8 walks a sorted key table
    // (qword_82CDA330, count dword_82CD53AC) and returns the matching entry of a parallel
    // handler-pointer table (off_82CDA340). The ARTIST build has ONE entry:
    //     key 0x97C5113DE9BD1AD6 == StringToKey64("Attrib::RefSpec")  ->  off_82CDA348,
    // a static Attrib_RefSpec_TypeHandler whose vtable (0x82000CEC) is
    //     ~ @0x821F0308, Retain @0x821F02F8, Clone @0x821F0510, Clean @0x821F0300, Release @0x821F0300
    // (Clean and Release are the same body: `mr r3,r4 ; b Attrib::RefSpec::Clean`).
    //
    // Why it matters: Collection::Clean (run for EVERY collection by
    // CollectionExportPolicy::PrepareToDeinitialize at every vault unload) hands each node to
    // its type's handler->Clean. RefSpec::Clean releases the collection reference the RefSpec
    // cached in RefSpec::GetCollection. With this table EMPTY (the placeholder that lived here),
    // those references were never released, a vault's collections never reached refcount zero at
    // unload, Class::RemoveCollection never ran, and the class tables kept pointers into the
    // freed vault block -- the next reload of the same vehicle's vault resolved the stale entry
    // and VehicleAttribs::SetupAttribs faulted in Attrib::Instance::Instance (heap luck: it only
    // fires when the freed block has been reused).
    // ---------------------------------------------------------------------------------------
    class Attrib_RefSpec_TypeHandler : public ITypeHandler
    {
    public:
        virtual ~Attrib_RefSpec_TypeHandler() {}                                   // @0x821F0308
        virtual void* Retain(void* lpObj) { return lpObj; }                        // @0x821F02F8
        virtual void* Clone(void* lpObj)                                           // @0x821F0510
        {
            // Attrib::Alloc(24) (the accounting + linear-allocator assert are inlined there),
            // then the RefSpec copy constructor in place.
            void* lpBlock = Attrib::Alloc(sizeof(RefSpec));
            return lpBlock != NULL ? new (lpBlock) RefSpec(*static_cast<const RefSpec*>(lpObj)) : NULL;
        }
        virtual void  Clean(void* lpObj)   { static_cast<RefSpec*>(lpObj)->Clean(); }   // @0x821F0300
        virtual void  Release(void* lpObj) { static_cast<RefSpec*>(lpObj)->Clean(); }   // @0x821F0300
    };

    static Attrib_RefSpec_TypeHandler gRefSpecTypeHandler;                          // off_82CDA348
    static const u64            gaTypeKeys[1]     = { 0x97C5113DE9BD1AD6ull };      // qword_82CDA330
    static ITypeHandler* const  gaTypeHandlers[1] = { &gRefSpecTypeHandler };       // off_82CDA340
    static const u32            KU_TYPE_COUNT     = 1u;                             // dword_82CD53AC

    // The shared default-data area; the largest generated type is 0x1D48 bytes.
    static u8 gaDefaultData[0x1D48] = {};
    static const u32 KU_MAX_DEFAULT_DATA_SIZE = 0x1D48;

    u64  StringToKey(const char* pcName);

    void* DefaultDataArea(u32 luSize)
    {
        CGS_ASSERT(luSize <= KU_MAX_DEFAULT_DATA_SIZE, "DefaultData requested for type which is too large.");
        return gaDefaultData;
    }

    // TypeDesc::Lookup @0x821F00E8 -- implicit binary search over the sorted key table
    // (child of node i is 2i+1 / 2i+2 by the compare), the handler table parallel to it.
    ITypeHandler* TypeDesc::Lookup(u64 luType)
    {
        if (KU_TYPE_COUNT == 0u)
            return NULL;
        u32 luIndex = 0;
        for (;;)
        {
            const u64 luKey = gaTypeKeys[luIndex];
            if (luKey == luType)
                break;
            luIndex = 2u * luIndex + (luKey < luType ? 1u : 0u) + 1u;
            if (luIndex >= KU_TYPE_COUNT)
                return NULL;
        }
        return (luIndex < KU_TYPE_COUNT) ? gaTypeHandlers[luIndex] : NULL;
    }

    // TypeDesc::NameToType @0x821F0150 -- the full 64-bit key (StringToKey64); the earlier
    // u32 narrowing here was a leftover of a forked StringToKey declaration.
    u64 TypeDesc::NameToType(const char* pcName)
    {
        return StringToKey(pcName);
    }
}
