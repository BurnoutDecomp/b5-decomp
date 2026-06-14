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
    u32  StringToKey(const char* pcName);
    u32  StringToKey(const char*) { __debugbreak(); return 0; }

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
            return StringToKey(pcName);
        }
    }
}
