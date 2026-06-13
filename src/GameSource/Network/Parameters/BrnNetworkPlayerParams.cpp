#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::PlayerParamsBase::GetData        @ 0x825841D0
//   BrnNetwork::PlayerParamsBase::GetDataSize    @ 0x825841C8
//   BrnNetwork::PlayerParamsBase::GetPattern     @ 0x825841B8
//   BrnNetwork::PlayerParamsBase::Prepare        @ 0x8258A818
//   BrnNetwork::PlayerParamsBase::PreparePattern @ 0x82584148
//
// PlayerParamsBase wraps a 28-byte network-replicated payload living at byte offset
// 132 of the object. GetData/GetDataSize/GetPattern expose the payload, its size, and
// the (lazily built) serialisation pattern string. PreparePattern builds that pattern
// once into a shared static buffer: "%dsbbblll" formatted with a per-subclass field
// count obtained through the virtual at vtable slot 2. Prepare resets the payload's
// bookkeeping fields after the platform server-params prepare succeeds.

// Platform server-interface params (other TU); declared for the compile-only gate.
namespace CgsNetwork
{
    struct ServerInterfacePlayerParamsX360
    {
        static int Prepare();
    };
}

namespace BrnNetwork
{
    namespace
    {
        // Static serialisation-pattern buffer (X360 byte_82FB7150) and its
        // build-once guard (byte_82FB7164).
        char gaPatternBuffer[13];
        bool gbPatternBuilt = false;
    }

    class PlayerParamsBase
    {
    public:
        void* GetData()      { return reinterpret_cast<u8*>(this) + 132; }
        int   GetDataSize()  { return 28; }
        void* GetPattern()   { return gaPatternBuffer; }

        int  Prepare();
        void* PreparePattern();

        // Declared here; defined in another TU.
        void SetFreeBurnCarID(int liId);
    };

    void* PlayerParamsBase::PreparePattern()
    {
        if (!gbPatternBuilt)
        {
            gbPatternBuilt = true;

            // Virtual call at vtable slot 2 -> per-subclass leading field count.
            typedef int (*PatternCountFn)(void*);
            void** lpVtable = *reinterpret_cast<void***>(this);
            int liCount = reinterpret_cast<PatternCountFn>(lpVtable[2])(this);

            CgsCore::SPrintf(gaPatternBuffer, sizeof(gaPatternBuffer), "%dsbbblll", liCount);
        }
        return this;
    }

    int PlayerParamsBase::Prepare()
    {
        // Platform server-params prepare must succeed first.
        if (!CgsNetwork::ServerInterfacePlayerParamsX360::Prepare())
            return 0;

        PreparePattern();

        u8* lpThis = reinterpret_cast<u8*>(this);
        u8 luFlags = lpThis[146] & 0xF8;   // preserve high 5 bits
        lpThis[145] = 0;
        lpThis[152] = 0;
        lpThis[148] = 0xFF;                // -1
        lpThis[146] = luFlags;
        lpThis[158] = 0;
        lpThis[147] = 0xFF;                // -1
        SetFreeBurnCarID(0);
        return 1;
    }
}
