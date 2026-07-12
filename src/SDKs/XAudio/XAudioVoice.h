#pragma once

// ===========================================================================
// XAUDIO::CVoice -- the base XAudio voice object in BURNOUT_X360_ARTIST.XEX.
// It is the shared base of the concrete voices (CSourceVoice, CSubmixVoice,
// CMasteringVoice, CRoutedVoice). This header is the canonical OWNING home for
// the XAUDIO::CVoice TU.
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of the CVoice methods. `XAUDIO` is an
// external middleware boundary, so its identifiers are preserved verbatim per
// the naming convention.
//
// LAYOUT (grounded in the asm load/store displacements; X360 byte offsets in
// comments, pointer members widened to x64):
//   +0x00  vptr          -- polymorphic; ctor seats the base vtable
//                            (off_821B5514) then the CVoice vtable (off_821B5770),
//                            dtor reseats them in reverse down to off_82108FF4.
//   +0x04  miRefCount    -- intrusive refcount seeded to 1 by the ctor.
//   +0x08  mpAllocator   -- ref-counted allocator/collaborator passed to the
//                            ctor (AddRef'd via its vtable[0] on construct,
//                            Release'd via vtable[1] on destruct, and used to
//                            allocate the effect array in Initialize).
//   +0x0C  muVoiceType   -- byte voice type (ctor arg a3; GetVoiceType).
//   +0x10  mpListANext   -- intrusive circular list node A (self-linked by ctor).
//   +0x14  mpListAPrev
//   +0x18  mpListBNext   -- intrusive circular list node B (self-linked by ctor;
//                            (un)linked by OnStartVoice / OnStopVoice).
//   +0x1C  mpListBPrev
//   +0x20  mpFrameBuffer -- frame buffer (Initialize; released in the dtor).
//   +0x24  mpEffects     -- effect record array (SVoiceEffect[muNumEffects]).
//   +0x28  mpReserved28  -- word zeroed by the ctor; role not exercised by any
//                            CVoice method (FLAG: reserved).
//   +0x2C  mpProcessCallback -- per-process callback (Initialize; fired in Process).
//   +0x30  mpVoiceContext    -- user context pointer (Initialize; GetVoiceContext).
//   +0x34  mFormat       -- voice format pair (Get/SetVoiceFormat; Initialize).
//   +0x3C  muNumEffects  -- byte effect count (bounds every effect accessor).
//   +0x3D  muState       -- byte state/flag bits (GetVoiceState; Start/Stop/Process).
//   +0x3E  muMaxFrameSize    -- u16 running max effect frame size (CreateEffect).
//   +0x40  muFrameCountdown  -- u16 frame countdown (Stop seeds it, Process
//                                decrements it).
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// One entry of the CVoice effect array (mpEffects). The X360 build packs these
// at an 8-byte stride (the array is allocated as `8 * muNumEffects` bytes);
// members are accessed by name here, widened to the natural x64 layout.
struct SVoiceEffect
{
    void* mpEffect;   // +0x00  effect object (foreign vtable; opaque here)
    u8    muFlagA;    // +0x04  create flag (CreateEffect arg)
    u8    muState;    // +0x05  enabled/state byte (Get/SetEffectState)
    u8    muFlagB;    // +0x06  routing/mode flags
    u8    mPad07;     // +0x07  alignment
};

// The two-word voice format record carried at +0x34. Its exact field semantics
// are not exercised by this TU; the pair is copied verbatim by Get/SetVoiceFormat
// and seeded by Initialize.
struct SVoiceFormat
{
    u32 muField0; // +0x00
    u32 muField1; // +0x04
};

// ---------------------------------------------------------------------------
// IXAudioAllocator -- the ref-counted allocator/collaborator a CVoice is handed
// at construction (mpAllocator, +0x08). It is a foreign polymorphic object; this
// captures only the vtable slots the CVoice methods dispatch, at their asm-
// attested offsets (byte offset == slot index * 4). No slot is fabricated -- the
// intervening opaque slots keep the recovered ones at the right index.
//   [0] +0x00 AddRef   -- ctor: `(**a2)(a2)`.
//   [1] +0x04 Release  -- dtor: `(*(*mpAllocator + 4))(mpAllocator)`.
//   [5] +0x14 Allocate -- Initialize: `(*(**(a1+8)+20))(mpAllocator, byteSize)`.
// ---------------------------------------------------------------------------
struct IXAudioAllocatorVTable
{
    s32   (*mpAddRef)(void* apSelf);               // [0] +0x00
    s32   (*mpRelease)(void* apSelf);              // [1] +0x04
    void* mpSlot08;                                // [2] +0x08
    void* mpSlot0C;                                // [3] +0x0C
    void* mpSlot10;                                // [4] +0x10
    void* (*mpAllocate)(void* apSelf, u32 auSize); // [5] +0x14
};

