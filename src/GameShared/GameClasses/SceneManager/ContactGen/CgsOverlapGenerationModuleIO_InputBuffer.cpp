#include "GameShared/GameClasses/Development/CgsStrStream.h"
// ===========================================================================
// CgsSceneManager::OverlapGenerationIO::InputBuffer -- lifetime, producers, accessors.
//
// The SceneManagerModule pushes per-frame body-mutation requests onto this input
// buffer's four event queues while it drains InSceneUpdateInterface; the
// OverlapGenerationModule's Process*Queue passes (CgsOverlapGenerationModule.cpp) then
// replay them into the SceneSweeper.
//
// The class lives in its own home header, CgsOverlapGenerationModuleIO.h -- the file the
// X360 build names in every accessor tripwire it bakes. Only ONE of the bodies below is
// emitted out of line by the console (UpdateBody @0x828BA430); the rest are header-shaped
// members the X360 inlined into their single caller, and each one's reconstruction cites
// the call site it was lifted from.
//
// Split: this TU owns the INPUT half; the sibling CgsOverlapGenerationModuleIO_OutputBuffer.cpp
// owns the OUTPUT half (same split the CgsGuiModuleIO_{Input,Output}Buffer.cpp pair uses).
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT (the lock tripwires)

namespace CgsSceneManager
{
namespace OverlapGenerationIO
{

// ---------------------------------------------------------------------------------
// InputBuffer::Construct @ 0x828CB918   (CgsOverlapGenerationModuleIO.h:185,
//                                        CgsOverlapGenerationModuleIO.cpp:46)
//   Mark the buffer constructed, then Construct each of the four queues (which points
//   each one at its own inline event array and sets its capacity) and clear each length.
//   The X360 body is:
//       stb r11(=1), 0(r31)                       -- IOBuffer::Construct, inlined: the
//                                                    status byte becomes eStatusConstructed
//       bl InAddBody_16384_::Construct(this+0x10)
//       bl InUpdateBody_16384_::Construct(this+0x100020)
//       bl InRemoveBody_16384_::Construct(this+0x200030)
//       bl InForceNoPadding_128_::Construct(this+0x240040)
//       stw 0, 0x18(this) / 0x100028 / 0x200038 / 0x240048
//   The four trailing stores are each queue's miLength word (queueBase+8) -- i.e. the
//   BaseEventQueue::Clear the DWARF's own call list names. They are redundant after
//   Construct (which already zeroes miLength) and are reproduced because the console
//   makes them; the DWARF cpp hint lists both calls per queue for the same reason.
// ---------------------------------------------------------------------------------
void InputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();

    mAddBodyQueue.Construct();
    mUpdateBodyQueue.Construct();
    mRemoveBodyQueue.Construct();
    mForceNoPaddingQueue.Construct();

