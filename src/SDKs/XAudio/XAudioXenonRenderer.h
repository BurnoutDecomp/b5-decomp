#pragma once

// ===========================================================================
// XAUDIO::CXenonRenderer -- the Xbox 360 (Xenon) audio render-driver client in
// BURNOUT_X360_ARTIST.XEX. It marries the XAudio render-driver frame pump to the
// XBOX-Media (XBM) audio-capture ring. This header is the canonical OWNING home
// for:
//
//     XAUDIO::CXenonRenderer::CXenonRenderer                              @ 0x829630B0
//     XAUDIO::CXenonRenderer::~CXenonRenderer                             @ 0x82962EC0
//     XAUDIO::CXenonRenderer::CaptureXBMAudioFrame                        @ 0x82962E00
//     XAUDIO::CXenonRenderer::GetInfo                                     @ 0x82962FC8
//     XAUDIO::CXenonRenderer::Initialize                                  @ 0x82962D78
//     XAUDIO::CXenonRenderer::Process                                     @ 0x82963000
//     XAUDIO::CXenonRenderer::QueryInterface                             @ 0x8296B1E8
//     XAUDIO::CXenonRenderer::Release                                     @ 0x82962F80
//     XAUDIO::CXenonRenderer::SetCallback                                 @ 0x82962D98
//     XAUDIO::CXenonRenderer::GetContext`adjustor{4}'                     @ 0x82963C80
//     XAUDIO::CXenonRenderer::Release`adjustor{4}'                        @ 0x82963060
//     XAUDIO::CXenonRenderer::`vector deleting destructor'                @ 0x82963080
//     XAUDIO::CXenonRenderer::`vector deleting destructor'`adjustor{4}'   @ 0x82962F78
//
// The four `adjustor{4}' / `vector deleting destructor' entries are
// compiler-synthesised thunks over the real bodies below + the base
// destructor/GetContext; per the XAudio MI precedent (CBatchAllocator,
// CSourceEffect) they are documented but not emitted as source:
//   - Release`adjustor{4}' @ 0x82963060           -> Release(this - 4)
//   - `vector deleting destructor' @ 0x82963080   -> ~CXenonRenderer(); return this
//   - `vd dtor'`adjustor{4}' @ 0x82962F78         -> `vector deleting destructor'(this - 4)
//   - GetContext`adjustor{4}' @ 0x82963C80        -> XAUDIO::CRouterEffect::GetContext(this - 4)
//     (GetContext itself is INHERITED from the secondary CRouterEffect base and
//      has no CXenonRenderer body -- it is not reconstructed here.)
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed purely from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// INHERITANCE (two-vptr MI, attested by the ctor's two staged secondary-vptr
// stores, the dtor's staged vptr re-seats, and the three `adjustor{4}' thunks
// that recover `this` from the secondary subobject at +4):
//   - Primary base   (vptr @+0x00, off_82109098) : the XAudio render-driver
//     client interface -- carries Initialize/Process/GetInfo/QueryInterface/
//     Release/SetCallback plus the two pure virtuals dispatched here (Prepare
//     @slot 0x1C, Render @slot 0x20). Un-homed; modelled as RendererVTable.
//   - Secondary base (vptr @+0x04, off_8210907C, re-seated to the CObjectRefCount
//     image off_82108FF4 by the base dtor) : XAUDIO::CRouterEffect, itself a
//     CObjectRefCount -- supplies the intrusive ref-count (Release) and
//     GetContext. CObjectRefCount is the homed shape of that subobject, so it is
//     embedded by value here (NOT re-forked) to seat the +4 vptr / +8 count.
//
// X360 LAYOUT (32-bit byte offsets from the asm load/store displacements; the
// host build's pointer width shifts the later offsets -- semantic parity by
// named members, the exact byte offsets are an X360 compiler artifact):
//   +0x00  mpRendererVTable      -- primary render-driver-client vtable.
//   +0x04  mRefCountBase.mpVTable-- secondary (CRouterEffect/CObjectRefCount) vptr.
//   +0x08  mRefCountBase.mRefCount-- intrusive reference count (ctor sets 1).
//   +0x0C  mpContext             -- client context (create-params +0x04); forwarded
//                                   to XAudioRegisterRenderDriverClient by SetCallback.
//   +0x10  muLastCaptureBuffer   -- address of the last XBM frame captured (ring).
//   +0x14  muCaptureRepeatCount  -- consecutive-same-slot capture counter (<=0xE
//                                   gates the copy).
//   +0x18  mpRenderDriverClient  -- XAudio render-driver client handle (registered
//                                   by SetCallback; the frame sink of Process; the
//                                   dtor unregisters it). NOTE: the X360 ctor does
//                                   NOT initialise this field -- SetCallback seats
//                                   it before Process/dtor read it (faithful).
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioObjectRefCount.h" // XAUDIO::CObjectRefCount (secondary subobject shape)
#include "SDKs/XAudio/XAudioFrameBuffer.h"    // XAUDIO::CFrameBufferProcessingData (Process/Capture payload)

