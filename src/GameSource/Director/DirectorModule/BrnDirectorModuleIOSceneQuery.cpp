#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer base (read/write lock-state queries)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

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
    // DWARF member types; forward-declared -- only their address (@+4) is used here.
    struct SceneQueryInterface;                            // CgsSceneManager::SceneManagerIO type (committed home)
    template <int N> struct OutSceneQueryResultsQueue;     // committed home; N == 4032

    struct SceneQueryOutputBuffer : public CgsModule::IOBuffer
    {
        // DWARF: mSceneQueryInterface (type SceneQueryInterface) @+4.
        const SceneQueryInterface* GetSceneQueryInterface() const;  // fn6, this+4, read-lock
        SceneQueryInterface*       GetSceneQueryInterface();        // fn7, this+4, write-lock
    };

    struct SceneQueryInputBuffer : public CgsModule::IOBuffer
    {
        // DWARF: mResultsQueue (type OutSceneQueryResultsQueue<4032>) @+4.
        OutSceneQueryResultsQueue<4032>* GetResultsQueue();         // fn5, this+4, write-lock
    };

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
}
}
