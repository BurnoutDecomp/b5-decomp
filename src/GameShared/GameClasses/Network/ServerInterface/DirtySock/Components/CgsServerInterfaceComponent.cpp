#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceComponent::ClearLastError       @ 0x828766F0
//   CgsNetwork::ServerInterfaceComponent::Construct            @ 0x82876418
//   CgsNetwork::ServerInterfaceComponent::ConvertError         @ 0x82876728
//   CgsNetwork::ServerInterfaceComponent::GetAndClearLastError @ 0x82876708
//
// The component holds, at word offsets: [1] a pointer to its static error-string
// data, [2] a status word (2 == "no error"), [3] the last error code. Construct
// points [1] at the static error data (unk_820046A7) and resets status/error;
// Clear/GetAndClear reset them ([1]=0, [2]=2, [3]=0).
//
// ConvertError maps a server result fourcc to an integer error value using a caller-
// supplied table of 8-byte { u32 code; s32 value } entries. A zero code maps to 0.
// If the code is absent from the supplied table (or no table is given) it falls back
// to the one-entry default table keyed on the 'nfnd' ("not found") fourcc, returning
// 1 when even that misses.

namespace CgsNetwork
{
    namespace
    {
        struct ServerErrorEntry
        {
            u32 muCode;
            s32 miValue;
        };

        // 'nfnd' packed as the X360 build reads it (big-endian byte order).
        const u32 KU_NOT_FOUND_CODE =
            (static_cast<u32>('n') << 24) | (static_cast<u32>('f') << 16) |
            (static_cast<u32>('n') << 8)  |  static_cast<u32>('d');

        const ServerErrorEntry KA_DEFAULT_TABLE[1] = { { KU_NOT_FOUND_CODE, 0 } };
    }

    // Static error-string data, defined in another (not-yet-reconstructed) TU.
    extern const u8 gServerInterfaceErrorData[];

    class ServerInterfaceComponent
    {
    public:
        void* ClearLastError();
        void* Construct();
        int   ConvertError(u32 luCode, const ServerErrorEntry* pTable, int liCount);
        int   GetAndClearLastError();
    };

    void* ServerInterfaceComponent::ClearLastError()
    {
        u32* lpThis = reinterpret_cast<u32*>(this);
        lpThis[3] = 0;
        lpThis[2] = 2;
        lpThis[1] = 0;
        return lpThis;
    }

    void* ServerInterfaceComponent::Construct()
    {
        u32* lpThis = reinterpret_cast<u32*>(this);
        lpThis[2] = 2;
        lpThis[1] = static_cast<u32>(reinterpret_cast<uintptr_t>(&gServerInterfaceErrorData));
        lpThis[3] = 0;
        return lpThis;
    }

    int ServerInterfaceComponent::GetAndClearLastError()
    {
        u32* lpThis = reinterpret_cast<u32*>(this);
        int liError = static_cast<int>(lpThis[3]);
        lpThis[3] = 0;
        lpThis[2] = 2;
        lpThis[1] = 0;
        return liError;
    }

    int ServerInterfaceComponent::ConvertError(u32 luCode, const ServerErrorEntry* pTable, int liCount)
    {
        int liResult = 1;
        if (!luCode)
            return 0;

        for (;;)
        {
            const ServerErrorEntry* lpTable = pTable;
            int liEntries = liCount;
            if (!pTable || !liCount)
            {
                liEntries = 1;
                lpTable = KA_DEFAULT_TABLE;
            }

            int liIdx = 0;
            if (liEntries > 0)
            {
                while (lpTable[liIdx].muCode != luCode)
                {
                    if (++liIdx >= liEntries)
                        break;
                }
                if (liIdx < liEntries)
                    liResult = lpTable[liIdx].miValue;
            }

            if (liIdx != liEntries || lpTable == KA_DEFAULT_TABLE)
                return liResult;

            // Not found in the supplied table: retry against the default table.
            pTable = KA_DEFAULT_TABLE;
            liCount = 1;
            liResult = 1;
        }
    }
}