struct IXAudioAllocator
{
    const IXAudioAllocatorVTable* mpVTable; // +0x00
};

// ---------------------------------------------------------------------------
// IXAudioEffect -- the polymorphic effect object the CVoice effect array holds
// (SVoiceEffect::mpEffect). Foreign vtable; only the slots the CVoice methods
// dispatch are modelled, at their asm-attested offsets.
//   [1] +0x04 Release  -- ~CVoice:        `(*(*effect + 4))(effect)`.
//   [2] +0x08 GetSize  -- CreateEffect:   `(*(*effect + 8))(effect, &sizeInfo)`.
//   [3] +0x0C GetParam -- GetEffectParam: `(*(*effect + 12))(effect, a, b, c)`.
//   [4] +0x10 SetParam -- SetEffectParam: `(*(*effect + 16))(effect, a, b, c)`.
//   [6] +0x18 Process  -- ProcessEffect:  `(*(*effect + 24))(effect, in, out)`.
// ---------------------------------------------------------------------------
struct IXAudioEffectVTable
{
    void* mpSlot00;                                          // [0] +0x00
    s32   (*mpRelease)(void* apSelf);                        // [1] +0x04
    s32   (*mpGetSize)(void* apSelf, void* apSizeOut);       // [2] +0x08
    s32   (*mpGetParam)(void* apSelf, s32 aP0, s32 aP1, s32 aP2); // [3] +0x0C
    s32   (*mpSetParam)(void* apSelf, s32 aP0, s32 aP1, s32 aP2); // [4] +0x10
    void* mpSlot14;                                          // [5] +0x14
    s32   (*mpProcess)(void* apSelf, s32 aIn, s32 aOut);     // [6] +0x18
};

struct IXAudioEffect
{
    const IXAudioEffectVTable* mpVTable; // +0x00
};

class CVoice
{
public:
    // @ 0x82C324D8 -- construct: store the allocator (+0x08) and AddRef it through
    // its vtable[0], seed the refcount (+0x04) to 1 and the voice type (+0x0C),
    // self-link the two intrusive list heads (+0x10 / +0x18) and clear the reserved
    // word (+0x28). The base + this-class vtable seats the asm shows
    // (off_821B5514 -> off_821B5770) are compiler-generated.
    CVoice(void* apAllocator, u8 auVoiceType);

    // @ 0x82C32278 -- destruct: release each live effect through its vtable[1],
    // release the frame buffer and the allocator (allocator vtable[1]). The vtable
    // reseats (off_821B5770 -> off_821B5514 -> off_82108FF4) are compiler-generated.
    virtual ~CVoice();

    // @ 0x82C32340 -- create one effect into `apEffect` through the engine effect
    // manager (gpEngine->mpEffectManager), then query its size (effect vtable[2])
    // to seat the record's flags/state and grow the running max frame size (+0x3E).
    s32 CreateEffect(SVoiceEffect* apEffect, const void* apDesc, u8 auFlag);

    // @ 0x82C323D8 -- dispatch an effect's GetParam (effect vtable[3]) by index.
    // Returns E_INVALIDARG (0x80070057) when the index is out of range.
    s32 GetEffectParam(u8 auIndex, s32 aParam0, s32 aParam1, s32 aParam2);

    // @ 0x82C32420 -- dispatch an effect's SetParam (effect vtable[4]) by index.
    // Returns E_INVALIDARG (0x80070057) when the index is out of range.
    s32 SetEffectParam(u8 auIndex, s32 aParam0, s32 aParam1, s32 aParam2);

    // @ 0x82C32E60 -- run ProcessEffect over every effect in order, stopping on the
    // first negative HRESULT.
    s32 ProcessEffects(int* apInOut);

