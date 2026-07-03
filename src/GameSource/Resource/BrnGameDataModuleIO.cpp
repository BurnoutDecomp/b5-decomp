#include "GameSource/Resource/BrnGameDataModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnResource::GameDataIO::InputBuffer / OutputBuffer -- the lock-guarded IO-buffer accessors:
//   InputBuffer::GetRequestInterface() const/non-const     @ 0x82663F90 / 0x823B1788  -> &mRequestInterface
//   InputBuffer::GetAttribSysRequestInterface() non/const  @ 0x823B1830 / 0x82664038  -> &mAttribSysRequestInterface
//   OutputBuffer::GetAllocatorList() const                 @ 0x823B18D8               -> *(mpAllocatorList)
//   OutputBuffer::SetAllocatorList()                       @ 0x826640E0               ->  mpAllocatorList
//   OutputBuffer::GetLiveUpdateStatus()                    @ 0x823B1638               -> &mbLiveUpdateStatus
//   OutputBuffer::SetLiveUpdateStatus()                    @ 0x82663E30               ->  mbLiveUpdateStatus
// Each tests the inherited CgsModule::IOBuffer read/write-lock bit (bit 4 read / bit 3 write) via
// IsBufferLockedFor{Reading,Writing}() and then reads/writes the named member. See the AttribSys/
// OutputBuffer FLAGs in the header (opaque AttribSys storage; the +4 pointer layout fix).
//
// The GetRequestInterface overloads are thin lock-guarded accessors that hand back the embedded
// RequestInterface<32768>
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

    // @ 0x823B1830 -- write-locked AttribSys accessor (fires assert at DWARF h:226). Returns this+32788
    // (= &mAttribSysRequestInterface). FLAG: DWARF return type is AttribSysRequestInterface<32768>*;
    // opaque byte-storage member -> address handed back as void* (see header note).
    void* InputBuffer::GetAttribSysRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mAttribSysRequestInterface[0];
    }

    // @ 0x82664038 -- read-locked AttribSys accessor (fires assert at DWARF h:233). Returns this+32788
    // (= &mAttribSysRequestInterface, right after the 4-byte IOBuffer base + RequestInterface<32768>
    // (32784 bytes)). FLAG: DWARF return type is const AttribSysRequestInterface<32768>*; the member is
    // opaque byte storage pending the committed templated AttribSys request interface, so its address is
    // handed back as const void* (see header note).
    const void* InputBuffer::GetAttribSysRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mAttribSysRequestInterface[0];
    }

    // @ 0x823B18D8 -- read-locked accessor. Tests IOBuffer status bit 4 (read-lock); on failure fires
    // the assert. Returns mpAllocatorList (this+4; asm `lwz r3,4(r28)` loads the POINTER value stored
    // there, right after the 4-byte CgsModule::IOBuffer status base).
    const AllocatorList* OutputBuffer::GetAllocatorList() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mpAllocatorList;
    }

    // @ 0x826640E0 -- write-locked mutator. Tests IOBuffer status bit 3 (write-lock); on failure fires
    // the assert. Stores the list pointer into mpAllocatorList (this+4; asm `stw r27,4(r28)`).
    void OutputBuffer::SetAllocatorList(const AllocatorList* lpAllocatorList)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mpAllocatorList = lpAllocatorList;
    }

    // @ 0x823B1638 -- read-locked accessor. Tests IOBuffer status bit 4 (read-lock); on failure fires
    // the assert. Returns &mbLiveUpdateStatus (this+8; asm `addi r3,r28,8`; IDA return type
    // unsigned __int8*). X360 symbol GetLive[UpdateStatus]; the PS3 DWARF drifts this slot into
    // FileSystemStatusInterface (h:189) -- we follow the X360 asm/symbol.
    u8* OutputBuffer::GetLiveUpdateStatus()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mbLiveUpdateStatus;
    }

    // @ 0x82663E30 -- write-locked mutator. Tests IOBuffer status bit 3 (write-lock); on failure fires
    // the assert. Copies a single status byte from *lpStatus into mbLiveUpdateStatus (this+8; asm
    // `lbz r11,0(r27); stb r11,8(r28)`). X360 symbol SetLiveUpdateStatus.
    void OutputBuffer::SetLiveUpdateStatus(const u8* lpStatus)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbLiveUpdateStatus = *lpStatus;
    }
}
}
