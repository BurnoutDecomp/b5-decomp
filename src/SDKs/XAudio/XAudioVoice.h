#pragma once

// ===========================================================================
// XAUDIO::CVoice -- the base XAudio voice object in BURNOUT_X360_ARTIST.XEX.
// It is the shared base of the concrete voices (CSourceVoice, CSubmixVoice,
// CMasteringVoice, CRoutedVoice). This header is the canonical OWNING home for
// the XAUDIO::CVoice TU.
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of the CVoice methods PLUS the class
// vtable rodata recovered from the local IDA DB (wave D; dump in
// scratchpad/waveD/cvoice_vtables_dump.txt). `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming convention.
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
//   +0x0C  muVoiceType   -- byte voice type (ctor arg a3; GetVoiceType). Read by
//                            CEngine::GetVoiceList through vtable slot 13 as the
//                            list selector (0 => master list, 1 => active list).
//   +0x10  mListA        -- intrusive circular list link A (self-linked by ctor).
//   +0x18  mListB        -- intrusive circular list link B (self-linked by ctor;
//                            tail-inserted into the engine list's back active
//                            sub-list by OnStartVoice, unlinked by OnStopVoice).
//   +0x20  mpFrameBuffer -- CFrameBuffer* (Initialize; released in the dtor;
//                            returned by GetFrameBuffer, vtable slot 18).
//   +0x24  mpEffects     -- effect record array (SVoiceEffect[muNumEffects]).
//   +0x28  mpReserved28  -- word zeroed by the ctor; role not exercised by any
//                            CVoice method (FLAG: reserved).
//   +0x2C  mpProcessCallback -- per-process callback (Initialize; fired in
//                            Process with a pointer to a stack COPY of the
//                            context word).
//   +0x30  mpVoiceContext    -- user context pointer (Initialize; GetVoiceContext).
//   +0x34  mFormat       -- voice format pair (Get/SetVoiceFormat; Initialize).
//                            Byte +1 of the first word is the CHANNEL COUNT
//                            (`lbz 1(desc)` in Initialize/GetObjectAdditionalSize,
//                            seeded into the CFrameBufferDesc they build).
//   +0x3C  muNumEffects  -- byte effect count (bounds every effect accessor).
//   +0x3D  muState       -- byte state/flag bits (GetVoiceState; Start/Stop/
//                            Process). bit0 = started, bit2 = async stop pending,
//                            bit6 = cleared by Start (role not exercised here).
//   +0x3E  muMaxFrameSize    -- u16 running max effect frame size (CreateEffect).
//   +0x40  muFrameCountdown  -- u16 frame countdown (async Stop seeds it from
//                                muMaxFrameSize, Process decrements it and calls
//                                the virtual Stop(1) when it reaches 0).
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioPacketCtx.h" // XAUDIO::XAudioListLink (intrusive links)

