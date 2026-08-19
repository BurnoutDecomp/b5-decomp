#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapCullingModuleIO.h"

// =============================================================================
// CgsSceneManager::OverlapCullingIO -- the overlap-culling (narrow-phase) module's
// two IO buffers, reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not
// byte match). This is the console's own CgsOverlapCullingModuleIO.cpp: the DecFIGS
// DWARF for that file declares exactly these four bodies at
//   :46  InputBuffer::Construct     :72  InputBuffer::Destruct
//   :98  OutputBuffer::Construct    :119 OutputBuffer::Destruct
//
// X360 anchors:
//   InputBuffer::Construct                     @ 0x828CB640  (out of line)
//   DestroyIOBuffer<InputBuffer>               @ 0x828C4F70  (Destruct inlined, size 0x41028)
//   CreateIOBuffer<InputBuffer>                @ 0x828CC5C0
//   CreateIOBuffer<OutputBuffer>               @ 0x828CC698  (Construct inlined, size 0x100020)
//   DestroyIOBuffer<OutputBuffer>              @ 0x828C5148  (Destruct inlined)
//   EventQueue<OverlappingPair,16384>::Construct @ 0x828C4D40 (explicit instantiation below)
// =============================================================================

namespace CgsSceneManager
{
namespace OverlapCullingIO
{

// -----------------------------------------------------------------------------
// InputBuffer::Construct -- X360 0x828CB640, store for store:
//
//   li   r11, 1 ; stb r11, 0(r31)                    -> CgsModule::IOBuffer::Construct()
//   addi r3, r31, 4        ; bl OverlappingPair,16384>::Construct   (+4)
//   addis r3,r31,4 ; addi 0x10   -> +0x40010 = 262160
//                          ; bl AddInternalCollisionVolume,256>::Construct
//   addis r3,r31,4 ; addi 0xC1C  -> +0x40C1C = 265244
//                          ; bl RemoveInternalCollisionVolume,256>::Construct
//   li   r11, 0
//   stw  r11, 0xC(r31)                 -> +12     = mOverlappingPairQueue.miLength
//   stwx r11, r31, 0x40018 (=262168)   -> +262168 = mAddInternalVolumeQueue.miLength
//   stwx r11, r31, 0x40C24 (=265252)   -> +265252 = mRemoveInternalVolumeQueue.miLength
//
// Each of the three trailing stores is `queue base + 8`, i.e. BaseEventQueue<T>::miLength --
// the queues' own Clear(). (The DWARF hint listing for :46 shows the same shape: three
// EventQueue Constructs interleaved with SetEventPointer/Clear, which is what
// EventQueue<T,N>::Construct + Clear() inline to.)
// -----------------------------------------------------------------------------
void InputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();          // *(this) = 1

    mOverlappingPairQueue.Construct();         // console +4
    mAddInternalVolumeQueue.Construct();       // console +262160
    mRemoveInternalVolumeQueue.Construct();    // console +265244

    mOverlappingPairQueue.Clear();             // console +12
    mAddInternalVolumeQueue.Clear();           // console +262168
    mRemoveInternalVolumeQueue.Clear();        // console +265252
}

// -----------------------------------------------------------------------------
// InputBuffer::Destruct -- the console folds it into
// DestroyIOBuffer<InputBuffer> @0x828C4F70, whose tail is:
//
//   lwz  r3, 0(r27)                         ; r3 = the buffer
//   stw  r29(=0), 0xC(r3)                   -> mOverlappingPairQueue.Clear()
//   stwx r29,     r3, 0x40C24 (=265252)     -> mRemoveInternalVolumeQueue.Clear()
//   stwx r29,     r3, 0x40018 (=262168)     -> mAddInternalVolumeQueue.Clear()
//   bl   CgsModule::IOBuffer::Destruct
//   bl   IOBufferStack::Free(stack, buffer, 0x41028)      <- sizeof(InputBuffer) == 266280
//
// The three queues have no storage of their own to release (their events are inline
// arrays), so the console's BaseEventQueue<T>::Destruct is exactly the miLength store.
// -----------------------------------------------------------------------------
void InputBuffer::Destruct()
{
    mOverlappingPairQueue.Clear();
    mRemoveInternalVolumeQueue.Clear();
    mAddInternalVolumeQueue.Clear();

    CgsModule::IOBuffer::Destruct();
}

// -----------------------------------------------------------------------------
// OutputBuffer::Construct -- the console folds it into
// CreateIOBuffer<OutputBuffer> @0x828CC698:
//
//   bl   IOBufferStack::Alloc(this, 0x100020, name)   <- sizeof(OutputBuffer) == 1048608
//   li   r11, 1 ; stb r11, 0(r31)                     -> CgsModule::IOBuffer::Construct()
//   addi r3, r31, 0x10 ; bl Contact,16384>::Construct -> mContactQueue at +16
//   stw  r28(=0), 0x18(r31)                           -> +24 = mContactQueue.miLength (Clear)
//
// (The +16 seat, rather than the +12 the other queues get, is Contact's 16-byte alignment
// padding the 12-byte BaseEventQueue header out -- Contact carries vec4 lanes.)
// -----------------------------------------------------------------------------
void OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();   // *(this) = 1

