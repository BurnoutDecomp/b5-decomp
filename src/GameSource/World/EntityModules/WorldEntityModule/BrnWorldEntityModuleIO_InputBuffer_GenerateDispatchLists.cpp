#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists members, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Four scalar accessors over two 32-bit fields:
//   muDispatchFrame @ +4        muShadowMap @ +0x8018
//
// GetDispatchFrame @ 0x822BAA08 -- read-lock tripwire ("Not locked for reading",
//   BrnWorldEntityModuleIO.h:278), then `lwz r3,4(this)` returns muDispatchFrame.
// SetDispatchFrame @ 0x827A2FC8 -- write-lock tripwire ("Not locked for writing", line 279),
//   then `stw r27,4(this)` stores muDispatchFrame.
// GetShadowMap     @ 0x822BAAB0 -- read-lock tripwire (line 281), then `lwzx r3,this,0x8018`
//   returns muShadowMap.
// SetShadowMap     @ 0x827A3070 -- write-lock tripwire (line 282), then `stwx r27,this,0x8018`
//   stores muShadowMap.
//
// The read/write-lock bits are the IOBuffer status flags (`extrwi r11,r11,1,27` == bit 4 ==
// read; `extrwi r11,r11,1,28` == bit 3 == write). The streamed-message asserts map to the
// house CGS_ASSERT (the trailing "\n" is dropped from the stringized condition).

namespace BrnWorld
{
namespace WorldEntityIO
{
    void InputBuffer_GenerateDispatchLists::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_GenerateDispatchLists, muDispatchFrame) == 4,
                      "muDispatchFrame @4");
        static_assert(offsetof(InputBuffer_GenerateDispatchLists, muShadowMap) == 0x8018,
                      "muShadowMap @0x8018");
    }

    // X360 0x822BAA08: read-lock; return muDispatchFrame (this+4).
    u32 InputBuffer_GenerateDispatchLists::GetDispatchFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return muDispatchFrame;
    }

    // X360 0x827A2FC8: write-lock; set muDispatchFrame (this+4).
    void InputBuffer_GenerateDispatchLists::SetDispatchFrame(u32 luDispatchFrame)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        muDispatchFrame = luDispatchFrame;
    }

    // X360 0x822BAAB0: read-lock; return muShadowMap (this+0x8018).
    u32 InputBuffer_GenerateDispatchLists::GetShadowMap() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return muShadowMap;
    }

    // X360 0x827A3070: write-lock; set muShadowMap (this+0x8018).
    void InputBuffer_GenerateDispatchLists::SetShadowMap(u32 luShadowMap)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        muShadowMap = luShadowMap;
    }
}
}