namespace XAUDIO
{

class CFrameBuffer;              // XAudioFrameBuffer.h (pointer-only here)
struct CFrameBufferDesc;         // XAudioFrameBuffer.h (built on-stack by Initialize)
struct SVoiceOutputConfig;       // XAudioRoutedVoice.h (pure-virtual slot 9 param)
struct SVoiceOutputVolumeConfig; // XAudioRoutedVoice.h (pure-virtual slot 10 param)

// One entry of the CVoice effect array (mpEffects). The X360 build packs these
// at an 8-byte stride (the array is allocated as `8 * muNumEffects` bytes);
// members are accessed by name here, widened to the natural x64 layout.
struct SVoiceEffect
{
    void* mpEffect;   // +0x00  effect object (foreign vtable; opaque here)
    u8    muFlagA;    // +0x04  create flag (CreateEffect arg)
    u8    muState;    // +0x05  enabled/state byte (Get/SetEffectState; bit0
                      //        gates ProcessEffect)
    u8    muFlagB;    // +0x06  routing/mode flags from the effect's size query
                      //        (ProcessEffect: (..&6)==6 => ping-pong out-of-place,
                      //        &3 => input-only, &4 => output-only)
    u8    mPad07;     // +0x07  alignment
};

// The two-word voice format record carried at +0x34, copied verbatim by
// Get/SetVoiceFormat and seeded by Initialize from the creation descriptor.
// The interior CHANNEL-COUNT byte (word0 byte +1) is the one field this TU
// reads out of it (`lbz 1(desc)` in Initialize / GetObjectAdditionalSize); the
// shape mirrors CFrameBufferFormat's first word (XAudioFrameBuffer.h).
struct SVoiceFormat
{
    u8  muByte0;    // +0x00  leading byte of the format word (role unknown)
    u8  muChannels; // +0x01  interleaved channel count
    u16 muHalf2;    // +0x02  trailing half of the format word (role unknown)
    u32 muField1;   // +0x04  second format word (role not exercised by this TU)
};

// The per-voice effect table handed to Initialize / GetObjectAdditionalSize /
// CreateUserEffects inside the voice-init descriptor (+0x08). Count byte at
// +0x00 (`lbz 0(tbl)`), pointer to the per-effect creation-descriptor array at
// +0x04 (`lwz 4(tbl)`, entries read `lwzx entry(tbl->descs, 4*i)`).
struct SVoiceEffectTable
{
    u8                 muNumEffects;  // +0x00
    // +0x01..+0x03 alignment padding (X360)
    const void* const* mpEffectDescs; // +0x04  per-effect creation descriptors
                                      //        (opaque; fed to CreateEffect /
                                      //        XAudioEffectManager_QueryEffectSize)
};

// ---------------------------------------------------------------------------
// The voice creation/init descriptor consumed by Initialize (`a2`) and
// GetObjectAdditionalSize (`a1`). Field offsets are the X360 asm displacements;
// the base descriptor spans +0x00..+0x17 (the derived voices append their own
// fields from +0x18, e.g. CRoutedVoice's route count -- see the CRoutedVoice
// GetObjectAdditionalSize asm reading +0x18/+0x19).
//   +0x00 mFormat            -- two-word voice format (copied to CVoice +0x34;
//                               byte +1 = channel count).
//   +0x08 mpEffectTable      -- optional per-voice effect table.
//   +0x0C mpFrameBuffer      -- optional EXTERNAL frame buffer (AddRef'd and
//                               adopted; when null Initialize creates one via
//                               XAudioCreateFrameBuffer at 48 kHz).
//   +0x10 mpfnProcessCallback-- optional per-process callback (fired by Process).
//   +0x14 mpContext          -- user context word (GetVoiceContext).
// ---------------------------------------------------------------------------
struct SVoiceInit
{
    SVoiceFormat             mFormat;              // +0x00
    const SVoiceEffectTable* mpEffectTable;        // +0x08
    CFrameBuffer*            mpFrameBuffer;        // +0x0C
    void (*mpfnProcessCallback)(void** appContext);// +0x10
    void*                    mpContext;            // +0x14
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
//   [6] +0x18 Process  -- ProcessEffect:  `(*(*effect + 24))(effect, in, out)`
//                          where in/out are frame-buffer taps.
// ---------------------------------------------------------------------------
struct IXAudioEffectVTable
{
    void* mpSlot00;                                          // [0] +0x00
    s32   (*mpRelease)(void* apSelf);                        // [1] +0x04
    s32   (*mpGetSize)(void* apSelf, void* apSizeOut);       // [2] +0x08
    s32   (*mpGetParam)(void* apSelf, s32 aP0, s32 aP1, s32 aP2); // [3] +0x0C
    s32   (*mpSetParam)(void* apSelf, s32 aP0, s32 aP1, s32 aP2); // [4] +0x10
    void* mpSlot14;                                          // [5] +0x14
    s32   (*mpProcess)(void* apSelf, CFrameBuffer* apIn, CFrameBuffer* apOut); // [6] +0x18
};

struct IXAudioEffect
{
    const IXAudioEffectVTable* mpVTable; // +0x00
};

// ===========================================================================
// CVoice VTABLE (RECOVERED -- wave D). The full 22-slot table was dumped from
// the rodata at off_821B5770 (the CVoice vtable the ctor seats) and cross-
// checked against the derived images (CSourceVoice off_821B5568, CSubmixVoice
// off_821B55C8, CMasteringVoice off_821B5628, CRoutedVoice off_821B5708), which
// override the SAME indices. Byte offset into the vtable == slot index * 4.
// The `virtual` declarations below are in EXACT slot order so the PC build
// emits the same table; do NOT reorder or insert.
//
//   slot  0 +0x00  `scalar deleting destructor'    (virtual ~CVoice + op delete)
//   slot  1 +0x04  AddRef            -> shared CObjectRefCount::AddRef body
//   slot  2 +0x08  Release           -> shared CObjectRefCount::Release body
//   slot  3 +0x0C  AbsoluteRelease   -> shared CBatchAllocatedObject::AbsoluteRelease
//   slot  4 +0x10  GetRefCount       -> shared `lwz r3,4(r3); blr` leaf (FLAG:
//                    name reconstructed from the body -- returns +0x04 refcount;
//                    the X360 label is an unrelated ICF-fold victim)
//   slot  5 +0x14  GetEffectState    @ 0x82C32468 (committed)
//   slot  6 +0x18  SetEffectState    @ 0x82C324A0 (committed)
//   slot  7 +0x1C  GetEffectParam    @ 0x82C323D8 (committed)
//   slot  8 +0x20  SetEffectParam    @ 0x82C32420 (committed)
//   slot  9 +0x24  SetVoiceOutput        = 0 (_purecall in the CVoice image;
//                    CRoutedVoice/CSubmixVoice override)
//   slot 10 +0x28  SetVoiceOutputVolume  = 0 (_purecall; CRoutedVoice overrides)
//   slot 11 +0x2C  GetVoiceContext   @ 0x82C2FC68 (committed)
//   slot 12 +0x30  GetVoiceState     @ 0x82C30828 (committed)
//   slot 13 +0x34  GetVoiceType      @ 0x82C2FC78 (committed; CEngine::GetVoiceList
//                    dispatches this slot for the list selector)
//   slot 14 +0x38  Start             @ 0x82C32770
//   slot 15 +0x3C  Stop              @ 0x82C32988 (Process dispatches it with
//                    flag 1 when the async-stop countdown expires)
//   slot 16 +0x40  GetVoiceFormat    @ 0x82C2FC88 (committed)
//   slot 17 +0x44  Process           @ 0x82C320A8 (CEngine::ProcessVoiceList /
//                    CEngine::Process dispatch this slot)
//   slot 18 +0x48  GetFrameBuffer    @ 0x82C30838 = `lwz r3,0x20(r3); blr`
//                    (returns mpFrameBuffer; NOT in this TU's 22-function ledger
//                    set -- declaration only, body belongs to its own TU)
//   slot 19 +0x4C  OnStartVoice      @ 0x82C30FE8 (Start dispatches this slot)
//   slot 20 +0x50  OnStopVoice       @ 0x82C30E70 (Stop dispatches this slot)
//   slot 21 +0x54  ProcessEffects    @ 0x82C32E60 (committed; Process dispatches
//                    this slot with the tap array)
//
// The CVoice table ENDS at slot 21 (the next rodata label follows at +0x58).
// The three routed-voice images (CRoutedVoice/CSourceVoice/CSubmixVoice) append
// ONE extra slot 22 = the two-arg virtual SetVoiceFormat CRoutedVoice
// introduces; it is NOT a CVoice slot (CVoice::SetVoiceFormat stays non-virtual).
// ===========================================================================

class CVoice
{
public:
    // @ 0x82C324D8 -- construct: store the allocator (+0x08) and AddRef it through
    // its vtable[0], seed the refcount (+0x04) to 1 and the voice type (+0x0C),
    // self-link the two intrusive list links (+0x10 / +0x18) and clear the reserved
    // word (+0x28). The base + this-class vtable seats the asm shows
    // (off_821B5514 -> off_821B5770) are compiler-generated.
    CVoice(void* apAllocator, u8 auVoiceType);

