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
void InputBuffer::Destruct()
{
    mAddBodyQueue.Clear();
    mUpdateBodyQueue.Clear();
    mRemoveBodyQueue.Clear();
    mForceNoPaddingQueue.Clear();

    CgsModule::IOBuffer::Destruct();
}

// ---------------------------------------------------------------------------------
// InputBuffer::AddBody   (CgsOverlapGenerationModuleIO.h:200)
//   Queue "start colliding this volume instance". No out-of-line symbol: the X360
//   inlines it into its only caller, SceneManagerModule::AddBody @0x828BA498, whose
//   arguments are this producer's arguments one-for-one
//   (r5 = luIndex, r6 = lpAabb, r7 = lCullGroup, r8 = leBodyState, r9 = lVolumeInstanceID):
//       ld/std x4 off r6      -> the 32-byte world box into mAabb   (+0x00..+0x1F)
//       stw r7                -> mCullGroup                          (+0x20)
//       stw r5                -> muIndex                             (+0x24)
//       stw r8                -> meBodyState                         (+0x28)
//       std r9                -> mVolumeInstanceID                   (+0x30)
//       bl sub_828B0188 (== the non-const GetAddBodyQueue, this+16, write-lock assert)
//       bl BaseEventQueue<InAddBody>::AddEvent
//   The caller's trailing culling-group bookkeeping (the assert + the
//   mCullingGroupManager byte store) is NOT part of this producer -- it stays in
//   SceneManagerModule::AddBody.
// ---------------------------------------------------------------------------------
void InputBuffer::AddBody(u32 luIndex, const rw::collision::AABBox* lpAabb,
                          InAddBody::CullingGroup lCullGroup, rw::physics::BodyState leBodyState,
                          VolumeInstanceId lVolumeInstanceID)
{
    InAddBody lEvent;

    // The four ld/std qword pairs: the caller's box copied whole. rw::collision::AABBox is
    // only forward-declared here (the rw::math::vpu::Vector3 fork -- see the header banner),
    // so the copy goes through its pinned two-corner-row image; same reader idiom as the
    // three committed precedents (CgsSceneSweeper_wQ5_01.cpp:137, CgsVolumeManager.cpp,
    // PropManager_wQ4_03.cpp).
    lEvent.mAabb             = *reinterpret_cast<const AABBoxRows*>(lpAabb);
    lEvent.mCullGroup        = lCullGroup;
    lEvent.muIndex           = luIndex;
    lEvent.meBodyState       = leBodyState;
    lEvent.mVolumeInstanceID = lVolumeInstanceID;

    GetAddBodyQueue()->AddEvent(lEvent);
}

// ---------------------------------------------------------------------------------
// InputBuffer::RemoveBody   (CgsOverlapGenerationModuleIO.h:205)
//   Queue "stop colliding this volume instance". No out-of-line symbol: the X360
//   inlines it into SceneManagerModule::ProcessRemoveForCollisionEvent @0x828CF828
//   (and into RemoveAllEntityVolumeInstances), which stacks
//       v10 = *(u64*)lpEvent   -> mVolumeInstanceID  (+0x00)
//       v11 = luIndex          -> muIndex            (+0x08)
//   then `sub_828B02D8(inputBuffer)` (== the non-const GetRemoveBodyQueue, this+2097200,
//   write-lock assert) and BaseEventQueue<InRemoveBody>::AddEvent.
// ---------------------------------------------------------------------------------
void InputBuffer::RemoveBody(u32 luIndex, VolumeInstanceId lVolumeInstanceID)
{
    InRemoveBody lEvent;

    lEvent.mVolumeInstanceID = lVolumeInstanceID;
    lEvent.muIndex           = luIndex;

    GetRemoveBodyQueue()->AddEvent(lEvent);
}

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
    // forward-declared only; see the header banner's Vector3-fork note).
    lEvent.mAabb             = *reinterpret_cast<const AABBoxRows*>(lpAabb);
    lEvent.mPadding          = lvPadding;
    lEvent.muIndex           = luIndex;
    lEvent.mVolumeInstanceID = lVolumeInstanceID;

    GetUpdateBodyQueue()->AddEvent(lEvent);
}

// ---------------------------------------------------------------------------------
// InputBuffer::ForceNoPadding   (CgsOverlapGenerationModuleIO.h:216)
//   Queue "drop this object's sweep padding for one frame". No out-of-line symbol: the
//   X360 inlines it into SceneManagerModule::ProcessForceNoPaddingEvent @0x828CFC80,
//   which stacks the single word (`v9 = VolumeInstanceIndexByID`), calls
//   `sub_828B0380(inputBuffer)` (== the non-const GetForceNoPaddingQueue, this+2359360,
//   write-lock assert) and appends through
//   BaseEventQueue<InForceNoPadding>::AddEvent @0x828B89D0 (a single 32-bit move).
// ---------------------------------------------------------------------------------
void InputBuffer::ForceNoPadding(u32 luVolInstIndex)
{
    InForceNoPadding lEvent;

    lEvent.muVolInstIndex = luVolInstIndex;

    GetForceNoPaddingQueue()->AddEvent(lEvent);
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
const InAddBodyQueue* InputBuffer::GetAddBodyQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mAddBodyQueue;
}

const InUpdateBodyQueue* InputBuffer::GetUpdateBodyQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mUpdateBodyQueue;
}

const InRemoveBodyQueue* InputBuffer::GetRemoveBodyQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mRemoveBodyQueue;
}

const InForceNoPaddingQueue* InputBuffer::GetForceNoPaddingQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mForceNoPaddingQueue;
}

InAddBodyQueue* InputBuffer::GetAddBodyQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mAddBodyQueue;
}

InUpdateBodyQueue* InputBuffer::GetUpdateBodyQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mUpdateBodyQueue;
}

InRemoveBodyQueue* InputBuffer::GetRemoveBodyQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mRemoveBodyQueue;
}

InForceNoPaddingQueue* InputBuffer::GetForceNoPaddingQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mForceNoPaddingQueue;
}

} // namespace OverlapGenerationIO
} // namespace CgsSceneManager
