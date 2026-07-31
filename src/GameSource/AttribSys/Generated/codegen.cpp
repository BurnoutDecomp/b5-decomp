#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
    // Generated type tables (defined by the AttribSys codegen).
    namespace { struct TypeKey { u32 muKey; u32 muNodeCount; }; }
    extern TypeKey gaTypeKeys[];
    extern void*   gaTypeDescs[];
    extern int     giTypeCount;
    TypeKey gaTypeKeys[1] = {};
    void*   gaTypeDescs[1] = {};
    int     giTypeCount = 0;

    // The shared default-data area; the largest generated type is 0x1D48 bytes.
    static u8 gaDefaultData[0x1D48] = {};
    static const u32 KU_MAX_DEFAULT_DATA_SIZE = 0x1D48;

    int  RefSpec_Clean(int liRefSpec);
    int  RefSpec_Clean(int) { __debugbreak(); return 0; }
    // StringToKey: the real definition lives in the SDK attribhash64.cpp
    // (Bob Jenkins lookup8, seed 0xABCDEF0011223344) -- the placeholder that
    // lived here was removed at the AttribSys mount (LNK2005 otherwise).
    // ⭐ 2026-07-31: this was a LOCAL `u32 StringToKey(const char*)` re-declaration. MSVC
    // does not mangle free-function return types, so it linked against the real 64-bit
    // body and then read only EAX -- a silent truncation at every call site in this TU.
    // Use the canonical declaration instead of re-spelling it.
    u64  StringToKey(const char* pcName);

    class Attrib_RefSpec_TypeHandler
    {
    public:
        int Release(int liRefSpec) { return RefSpec_Clean(liRefSpec); }
        int Retain(int liRefSpec)  { return liRefSpec; }
    };

    void* DefaultDataArea(u32 luSize)
    {
        CGS_ASSERT(luSize <= KU_MAX_DEFAULT_DATA_SIZE, "DefaultData requested for type which is too large.");
        return gaDefaultData;
    }

    namespace TypeDesc
    {
        void* Lookup(u32 luType)
        {
            if (giTypeCount == 0)
                return nullptr;

            u32 luIndex = 0;
            for (;;)
            {
                const TypeKey& lEntry = gaTypeKeys[luIndex];
                if (lEntry.muKey == luType)
                {
                    return (luIndex < lEntry.muNodeCount) ? gaTypeDescs[luIndex] : nullptr;
                }
                luIndex = 2 * luIndex + (lEntry.muKey < luType ? 1u : 0u) + 1;
                if (luIndex >= lEntry.muNodeCount)
                    return nullptr;
            }
        }

        u32 NameToType(const char* pcName)
        {
            // FLAG (2026-07-31): the narrowing is now EXPLICIT rather than hidden in a
            // forked u32 declaration of StringToKey, so today's behaviour is unchanged.
            // It is probably still wrong -- attribdatabase.cpp:29 records that the real
            // TypeDesc::NameToType (codegen @0x821F0150) keys on the full 64-bit hash, and
            // the type-id globals in that TU already use StringToKey64. Left as-is
            // deliberately: this TU's gaTypeKeys[].muKey is u32, so widening it is its own
            // slice, not a side effect of the StringToKey correction.
            return static_cast<u32>(StringToKey(pcName));
        }
    }
}