    // ----- virtuals in EXACT recovered vtable-slot order (see banner) --------

    // [slot 0] @ 0x82C32278 -- destruct: release each live effect through its
    // vtable[1], release the frame buffer and the allocator (allocator vtable[1]).
    // The vtable reseats (off_821B5770 -> off_821B5514 -> off_82108FF4) are
    // compiler-generated; the scalar deleting destructor @ 0x82C32578 is the
    // compiler-emitted thunk over this dtor + the class operator delete.
    virtual ~CVoice();

    // [slots 1-4] -- the shared XAudio ref-counted-object base slots. On X360
    // every voice-cluster vtable points these at the SAME shared bodies
    // (CObjectRefCount::AddRef @0x82C2F008 / ::Release @0x8295F428 /
    // CBatchAllocatedObject::AbsoluteRelease @0x82C2D828 / the refcount-getter
    // leaf @0x8295F4A0). Their bodies belong to those TUs (committed homes
    // XAudioObjectRefCount.h / XAudioBatchAllocatedObject.h); they are declared
    // here ONLY to hold their slot positions -- do not body them in this TU.
    virtual s32 AddRef();          // [slot 1] pre-increment miRefCount
    virtual s32 Release();         // [slot 2] decrement; destroys at 0
    virtual s32 AbsoluteRelease(); // [slot 3] guarded `delete this` (CEngine
                                   //          dispatches this slot on teardown)
    virtual s32 GetRefCount();     // [slot 4] returns miRefCount (FLAG: name
                                   //          reconstructed from the leaf body)