    // Class-specific deallocation routing through the XAudio XMem manager -- the
    // free arm of the scalar deleting destructor @ 0x82C32578.
    void operator delete(void* apMemory);

    // @ 0x82C2FC78 -- write the byte voice type through the out-param. Returns 0.
    s32 GetVoiceType(u8* apType);

    // @ 0x82C2FC68 -- write the stored voice-context pointer (+0x30). Returns 0.
    s32 GetVoiceContext(void** apContext);

    // @ 0x82C2FC88 -- copy the two-word voice format (+0x34) out. Returns 0.
    s32 GetVoiceFormat(SVoiceFormat* apFormat);

    // @ 0x82C32178 -- copy the two-word voice format (+0x34) in. (The X360 asm
    // leaves r3 holding `this`; the source-level shape is a void setter.)
    void SetVoiceFormat(const SVoiceFormat* apFormat);

    // @ 0x82C30828 -- write the byte state/flags (+0x3D) out. Returns 0.
    s32 GetVoiceState(u8* apState);

    // @ 0x82C32468 -- read an effect's state byte by index. Returns E_INVALIDARG
    // (0x80070057) when the index is out of range.
    s32 GetEffectState(u8 aIndex, u8* apState);

    // @ 0x82C324A0 -- write an effect's state byte by index. Returns E_INVALIDARG
    // (0x80070057) when the index is out of range.
    s32 SetEffectState(u8 aIndex, u8 aState);

    // ----- BLOCKED bodies (declared with asm-grounded signatures so the class
    //        interface + the callers/callees that bind against them compile; each
    //        body still needs an un-recovered dependency, per XAudioVoice.cpp) ---

    // @ 0x82C32D28 -- seed the format/context/callback, size + create the frame
    // buffer and effect array, then create the user effects.
    // BLOCKED: the source-init descriptor carries the channel count as an interior
    // sub-byte of the format word (`lbz 1(desc)`, endianness-dependent) and
    // XAudioCreateFrameBuffer's on-stack descriptor layout is un-recovered.
    s32 Initialize(const void* apDesc);

    // @ 0x82C32190 -- static: the extra bytes a voice built from `apDesc` needs
    // (frame-buffer size + per-effect sizes).
    // BLOCKED: the same interior channel sub-byte + the un-recovered
    // XAudioQueryFrameBufferSize / XAudioEffectManager_QueryEffectSize descriptors.
    static s32 GetObjectAdditionalSize(const void* apDesc, u32* apSize);

    // @ 0x82C32650 -- process one effect record, resolving its input/output tap.
    // BLOCKED: reads the per-hardware-thread index from the r13 KPCR
    // (`lbz 0x10C(r13)`) and indexes an un-recovered gpEngine sub-table
    // (`off_832BB944 + 8*idx + 12`).
    s32 ProcessEffect(SVoiceEffect* apEffect, int* apInOut);

    // @ external -- populate the effect array from the user effect table. Called by
    // Initialize / CreateEffect; its body is a separate TU (not in this ledger set).
    s32 CreateUserEffects(const void* apEffectTable);

    // @ 0x82C320A8 -- render one voice frame: fire the process callback, then
    // dispatch the voice's own generate/silence slots.
    // BLOCKED: dispatches this class's OWN vtable slots 15/18/21 (bytes +60/+72/+84)
    // whose order is not attested (the CVoice ctor that seats off_821B5770 is not
    // exported); declaring them would fabricate phantom slots.
    s32 Process();

    // @ 0x82C32770 -- start the voice under the module processing lock.
    // BLOCKED: Xbox spinlock/IRQL primitives (KeRaiseIrqlToDpcLevel /
    // KeAcquireSpinLockAtRaisedIrql / KeReleaseSpinLockFromRaisedIrql / KfLowerIrql)
    // + the un-recovered module lock/state globals (unk_83222C28 / dword_83222C2C /
    // dword_83222C30 / byte_83222C34).
    s32 Start();

    // @ 0x82C32988 -- stop the voice; when synchronous, block on the active voice
    // through the engine first.
    // BLOCKED: the same Xbox spinlock/IRQL primitives + module lock/state globals
    // (CEngine::BlockOnActiveVoice is now available, the lock block is not).
    s32 Stop(u8 auFlag);

