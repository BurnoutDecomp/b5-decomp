#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8221B3F8
//   (BrnDirector::DirectorIO::SceneQueryOutputBuffer::Destruct)
//
// Behaviour-faithful to the X360 pseudocode:
//     result[14]   = 0;   // 0x0038
//     result[178]  = 0;   // 0x02C8
//     result[1110] = 0;   // 0x1158
//     result[1674] = 0;   // 0x1A28
//     return CgsModule::IOBuffer::Destruct(result);
//
// The query-output buffer is a large, mostly-opaque object; this destructor clears
// four owned slot pointers/handles and then chains to the CgsModule::IOBuffer base
// destructor, forwarding its result. Because only these four slots are touched and
// the surrounding layout is unknown, they are addressed by their original 32-bit
// word offsets via a u32 view of `this` rather than modelled as named members.

namespace CgsModule
{
    // Base destructor, reconstructed in its own TU; declaration only (compile gate
    // is `cl /c`, so the definition is resolved at link time).
    struct IOBuffer
    {
        u32* Destruct();
    };
}

namespace BrnDirector
{
    namespace DirectorIO
    {
        struct SceneQueryOutputBuffer : CgsModule::IOBuffer
        {
            u32* Destruct();
        };

        u32* SceneQueryOutputBuffer::Destruct()
        {
            u32* lpuSlots = reinterpret_cast<u32*>(this);
            lpuSlots[14]   = 0;   // 0x0038
            lpuSlots[178]  = 0;   // 0x02C8
            lpuSlots[1110] = 0;   // 0x1158
            lpuSlots[1674] = 0;   // 0x1A28
            return CgsModule::IOBuffer::Destruct();
        }
    }
}
