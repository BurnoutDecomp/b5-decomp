#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // the header-baked lock asserts
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\entitymodules\\worldentitymodule\\BrnWorldEntityModuleIO.h"
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
        // X360 32-bit byte offsets retired 2026-07-24: typed members (host pointers)
        // -> semantic-by-NAME layout; the X360 offsets live as comments in the header.
    }

    // X360 0x822BAA08: read-lock; return muDispatchFrame (this+4).
    CgsGraphics::DispatchFrame* InputBuffer_GenerateDispatchLists::GetDispatchFrame() const
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(), "Not locked for reading\n", CGS_ARTIST_FILE, 278);
        return mpDispatchFrame;
    }

    // X360 0x827A2FC8: write-lock; set muDispatchFrame (this+4).
    void InputBuffer_GenerateDispatchLists::SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame)
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", CGS_ARTIST_FILE, 279);
        mpDispatchFrame = lpDispatchFrame;
    }

    // X360 0x822BAAB0: read-lock; return muShadowMap (this+0x8018).
    BrnWorld::ShadowMap* InputBuffer_GenerateDispatchLists::GetShadowMap() const
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(), "Not locked for reading\n", CGS_ARTIST_FILE, 281);
        return mpShadowMap;
    }

    // X360 0x827A3070: write-lock; set muShadowMap (this+0x8018).
    void InputBuffer_GenerateDispatchLists::SetShadowMap(BrnWorld::ShadowMap* lpShadowMap)
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", CGS_ARTIST_FILE, 282);
        mpShadowMap = lpShadowMap;
    }
}
}

// DecFIGS BrnWorldEntityModuleIO.cpp:160: the whole body is the base teardown. ARTIST ICF-folds it into
// BrnWorld::PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0 (`b CgsModule::IOBuffer::Destruct`),
// the target DestroyIOBuffer<InputBuffer_GenerateDispatchLists> @0x827BA1C8 calls.
void BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists::Destruct()
{
    CgsModule::IOBuffer::Destruct();
}

#undef CGS_ARTIST_FILE