namespace XAUDIO
{

// Create parameters passed to the constructor (by CMasteringVoice::Initialize).
// Only the context word the ctor reads (+0x04) is attested. FLAG: minimal
// reconstruction of an un-homed descriptor.
struct SXenonRendererCreateParams
{
    u32   muReserved0; // +0x00 not read by this TU (FLAG)
    void* mpContext;   // +0x04 -> CXenonRenderer::mpContext
};

// Initialise parameters passed to Initialize. Only the +0x08 word (forwarded to
// the primary vtable's Prepare slot) is attested. FLAG: minimal reconstruction.
struct SXenonRendererInitParams
{
    u8    mReserved0[8]; // +0x00 not read by this TU (FLAG)
    void* mpConfig;      // +0x08 -> Prepare argument
};

// The renderer-info block GetInfo fills. The asm writes a byte (2) at +0x00 and a
// halfword (0) at +0x02; only those two fields are attested. FLAG: field roles
// inferred (byte 2 == channel count / stereo).
struct SXenonRendererInfo
{
    u8  muChannels;   // +0x00 -- attested value 2
    u8  muReserved1;  // +0x01 -- not written by this TU
    u16 muReserved2;  // +0x02 -- attested value 0
};

// The client registration record SetCallback builds on the stack and hands to
// XAudioRegisterRenderDriverClient. The X360 asm reserves 12 words of scratch
// but writes only the first two; the rest are left uninitialised. FLAG: only the
// two attested words are modelled.
struct SRenderDriverRegistration
{
    void* mpCallback; // [0] = SetCallback's argument
    void* mpContext;  // [1] = CXenonRenderer::mpContext
};

class CXenonRenderer
{
public:
    // The primary (render-driver-client) vtable. Only the two pure-virtual slots
    // this TU dispatches -- Prepare @+0x1C (Initialize) and Render @+0x20
    // (Process) -- are attested; the lower slots are opaque here.
    struct RendererVTable
    {
        void* mpSlot00; // [0] @+0x00
        void* mpSlot04; // [1] @+0x04
        void* mpSlot08; // [2] @+0x08
        void* mpSlot0C; // [3] @+0x0C
        void* mpSlot10; // [4] @+0x10
        void* mpSlot14; // [5] @+0x14
        void* mpSlot18; // [6] @+0x18
        // [7] @+0x1C -- Prepare(this, pConfig): Initialize dispatches, returns HRESULT.
        s32  (*mpPrepare)(CXenonRenderer* apSelf, void* apConfig);
        // [8] @+0x20 -- Render(this, pProcessingData): Process dispatches (result ignored).
        void (*mpRender)(CXenonRenderer* apSelf, CFrameBufferProcessingData* apData);
    };

    // @ 0x829630B0 -- seat both vtables, set the ref count to 1, stash the client
    // context (create-params +0x04) and zero the capture-ring bookkeeping. Does
    // not touch mpRenderDriverClient (see layout note).
    explicit CXenonRenderer(const SXenonRendererCreateParams* apParams);

    // @ 0x82962EC0 -- re-seat both vtables, latch the XBM capture ring shut,
    // unregister the render-driver client (nulling the handle on success), then
    // re-seat the secondary vptr to the CObjectRefCount base image.
    ~CXenonRenderer();

    // @ 0x82962E00 -- atomically claim the next 6144-byte slot in the XBM audio
    // capture ring and copy `apSource`'s sample buffer into it, unless the ring is
    // latched (== 6144) or the same slot has already been captured more than 0xE
    // times. Returns the claimed slot address (or `this` when latched, or null
    // when the ring yields slot 0).
    void* CaptureXBMAudioFrame(CFrameBufferProcessingData* apSource);

    // @ 0x82962FC8 -- report renderer info (channels = 2, flags = 0). Returns 0.
    s32 GetInfo(SXenonRendererInfo* apInfo);

    // @ 0x82962D78 -- clear the XBM capture reset marker, then dispatch the
    // primary vtable's Prepare slot with the config word from `apParams` (+0x08).
    s32 Initialize(const SXenonRendererInitParams* apParams);

    // @ 0x82963000 -- pull a processing frame from `apFrameBuffer`, run it through
    // the primary vtable's Render slot, and submit the rendered buffer to the
    // render-driver client. Returns the driver submit result, or the frame-fetch
    // error when it is negative.
    s32 Process(void* apFrameBuffer);

    // @ 0x8296B1E8 -- trivial interface query: store `this` through the out-param
    // and return it.
    void* QueryInterface(void** appInterface);

    // @ 0x82962F80 -- the primary interface's Release: decrement the ref count on
    // the secondary CObjectRefCount subobject and, at zero, dispatch its
    // destructor. Returns the surviving count (0 once destroyed).
    s32 Release();

    // @ 0x82962D98 -- (re)register the render-driver callback: unregister any
    // existing client, then, when `apCallback` is non-null, register a new one
    // (callback + mpContext) and store the returned handle.
    s32 SetCallback(void* apCallback);

    const RendererVTable* mpRendererVTable; // +0x00
    CObjectRefCount mRefCountBase;       // +0x04 (mpVTable @+0x04, mRefCount @+0x08)
    void*           mpContext;           // +0x0C
    u32             muLastCaptureBuffer; // +0x10
    u32             muCaptureRepeatCount;// +0x14
    void*           mpRenderDriverClient;// +0x18
};

} // namespace XAUDIO