    mAddBodyQueue.Clear();
    mUpdateBodyQueue.Clear();
    mRemoveBodyQueue.Clear();
    mForceNoPaddingQueue.Clear();
}

// ---------------------------------------------------------------------------------
// InputBuffer::Destruct   (CgsOverlapGenerationModuleIO.h:189,
//                          CgsOverlapGenerationModuleIO.cpp:74)
//   No out-of-line symbol: the X360 inlines it whole into
//   CgsModule::IOBufferStack::DestroyIOBuffer<InputBuffer> @0x828C53F0, which is
//       *(u32*)(this + 0x18)      = 0     -- mAddBodyQueue.miLength
//       *(u32*)(this + 0x240048)  = 0     -- mForceNoPaddingQueue.miLength
//       *(u32*)(this + 0x200038)  = 0     -- mRemoveBodyQueue.miLength
//       *(u32*)(this + 0x100028)  = 0     -- mUpdateBodyQueue.miLength
//       bl CgsModule::IOBuffer::Destruct
//   i.e. every queue is Clear()ed, then the base is destructed. The four clears are
//   independent stores whose emitted order is compiler scheduling, not source order
//   (the DWARF call list shows the same four with the per-queue Destructs, which are
//   empty for a fixed-capacity queue and leave no code), so they are written here in
//   member order. Nothing is freed: the queues own inline storage.
// ---------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------
// InputBuffer::UpdateBody @ 0x828BA430   (CgsOverlapGenerationModuleIO.h:212)
//   Push a per-frame body-position update request. THE ONE PRODUCER THE X360 EMITS OUT
//   OF LINE. It assembles the 64-byte InUpdateBody record on the stack --
//       ld/std x4 off r5   -> the 32-byte world AABB value-copied into mAabb (+0x00)
//       stvx128 v1         -> mPadding                                        (+0x20)
//       stw r4             -> muIndex                                         (+0x30)
//       std r6             -> mVolumeInstanceID                               (+0x38)
//   -- then `bl sub_828B0230` (== the non-const GetUpdateBodyQueue, this+1048608, whose
//   OWN body carries the "Not locked for writing" assert; UpdateBody itself never tests
//   the status byte) and appends it via that queue's AddEvent.
//   ⚠️ The third argument is the sweeper PADDING lane, not a position: the DWARF names it
//   `Vector3 mPadding` and SceneSweeper::UpdateObject takes it as the padding vector.
// ---------------------------------------------------------------------------------
void InputBuffer::UpdateBody(u32 luIndex, const rw::collision::AABBox* lpAabb,
                             Vector3 lvPadding, VolumeInstanceId lVolumeInstanceID)
{
    InUpdateBody lEvent;

    // Same pinned two-corner-row image copy as AddBody above (rw::collision::AABBox is
    // forward-declared only; see the header banner's Vector3-fork note). Assigned in
    // ARTIST's store order (0x828BA458..0x828BA474): box, index, padding, volume id.
    lEvent.mAabb             = *reinterpret_cast<const AABBoxRows*>(lpAabb);
    lEvent.muIndex           = luIndex;
    lEvent.mPadding          = lvPadding;
    lEvent.mVolumeInstanceID = lVolumeInstanceID;

    GetUpdateBodyQueue()->AddEvent(lEvent);
}

// ---------------------------------------------------------------------------------
// The eight queue accessors. Each is a one-line `return &member` guarded by the matching
// IOBuffer lock tripwire; the X360 emits a separate out-of-line copy of each, and each
// copy bakes CgsOverlapGenerationModuleIO.h + its own line number into the assert, which
// is what pins the declaration line quoted beside every one of them:
//     0x828AFEE8 :222 read  this+16         0x828B0188 :227 write this+16
//     0x828AFF90 :223 read  this+1048608    0x828B0230 :228 write this+1048608
//     0x828B0038 :224 read  this+2097200    0x828B02D8 :229 write this+2097200
//     0x828B00E0 :225 read  this+2359360    0x828B0380 :230 write this+2359360
// (0x828B00E0 was an export hole; recovered by targeted headless IDA for this cluster --
// scratchpad/waveQ5/ida_ovgenio/ovgenio_dump.json -- and it reads exactly like its three
// siblings: `(*this >> 4) & 1` read-lock test, assert line 225, `addis 0x24; addi 0x40`.)
// The assert messages are the console's own rodata ("Not locked for reading\n" /
// "Not locked for writing\n", trailing newline included -- they are streamed into the
// assert message buffer).
// ---------------------------------------------------------------------------------
static const char KAC_OVERLAP_IO_ASSERT_FILE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\contactgen\\CgsOverlapGenerationModuleIO.h";

const InAddBodyQueue* InputBuffer::GetAddBodyQueue() const
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(), "Not locked for reading\n", KAC_OVERLAP_IO_ASSERT_FILE, 222);
    return &mAddBodyQueue;
}

const InUpdateBodyQueue* InputBuffer::GetUpdateBodyQueue() const
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(), "Not locked for reading\n", KAC_OVERLAP_IO_ASSERT_FILE, 223);
    return &mUpdateBodyQueue;
}

const InRemoveBodyQueue* InputBuffer::GetRemoveBodyQueue() const
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(), "Not locked for reading\n", KAC_OVERLAP_IO_ASSERT_FILE, 224);
    return &mRemoveBodyQueue;
}

const InForceNoPaddingQueue* InputBuffer::GetForceNoPaddingQueue() const
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(), "Not locked for reading\n", KAC_OVERLAP_IO_ASSERT_FILE, 225);
    return &mForceNoPaddingQueue;
}

InAddBodyQueue* InputBuffer::GetAddBodyQueue()
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", KAC_OVERLAP_IO_ASSERT_FILE, 227);
    return &mAddBodyQueue;
}

InUpdateBodyQueue* InputBuffer::GetUpdateBodyQueue()
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", KAC_OVERLAP_IO_ASSERT_FILE, 228);
    return &mUpdateBodyQueue;
}

InRemoveBodyQueue* InputBuffer::GetRemoveBodyQueue()
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", KAC_OVERLAP_IO_ASSERT_FILE, 229);
    return &mRemoveBodyQueue;
}

InForceNoPaddingQueue* InputBuffer::GetForceNoPaddingQueue()
{
    CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(), "Not locked for writing\n", KAC_OVERLAP_IO_ASSERT_FILE, 230);
    return &mForceNoPaddingQueue;
}

} // namespace OverlapGenerationIO
} // namespace CgsSceneManager