    // [slot 5] @ 0x82C32468 -- read an effect's state byte by index. Returns
    // E_INVALIDARG (0x80070057) when the index is out of range.
    virtual s32 GetEffectState(u8 aIndex, u8* apState);

    // [slot 6] @ 0x82C324A0 -- write an effect's state byte by index. Returns
    // E_INVALIDARG (0x80070057) when the index is out of range.
    virtual s32 SetEffectState(u8 aIndex, u8 aState);

    // [slot 7] @ 0x82C323D8 -- dispatch an effect's GetParam (effect vtable[3])
    // by index. Returns E_INVALIDARG (0x80070057) when the index is out of range.
    virtual s32 GetEffectParam(u8 auIndex, s32 aParam0, s32 aParam1, s32 aParam2);

    // [slot 8] @ 0x82C32420 -- dispatch an effect's SetParam (effect vtable[4])
    // by index. Returns E_INVALIDARG (0x80070057) when the index is out of range.
    virtual s32 SetEffectParam(u8 auIndex, s32 aParam0, s32 aParam1, s32 aParam2);

    // [slot 9] -- pure (_purecall in the CVoice image): (re)bind the voice's
    // output set. Overridden by CRoutedVoice / CSubmixVoice.
    virtual s32 SetVoiceOutput(const SVoiceOutputConfig* apConfig) = 0;

    // [slot 10] -- pure (_purecall in the CVoice image): set the per-output
    // volumes. Overridden by CRoutedVoice.
    virtual s32 SetVoiceOutputVolume(const SVoiceOutputVolumeConfig* apConfig) = 0;

    // [slot 11] @ 0x82C2FC68 -- write the stored voice-context pointer (+0x30).
    virtual s32 GetVoiceContext(void** apContext);

    // [slot 12] @ 0x82C30828 -- write the byte state/flags (+0x3D) out. Returns 0.
    virtual s32 GetVoiceState(u8* apState);

    // [slot 13] @ 0x82C2FC78 -- write the byte voice type through the out-param.
    virtual s32 GetVoiceType(u8* apType);

    // [slot 14] @ 0x82C32770 -- start the voice under the module recursive
    // spin-lock: when not started, set state bit0 (clearing bit6) and dispatch
    // the virtual OnStartVoice (slot 19); when already started, clear a pending
    // async-stop (state bit2). Returns 0.
    virtual s32 Start();

    // [slot 15] @ 0x82C32988 -- stop the voice under the module lock. Async
    // (auFlag bit0 clear): mark state bit2 and seed muFrameCountdown from
    // muMaxFrameSize (Process finishes the stop). Sync (bit0 set): block on the
    // active voice through the engine, clear state bits 0/2/6 and dispatch the
    // virtual OnStopVoice (slot 20). Returns 0.
    virtual s32 Stop(u8 auFlag);

    // [slot 16] @ 0x82C2FC88 -- copy the two-word voice format (+0x34) out.
    virtual s32 GetVoiceFormat(SVoiceFormat* apFormat);

    // [slot 17] @ 0x82C320A8 -- render one voice frame: seed the tap array from
    // the virtual GetFrameBuffer (slot 18), fire the process callback with a
    // pointer to a COPY of the context word, finish an expired async stop via
    // the virtual Stop(1) (slot 15), else run the virtual ProcessEffects
    // (slot 21) over the taps.
    virtual s32 Process();

    // [slot 18] @ 0x82C30838 -- `lwz r3,0x20(r3); blr`: return mpFrameBuffer.
    // NOT in this TU's 22-function ledger set: declaration only here; the body
    // belongs to its own (unhomed) ledger entry. Overridden by CSourceVoice.
    virtual CFrameBuffer* GetFrameBuffer();

