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
    // ---- SceneQueryOutputBuffer --------------------------------------------------------------

    // X360 0x82239578 (the CreateIOBuffer<SceneQueryOutputBuffer> instantiation @0x823AF4C0 calls it).
    //   stb 1, 0(this)                         -- IOBuffer::Construct (the status byte)
    //   six EventQueue<T,N>::Construct         -- +0x30 / +0x2C0 / +0xCD0 / +0xF60 / +0x1150 / +0x1A20
    //                                             (0x8222DA08 / DA78 / DAE8 / DB58 / DBC8 / DC38)
    //   six `stw 0` to each queue's length     -- +0x38 / +0x2C8 / +0xCD8 / +0xF68 / +0x1158 / +0x1A28
    //                                             (the inlined Clear)
    //   the interface's nine slots             -- the six queues in the producer's slot order
    //                                             (fine, nearest, fast-DS, sphere, deepest, volume-fine)
    //                                             then three NULL triangle-collision slots
    //                                             (+0x1C / +0x20 / +0x24; the director never asks one)
    void SceneQueryOutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mFineLineTestQueue.Construct();
        mFineLineTestNearestQueue.Construct();
        mFineLineTestFastDoubleSidedQueue.Construct();
        mSphereTestFastQueue.Construct();
        mFineVolumeTestDeepestQueue.Construct();
        mFineVolumeTestQueue.Construct();

        mFineLineTestQueue.Clear();
        mFineLineTestNearestQueue.Clear();
        mFineLineTestFastDoubleSidedQueue.Clear();
        mSphereTestFastQueue.Clear();
        mFineVolumeTestDeepestQueue.Clear();
        mFineVolumeTestQueue.Clear();

        mSceneQueryInterface.mpFineLineTestQueue                      = &mFineLineTestQueue;
        mSceneQueryInterface.mpFineLineTestNearestQueue               = &mFineLineTestNearestQueue;
        mSceneQueryInterface.mpFineLineTestFastDoubleSidedQueue       = &mFineLineTestFastDoubleSidedQueue;
        mSceneQueryInterface.mpFineSphereTestFastQueue                = &mSphereTestFastQueue;
        mSceneQueryInterface.mpFineVolumeTestDeepestQueue             = &mFineVolumeTestDeepestQueue;
        mSceneQueryInterface.mpFineVolumeTestQueue                    = &mFineVolumeTestQueue;
        mSceneQueryInterface.mpTriangleCollisionLineTestQueue         = 0;
        mSceneQueryInterface.mpTriangleCollisionLineTestNearestQueue  = 0;
        mSceneQueryInterface.mpTriangleCollisionSphereTestQueue       = 0;
    }

    // X360 0x8221B3F8 (DestroyIOBuffer<SceneQueryOutputBuffer> @0x823AF590 calls it): four `stw 0`
    // -- the lengths of the fine (+0x38), nearest (+0x2C8), deepest (+0x1158) and volume-fine
    // (+0x1A28) queues -- then a tail jump to IOBuffer::Destruct. The fast-DS and sphere queues'
    // lengths are NOT cleared (the console's own four stores; reproduced).
    // (Retired: the local fork of this body in BrnDirectorModuleIO.cpp, which was never mounted.)
    void SceneQueryOutputBuffer::Destruct()
    {
        mFineLineTestQueue.Clear();
        mFineLineTestNearestQueue.Clear();
        mFineVolumeTestDeepestQueue.Clear();
        mFineVolumeTestQueue.Clear();
        CgsModule::IOBuffer::Destruct();
    }

    // X360 0x823B25F0 (BrnDirectorModuleIO.h:510): read-lock; return &mSceneQueryInterface.
    const CgsSceneManager::SceneManagerIO::SceneQueryInterface* SceneQueryOutputBuffer::GetSceneQueryInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneQueryInterface;
    }

    // X360 0x82206B00 (BrnDirectorModuleIO.h:511): write-lock; return &mSceneQueryInterface.
    CgsSceneManager::SceneManagerIO::SceneQueryInterface* SceneQueryOutputBuffer::GetSceneQueryInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneQueryInterface;
    }

    // ---- SceneQueryInputBuffer ---------------------------------------------------------------

    // X360 0x8221B310 (CreateIOBuffer<SceneQueryInputBuffer> @0x823AF318 calls it):
    //   stb 1, 0(this); mResultsQueue.Construct(); if (!mResultsQueue.Prepare()) assert (:192).
    void SceneQueryInputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mResultsQueue.Construct();
        const bool lbPrepared = mResultsQueue.Prepare();
        CGS_ASSERT(lbPrepared, "mResultsQueue.Prepare()");                        // BrnDirectorModuleIO.cpp:192
        (void)lbPrepared;
    }

    // X360 0x8221B380 (DestroyIOBuffer<SceneQueryInputBuffer> @0x823AF3E8 calls it):
    //   if (!mResultsQueue.Release()) assert (:207); mResultsQueue.Destruct(); IOBuffer::Destruct().
    void SceneQueryInputBuffer::Destruct()
    {
        const bool lbReleased = mResultsQueue.Release();
        CGS_ASSERT(lbReleased, "mResultsQueue.Release()");                        // BrnDirectorModuleIO.cpp:207
        (void)lbReleased;
        mResultsQueue.Destruct();
        CgsModule::IOBuffer::Destruct();
    }

    // X360 0x823B2698 (BrnDirectorModuleIO.h:558): write-lock; return &mResultsQueue.
    SceneQueryInputBuffer::ResultsQueue* SceneQueryInputBuffer::GetResultsQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResultsQueue;
    }

    // X360 0x82206BA8 (BrnDirectorModuleIO.h:557): read-lock; return &mResultsQueue. The rodata
    // carries the trailing newline (aNotLockedForRe) -- kept verbatim.
    const SceneQueryInputBuffer::ResultsQueue* SceneQueryInputBuffer::GetResultsQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mResultsQueue;
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