    mContactQueue.Construct();          // console +16
    mContactQueue.Clear();              // console +24 (miLength)
}

// -----------------------------------------------------------------------------
// OutputBuffer::Destruct -- folded into DestroyIOBuffer<OutputBuffer> @0x828C5148:
//
//   lwz  r3, 0(r27)
//   stw  r28(=0), 0x18(r3)            -> mContactQueue.Clear()
//   bl   CgsModule::IOBuffer::Destruct
//   bl   IOBufferStack::Free(stack, buffer, 0x100020)
// -----------------------------------------------------------------------------
void OutputBuffer::Destruct()
{
    mContactQueue.Clear();

    CgsModule::IOBuffer::Destruct();
}

}
}

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlappingPair, 16384>::Construct
//   @ 0x828C4D40   (an IDA/identity EXPORT HOLE -- recovered by headless IDA on a private
//    .i64 copy in wave Q5; the address was named but had no per-address JSON.)
//
// The thin explicit instantiation of the generic EventQueue<T,N>::Construct for the
// culler input buffer's 16384-deep overlapping-pair queue. Store for store:
//   addi  r30, r31, 0xC            ; lpEventBuffer = &maEvents (this + 12)
//   cmplwi cr6, r30, 0 ; bne       ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160;
//                                    vacuous -- &maEvents is never null. Hex-Rays renders
//                                    the addi+cmplwi as `result == -12`.)
//   stw   r30, 0(r31)              ; mpEvents    = &maEvents
//   li    r11, 0x4000 ; stw r11, 4(r31)   ; miMaxLength = 16384
//   li    r10, 0      ; stw r10, 8(r31)   ; miLength    = 0
//
// ⭐ maEvents at this+0xC (NOT 0x10) is the console proof that CgsSceneManager::OverlappingPair
// is 4-ALIGNED there: the 12-byte {T*,s32,s32} header takes NO tail padding. It is corroborated
// by the queue's seat inside InputBuffer (buffer+4, itself not 8-aligned) and by
// BaseEventQueue<OverlappingPair>::GetEvent @0x828AE0D0 (`16 * liIndex + mpEvents`, stride
// 0x10 == sizeof).
// ✅ UPDATED 2026-08-19 (wave Q5, cluster E3a): this note used to end "on this LLP64 host the
// element's two u64 VolumeInstanceId members make it 8-aligned, so maEvents lands at +0x10 --
// a width difference absorbed by name access". Both halves of that sentence are now obsolete:
// the two-u64 spelling was WRONG (the element is {u32 muVolumeInstanceA, u32 muVolumeInstanceB,
// f32 mfPadding, bool mbCull} -- DWARF CgsSceneManagerTypes.h:87-90, and the producer/consumer
// asm in CgsOverlappingPair.h's banner), and with the real field set the host element is
// 4-aligned exactly like the console, so maEvents lands at +0xC here too. No packing, no
// divergence, and the console's 0x10 stride is the host's `sizeof` by construction.
//
// Callers (X360): OverlapCullingIO::InputBuffer::Construct @0x828CB640 (0x828CB660) and
// 0x828CC848.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlappingPair, 16384>::Construct();