    // @ 0x82C30FE8 -- link the voice into its engine voice list on start.
    // BLOCKED: the same Xbox spinlock/IRQL primitives + module lock/state globals
    // (CEngine::GetVoiceList is now available, the lock block is not).
    s32 OnStartVoice();

    // @ 0x82C30E70 -- unlink the voice from its engine voice list on stop.
    // BLOCKED: the same Xbox spinlock/IRQL primitives + module lock/state globals.
    s32 OnStopVoice();

    // ----- layout (declared in X360 offset order; see the header banner) -----
    int          miRefCount;        // +0x04
    void*        mpAllocator;       // +0x08
    u8           muVoiceType;       // +0x0C
    void*        mpListANext;       // +0x10
    void*        mpListAPrev;       // +0x14
    void*        mpListBNext;       // +0x18
    void*        mpListBPrev;       // +0x1C
    void*        mpFrameBuffer;     // +0x20
    SVoiceEffect* mpEffects;        // +0x24
    void*        mpReserved28;      // +0x28  (FLAG: zeroed by ctor, role unknown)
    void*        mpProcessCallback; // +0x2C
    void*        mpVoiceContext;    // +0x30
    SVoiceFormat mFormat;           // +0x34
    u8           muNumEffects;      // +0x3C
    u8           muState;           // +0x3D
    u16          muMaxFrameSize;    // +0x3E
    u16          muFrameCountdown;  // +0x40
};

// ===========================================================================
// OBSERVED CVoice VTABLE DISPATCHES (attested from the class:XAUDIO::CEngine asm,
// wave W40). These are the CVoice-base vtable slots the engine dispatches on a
// voice; because a derived voice (CRoutedVoice / CSourceVoice / CMasteringVoice)
// overrides a slot at the SAME index, these offsets are CVoice-base slots. Byte
// offset into the vtable == slot index * 4.
//
//   +0x0C  slot 3  -- `(*(*voice + 12))(voice)` : the shared XAudio ref-counted-
//                     object Release / deleting-destructor slot. Called on a
//                     queued voice in CEngine::~CEngine, on the mastering voice at
//                     CEngine +0x40, and on the CEngine object itself in
//                     CreateObject's failure path -- i.e. it sits in the shared
//                     base vtable image (off_82108FF4) common to CEngine/CVoice/
//                     the mastering voice. Returns/ignores; frees the object.
//   +0x34  slot 13 -- `(*(*voice + 52))(voice, u8 out[8])` : a category query used
//                     by CEngine::GetVoiceList. `out[0]` is a small selector
//                     (0 => master list, 1 => an active list, else none).
//   +0x44  slot 17 -- `(*(*voice + 68))(voice) -> s32` : Process. Corroborated on
//                     a generic queued voice (CEngine::ProcessVoiceList) and on
//                     the mastering voice (CEngine::Process). >= 0 on success.
//
// The intervening slots (0..2, 4..12, 14..16, 18+) are NOT yet recovered: the
// CVoice ctor asm that seats the vtable (off_821B5770) is not in the CEngine
// dossier and the vtable rodata blob is not exported, so the exact index of every
// slot cannot be attested. Per the fidelity rules the in-order `virtual`
// declarations are therefore DEFERRED rather than padded with phantom slots --
// only ~CVoice (slot 0, the vptr anchor) is declared above. A later wave with the
// CVoice ctor asm can promote these three attested slots to real overrides.
//
// W40 FINISH: with XAUDIO::CEngine + the effect/allocator foreign interfaces
// (IXAudioEffect / IXAudioAllocator) modelled above, the effect- and allocator-
// driven CVoice methods are now implemented (ctor, ~CVoice, CreateEffect,
// Get/SetEffectParam, ProcessEffects, operator delete). CVoice::Process stays
// BLOCKED: it dispatches this class's OWN vtable slots 15/18/21, which the note
// above shows are still un-attested, so implementing it would fabricate phantom
// slots. Start/Stop/OnStart/OnStopVoice + ProcessEffect stay BLOCKED on the Xbox
// kernel spinlock/IRQL primitives + KPCR reads + the un-recovered module state
// globals; Initialize/GetObjectAdditionalSize on the interior format sub-byte +
// the un-recovered frame-buffer descriptors (see XAudioVoice.cpp).
// ===========================================================================

} // namespace XAUDIO
