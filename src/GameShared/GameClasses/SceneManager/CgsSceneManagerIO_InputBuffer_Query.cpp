// =============================================================================
// GameShared/GameClasses/SceneManager/CgsSceneManagerIO_InputBuffer_Query.cpp
//
// SceneManagerIO::InputBuffer_Query -- the per-module scene-query input buffer -- and the
// per-pass SceneManagerIO::TriCacheQueryBuffer: the bring-up and the lock-checked getters.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match), scene-query
// wave 1 (2026-09-02). The layout itself is documented on the types in CgsSceneManagerIO.h.
//
//   InputBuffer_Query::Construct                          @ 0x828C7BC0   (56 insns)
//   InputBuffer_Query::GetCoarseQueryQueue() const        @ 0x828AF270   (:509)
//   InputBuffer_Query::GetFineLineTestQueue() const       @ 0x828AF318   (:511)
//   InputBuffer_Query::GetFineLineTestNearestQueue() const@ 0x828AF3C0   (:512)
//   InputBuffer_Query::GetFineLineTestFastDoubleSidedQueue() const @ 0x828AF468 (:513, IDA "_")
//   InputBuffer_Query::GetFineSphereTestFastQueue() const @ 0x828AF510   (:514)
//   InputBuffer_Query::GetFineVolumeTestDeepestQueue() const @ 0x828AF5B8 (:515)
//   InputBuffer_Query::GetFineVolumeTestQueue() const     @ 0x828AF660   (:516)
//   InputBuffer_Query::GetTriangleCollisionLineTestQueue() const        @ 0x828AF708 (:517)
//   InputBuffer_Query::GetTriangleCollisionLineTestNearestQueue() const @ 0x828AF7B0 (:518)
//   InputBuffer_Query::GetTriangleCollisionSphereTestQueue() const      @ 0x828AF858 (:519)
//   TriCacheQueryBuffer::GetTriangleCollisionLineTestQueue()            @ 0x828AFBA0 (:678, IDA "__cd")
//   TriCacheQueryBuffer::GetTriangleCollisionLineTestNearestQueue()     @ 0x828AFC48 (:679)
//   TriCacheQueryBuffer::GetTriangleCollisionSphereTestQueue()          @ 0x828AFCF0 (:680)
//
// Every getter is the same shape: `lbz r11,0(this) ; extrwi r11,r11,1,27` (status bit 4, the
// READ lock) -- or `1,28` (bit 3, the WRITE lock) for the TriCacheQueryBuffer trio -- then, when
// clear, the StrStream'd "Not locked for reading\n" / "Not locked for writing\n" +
// FireAssert(CgsSceneManagerModuleIO.h, <line>), then `return this + <seat>`. The assert is a
// NON-gating tripwire (the console returns the seat after firing); CGS_ASSERT mirrors that.
//
// The four InputBuffer_Query getters the console emits with a `:510`-style line but NO
// out-of-line body (GetCoarseLineTestQueue() const, the three TriCacheQueryBuffer const
// getters) follow the identical shape -- they are the same source macro, and the read-lock
// bit is the only contract a const getter on an IOBuffer has.
// =============================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // ---------------------------------------------------------------------------------
    // InputBuffer_Query::Construct @ 0x828C7BC0
    //
    //   0x828C7BD8  stb  1, 0(this)                       IOBuffer::Construct (status = constructed)
    //   0x828C7BDC  bl   VariableEventQueue<16384,16>::Construct(this+0x28)      mCoarseQueryQueue
    //   0x828C7BE4  bl   EQ<InEventLineTest,256>::Construct(this+0x4040)         mCoarseLineTestQueue
    //   0x828C7BF0  bl   EQ<InEventLineTestFine,256>::Construct(this+0x7050)     mFineLineTestQueue
    //   0x828C7C00  bl   EQ<InEventLineTestNearest,256>::Construct(this+0xB060)  mFineLineTestNearestQueue
    //   0x828C7C10  bl   EQ<InEventLineTestFastDoubleSided,16>::Construct(+0xF070)
    //   0x828C7C20  bl   EQ<InEventSphereTestFast,16>::Construct(+0xF480)
    //   0x828C7C30  bl   EQ<InEventVolumeTestDeepest,256>::Construct(+0xF790)
    //   0x828C7C40  bl   EQ<InEventVolumeTestFine,64>::Construct(+0x1D7A0)
    //   0x828C7C50  bl   EQ<InEventTriangleCollisionLineTest,256>::Construct(+0x20FB0)
    //   0x828C7C60  bl   EQ<InEventTriangleCollisionLineTestNearest,256>::Construct(+0x23FC0)
    //   0x828C7C70  bl   EQ<InEventTriangleCollisionSphereTest,256>::Construct(+0x26FD0)
    //   0x828C7C74..0x828C7C94  stw <seat>, 4..0x24(this)  -- the nine SceneQueryInterface slots,
    //                            in member order (the fine six, then the tri-collision three).
    // ---------------------------------------------------------------------------------
    void InputBuffer_Query::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mCoarseQueryQueue.Construct();
        mCoarseLineTestQueue.Construct();
        mFineLineTestQueue.Construct();
        mFineLineTestNearestQueue.Construct();
        mFineLineTestFastDoubleSidedQueue.Construct();
        mFineSphereTestFastQueue.Construct();
        mFineVolumeTestDeepestQueue.Construct();
        mFineVolumeTestQueue.Construct();
        mTriangleCollisionLineTestQueue.Construct();
        mTriangleCollisionLineTestNearestQueue.Construct();
        mTriangleCollisionSphereTestQueue.Construct();

        mSceneQueryInterface.mpFineLineTestQueue                     = &mFineLineTestQueue;                      // stw r30, 4(this)
        mSceneQueryInterface.mpFineLineTestNearestQueue              = &mFineLineTestNearestQueue;               // stw r29, 8(this)
        mSceneQueryInterface.mpFineLineTestFastDoubleSidedQueue      = &mFineLineTestFastDoubleSidedQueue;       // stw r28, 0xC(this)
        mSceneQueryInterface.mpFineSphereTestFastQueue               = &mFineSphereTestFastQueue;                // stw r27, 0x10(this)
        mSceneQueryInterface.mpFineVolumeTestDeepestQueue            = &mFineVolumeTestDeepestQueue;             // stw r26, 0x14(this)
        mSceneQueryInterface.mpFineVolumeTestQueue                   = &mFineVolumeTestQueue;                    // stw r25, 0x18(this)
        mSceneQueryInterface.mpTriangleCollisionLineTestQueue        = &mTriangleCollisionLineTestQueue;         // stw r24, 0x1C(this)
        mSceneQueryInterface.mpTriangleCollisionLineTestNearestQueue = &mTriangleCollisionLineTestNearestQueue;  // stw r23, 0x20(this)
        mSceneQueryInterface.mpTriangleCollisionSphereTestQueue      = &mTriangleCollisionSphereTestQueue;       // stw r22, 0x24(this)
    }

    // ---------------------------------------------------------------------------------
    // READ-locked getters (status bit 4; CgsSceneManagerModuleIO.h:509..:519)
    // ---------------------------------------------------------------------------------
    const InputBuffer_Query::InSmCoarseQueryQueue* InputBuffer_Query::GetCoarseQueryQueue() const   // @0x828AF270 :509
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mCoarseQueryQueue;
    }

    const InputBuffer_Query::InCoarseLineTestQueue* InputBuffer_Query::GetCoarseLineTestQueue() const   // :510
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mCoarseLineTestQueue;
    }

    const InputBuffer_Query::InFineLineTestQueue* InputBuffer_Query::GetFineLineTestQueue() const   // @0x828AF318 :511
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mFineLineTestQueue;
    }

    const InputBuffer_Query::InFineLineTestNearestQueue* InputBuffer_Query::GetFineLineTestNearestQueue() const   // @0x828AF3C0 :512
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mFineLineTestNearestQueue;
    }

    const InputBuffer_Query::InFineLineTestFastDoubleSidedQueue* InputBuffer_Query::GetFineLineTestFastDoubleSidedQueue() const   // @0x828AF468 :513
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mFineLineTestFastDoubleSidedQueue;
    }

    const InputBuffer_Query::InFineSphereTestFastQueue* InputBuffer_Query::GetFineSphereTestFastQueue() const   // @0x828AF510 :514
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mFineSphereTestFastQueue;
    }

    const InputBuffer_Query::InFineVolumeTestDeepestQueue* InputBuffer_Query::GetFineVolumeTestDeepestQueue() const   // @0x828AF5B8 :515
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mFineVolumeTestDeepestQueue;
    }

    const InputBuffer_Query::InFineVolumeTestQueue* InputBuffer_Query::GetFineVolumeTestQueue() const   // @0x828AF660 :516
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mFineVolumeTestQueue;
    }

    const InputBuffer_Query::InTriangleCollisionLineTestQueue* InputBuffer_Query::GetTriangleCollisionLineTestQueue() const   // @0x828AF708 :517
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriangleCollisionLineTestQueue;
    }

    const InputBuffer_Query::InTriangleCollisionLineTestNearestQueue* InputBuffer_Query::GetTriangleCollisionLineTestNearestQueue() const   // @0x828AF7B0 :518
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriangleCollisionLineTestNearestQueue;
    }

    const InputBuffer_Query::InTriangleCollisionSphereTestQueue* InputBuffer_Query::GetTriangleCollisionSphereTestQueue() const   // @0x828AF858 :519
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriangleCollisionSphereTestQueue;
    }

    // ---------------------------------------------------------------------------------
    // TriCacheQueryBuffer getters (CgsSceneManagerModuleIO.h:675..:680)
    // ---------------------------------------------------------------------------------
    const InputBuffer_Query::InTriangleCollisionLineTestQueue* TriCacheQueryBuffer::GetTriangleCollisionLineTestQueue() const   // :675
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriangleCollisionLineTestQueue;
    }

    const InputBuffer_Query::InTriangleCollisionLineTestNearestQueue* TriCacheQueryBuffer::GetTriangleCollisionLineTestNearestQueue() const   // :676
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriangleCollisionLineTestNearestQueue;
    }

    const InputBuffer_Query::InTriangleCollisionSphereTestQueue* TriCacheQueryBuffer::GetTriangleCollisionSphereTestQueue() const   // :677
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTriangleCollisionSphereTestQueue;
    }

    InputBuffer_Query::InTriangleCollisionLineTestQueue* TriCacheQueryBuffer::GetTriangleCollisionLineTestQueue()   // @0x828AFBA0 :678
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTriangleCollisionLineTestQueue;
    }

    InputBuffer_Query::InTriangleCollisionLineTestNearestQueue* TriCacheQueryBuffer::GetTriangleCollisionLineTestNearestQueue()   // @0x828AFC48 :679
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTriangleCollisionLineTestNearestQueue;
    }

    InputBuffer_Query::InTriangleCollisionSphereTestQueue* TriCacheQueryBuffer::GetTriangleCollisionSphereTestQueue()   // @0x828AFCF0 :680
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTriangleCollisionSphereTestQueue;
    }
}
}
