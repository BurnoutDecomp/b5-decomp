#include "types.hpp"

#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h" // the two buffer decls (promoted out of this TU)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"     // CgsModule::IOBuffer base (read/write lock-state queries)
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h" // CgsModule::IOHelper<T> (inline ctor/dtor)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

// BrnDirector::DirectorIO scene-query IO-buffer accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Three X360-emitted functions across two small IO buffers:
//
//   SceneQueryOutputBuffer::GetSceneQueryInterface() const @ 0x823B25F0 (read-lock, DWARF :510/479)
//   SceneQueryOutputBuffer::GetSceneQueryInterface()       @ 0x82206B00 (write-lock, DWARF :511/480)
//   SceneQueryInputBuffer::GetResultsQueue()               @ 0x823B2698 (write-lock, DWARF :558/527)
//
// Each is the recurring CgsModule::IOBuffer lock-guarded getter: test a lock bit on the status byte
// at this+0, CGS_ASSERT on violation, then return the address of the first published member @+4
// (immediately after the 1-byte IOBuffer/FlagSet8 base). Read overloads assert the READ lock
// (bit 4); write overloads assert the WRITE lock (bit 3) -- mirroring the asm.
//
// This TU is kept decoupled the same way the committed BrnDirectorModuleIO.cpp
// (SceneQueryOutputBuffer::Destruct) is: light LOCAL class decls deriving CgsModule::IOBuffer, with
// only the addressed member's ADDRESS (@+4) used -- so the DWARF member types are FORWARD-DECLARED
// opaque. The RETURN TYPES are the DWARF-authoritative types (NOT void*): SceneQueryInterface* and
// OutSceneQueryResultsQueue<4032>*. Both have committed homes under CgsSceneManager::SceneManagerIO
// (CgsSceneManagerIO_SceneQueryInterface.h / CgsSceneManagerIO_SceneQueryResultsQueue.h); pulling
// those in instead would let the fully-qualified names replace the forward decls and drop them --
// but the address-return bodies only need a forward-declared opaque type, so we keep them local to
// avoid the heavy includes, matching the sibling BrnDirector local-decl TUs.

namespace BrnDirector
{
namespace DirectorIO
{
    // (The two buffer declarations that used to live here now sit in the sibling header
    // BrnDirectorModuleIOSceneQuery.h -- promoted verbatim so DirectorModule's per-frame
    // entry points can name them in their signatures. Nothing else changed.)

    // ---- SceneQueryOutputBuffer::GetSceneQueryInterface (read/write) -------------------------

    // X360 0x823B25F0 (BrnDirectorModuleIO.h:510): read-lock; return &mSceneQueryInterface (this+4).
    // DWARF (BrnDirectorModuleIO.h:468/479): SceneQueryInterface mSceneQueryInterface @+4;
    // GetSceneQueryInterface() const returns const SceneQueryInterface*.
    const SceneQueryInterface* SceneQueryOutputBuffer::GetSceneQueryInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<const SceneQueryInterface*>(reinterpret_cast<const u8*>(this) + 4);
    }

    // X360 0x82206B00 (BrnDirectorModuleIO.h:511): write-lock; return &mSceneQueryInterface (this+4).
    // Non-const (write-side) overload; tests bit 3 (write-lock). DWARF line 480.
    SceneQueryInterface* SceneQueryOutputBuffer::GetSceneQueryInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<SceneQueryInterface*>(reinterpret_cast<u8*>(this) + 4);
    }

    // ---- SceneQueryInputBuffer::GetResultsQueue (write) -------------------------------------

    // X360 0x823B2698 (BrnDirectorModuleIO.h:558): write-lock; return &mResultsQueue (this+4).
    // Non-const (write-side) overload; tests bit 3 (write-lock). DWARF (BrnDirectorModuleIO.h:503/527)
    // types the member OutSceneQueryResultsQueue<4032> mResultsQueue @+4 (first member after the
    // 1-byte IOBuffer base). Address-return only -> the queue interior is not needed here. (The
    // const read-lock GetResultsQueue() overload is NOT one of the 7 funcs in this batch.)
    OutSceneQueryResultsQueue<4032>* SceneQueryInputBuffer::GetResultsQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<OutSceneQueryResultsQueue<4032>*>(reinterpret_cast<u8*>(this) + 4);
    }

    // X360 0x82206BA8 (BrnDirectorModuleIO.h:557 assert; DWARF decl line 526): read-lock;
    // return &mResultsQueue (this+4). const (read-side) complement of the write-lock
    // GetResultsQueue() @0x823B2698 (DWARF line 527). Asm tests bit 4 (read-lock: extrwi r11,r11,1,27)
    // then addi r3,r28,4. Caller DirectorModule::ProcessSceneQueryResults consumes the queue read-only.
    // Rodata carries the trailing newline (aNotLockedForRe) -- kept verbatim.
    const OutSceneQueryResultsQueue<4032>* SceneQueryInputBuffer::GetResultsQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const OutSceneQueryResultsQueue<4032>*>(reinterpret_cast<const u8*>(this) + 4);
    }
}
}

// ---- IOHelper<SceneQueryInputBuffer> constructor (X360 0x823C1A90) -----------------
// The generic CgsModule::IOHelper<BufferType> ctor body is inline in CgsModuleIOHelper.h;
// the X360 emits one out-of-line copy per instantiation. This TU owns the
// SceneQueryInputBuffer instantiation (its GetResultsQueue accessors are bodied above), so it
// also forces the matching helper ctor the X360 emitted here (the DoUpdate_Director borrow):
//   mpStack = lpStack; CGS_ASSERT( mpStack->CreateIOBuffer( &mpBuffer, lpcName ) )
// stw r3,0(r31) stores mpStack@+0; addi r4,r31,4 forms &mpBuffer@+4; the CreateIOBuffer bool
// result gates the single assert. Force ONLY the ctor; the dtor's DestroyIOBuffer pop is a
// different caller's out-of-line symbol.
template
CgsModule::IOHelper<BrnDirector::DirectorIO::SceneQueryInputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);
