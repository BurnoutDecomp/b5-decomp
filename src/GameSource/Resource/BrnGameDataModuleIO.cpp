#include "GameSource/Resource/BrnGameDataModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnResource::GameDataIO::InputBuffer::GetRequestInterface (const + non-const) @ 0x82663F90 /
// 0x823B1788.
//
// Both overloads are thin lock-guarded accessors that hand back the embedded RequestInterface<32768>
// (which lands at this+4, right after the 4-byte CgsModule::IOBuffer status base -- matching the asm's
// `return a1 + 4`):
//
//   const overload  @ 0x82663F90: tests IOBuffer status bit 4 ((*a1 >> 4) & 1 == read-lock); on
//                    failure inlines a StrStream to build "Not locked for reading\n" and fires the
//                    assert at BrnGameDataModuleIO.h:211. The READER takes a (non-exclusive) read lock.
//
//   non-const ovl   @ 0x823B1788: tests IOBuffer status bit 3 ((*a1 >> 3) & 1 == write-lock); on
//                    failure inlines a StrStream to build "Not locked for writing\n" and fires the
//                    assert at BrnGameDataModuleIO.h:218. The WRITER (the module building requests into
//                    the interface) takes the exclusive write lock.
//
// Mirrors the committed BrnGame::DispatchThreadInputBuffer::GetParticleData() precedent
// (b5-decomp/src/GameSource/Game/BrnDispatchThreadInputBuffer.cpp) and CgsMemoryModuleIO: the simple
// CGS_ASSERT(IsBufferLockedFor{Reading,Writing}(), "...") (those queries are the inherited
// CgsModule::IOBuffer read/write-lock bit tests -- bit 4 / bit 3 respectively) then return
// &mRequestInterface.
//
// FLAG (benign parity gap): the X360 inlined a StrStream (BeginAssert / BasePriorityQueue::Clear / the
// stream operator / FireAssert / EndAssert) to compose the assert message; we deliberately use the
// committed CGS_ASSERT macro instead and do NOT reproduce that StrStream machinery. Parity YELLOW for
// the missing assert-stream calls is the expected/committed convention.
namespace BrnResource
{
namespace GameDataIO
{
    // @ 0x82663F90 -- read-locked accessor (asserts at DWARF h:211).
    const RequestInterface<InputBuffer::knRequestInterfaceQueueSize>* InputBuffer::GetRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mRequestInterface;
    }

    // @ 0x823B1788 -- write-locked accessor (asserts at DWARF h:218).
    RequestInterface<InputBuffer::knRequestInterfaceQueueSize>* InputBuffer::GetRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mRequestInterface;
    }
}
}