    // [slot 19] @ 0x82C30FE8 -- on start: resolve this voice's engine voice list
    // (CEngine::GetVoiceList) and, under the module lock, tail-insert the +0x18
    // link into the list's BACK active sub-list (mpActiveB) at the link offset
    // the sub-list head carries in its mpBase word. Returns 0.
    virtual s32 OnStartVoice();

    // [slot 20] @ 0x82C30E70 -- on stop: under the module lock, unlink the +0x18
    // link from whatever list holds it and re-seat it self-circular. Returns 0.
    virtual s32 OnStopVoice();

    // [slot 21] @ 0x82C32E60 -- run ProcessEffect over every effect in order,
    // stopping on the first negative HRESULT. `apInOut` is the frame-buffer tap
    // slot threaded through the effect chain.
    virtual s32 ProcessEffects(CFrameBuffer** apInOut);

    // ----- non-virtual methods ----------------------------------------------

    // @ 0x82C32340 -- create one effect into `apEffect` through the engine effect
    // manager (gpEngine->mpEffectManager), then query its size (effect vtable[2])
    // to seat the record's flags/state and grow the running max frame size (+0x3E).
    s32 CreateEffect(SVoiceEffect* apEffect, const void* apDesc, u8 auFlag);

    // @ 0x82C32178 -- copy the two-word voice format (+0x34) in. (The X360 asm
    // leaves r3 holding `this`; the source-level shape is a void setter.)
    // Non-virtual in CVoice: the vtable ends at slot 21. (CRoutedVoice introduces
    // a DIFFERENT two-arg virtual SetVoiceFormat as its own slot 22.)
    void SetVoiceFormat(const SVoiceFormat* apFormat);

    // @ 0x82C32D28 -- seed the format/callback/context from the descriptor,
    // adopt (AddRef) the external frame buffer or create one at 48 kHz via
    // XAudioCreateFrameBuffer, allocate the 8-byte-stride effect array through
    // the allocator (vtable[5]) and populate it via CreateUserEffects.
    s32 Initialize(const SVoiceInit* apInit);

    // @ 0x82C32190 -- static: the extra bytes a voice built from `apInit` needs
    // (frame-buffer size when no external buffer is supplied + the per-effect
    // sizes from XAudioEffectManager_QueryEffectSize + 8 bytes per effect record).
    static s32 GetObjectAdditionalSize(const SVoiceInit* apInit, u32* apSize);

    // @ 0x82C32650 -- process one effect record: resolve the input/output taps
    // from the record's routing flags (out-of-place effects ping-pong between the
    // current hardware-thread's engine frame-buffer pair) and dispatch the
    // effect's Process (vtable[6]).
    s32 ProcessEffect(SVoiceEffect* apEffect, CFrameBuffer** apInOut);

    // @ external -- populate the effect array from the per-voice effect table.
    // Called by Initialize; its body is a separate TU (not in this ledger set).
    s32 CreateUserEffects(const SVoiceEffectTable* apEffectTable);

    // Class-specific deallocation routing through the XAudio XMem manager -- the
    // free arm of the scalar deleting destructor @ 0x82C32578.
    void operator delete(void* apMemory);

    // ----- layout (declared in X360 offset order; see the header banner) -----
    int            miRefCount;        // +0x04
    void*          mpAllocator;       // +0x08
    u8             muVoiceType;       // +0x0C
    XAudioListLink mListA;            // +0x10  (self-linked by the ctor)
    XAudioListLink mListB;            // +0x18  (engine active-list link;
                                      //         OnStartVoice / OnStopVoice)
    CFrameBuffer*  mpFrameBuffer;     // +0x20
    SVoiceEffect*  mpEffects;         // +0x24
    void*          mpReserved28;      // +0x28  (FLAG: zeroed by ctor, role unknown)
    void (*mpProcessCallback)(void** appContext); // +0x2C
    void*          mpVoiceContext;    // +0x30
    SVoiceFormat   mFormat;           // +0x34
    u8             muNumEffects;      // +0x3C
    u8             muState;           // +0x3D
    u16            muMaxFrameSize;    // +0x3E
    u16            muFrameCountdown;  // +0x40
};

} // namespace XAUDIO
