#include "SDKs/XAudio/XAudioXenonRenderer.h"

#include <atomic>
#include <cstring>

// ===========================================================================
// XAUDIO::CXenonRenderer -- member bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. The PowerPC asm is authoritative for every store; see
// XAudioXenonRenderer.h for the reconstructed layout, the two-vptr MI notes, and
// the compiler-synthesised thunks that are documented there rather than emitted.
// No reference source and no DWARF exist for this TU.
// ===========================================================================

namespace XAUDIO
{

// --- External rodata vtable images (owned by the X360 image, seated here) -----
// The primary render-driver-client vtable @0x82109098 and the secondary
// (CRouterEffect) vtable @0x8210907C the ctor installs, plus the CObjectRefCount
// base image @0x82108FF4 the base dtor re-seats. They are supplied by the
// platform/link layer; this TU only stores their addresses.
extern const CXenonRenderer::RendererVTable XenonRendererVTable;       // @0x82109098
extern const CObjectRefCount::VTable        XenonRendererRouterVTable; // @0x8210907C
extern const CObjectRefCount::VTable        ObjectRefCountVTable;      // @0x82108FF4

// --- Un-homed XAudio render-driver platform API (declared for the compile gate).
// These X360 XAudio driver entry points live in their own (un-homed) TUs; only
// their prototypes -- inferred from the call sites in this TU -- are needed here.
extern s32 XAudioFrameBuffer_GetProcessingData(void* apFrameBuffer, CFrameBufferProcessingData* apOut);
extern s32 XAudioSubmitRenderDriverFrame(void* apClient, void* apBuffer);
extern s32 XAudioRegisterRenderDriverClient(SRenderDriverRegistration* apRegistration, void** appClientOut);
extern s32 XAudioUnregisterRenderDriverClient(void* apClient);

// --- XBOX-Media (XBM) audio-capture ring globals (file-static to this TU) ------
// dword_82F37684: the capture ring's write cursor. Advanced atomically by one
//   frame (6144 bytes) per capture; the sentinel 6144 latches it after a single
//   frame. The X360 uses an interrupt-disabled lwarx/stwcx RMW; std::atomic is
//   the faithful portable rendering.
// dword_832BE604: the reset marker Initialize clears (0) and the dtor latches
//   (6144); read as the fallback "last buffer" when the ring yields slot 0.
static std::atomic<u32> guXBMCaptureCursor{0}; // dword_82F37684
static u32              guXBMResetMarker = 0;   // dword_832BE604

static const u32 KU_XBM_FRAME_BYTES = 6144; // 0x1800

// @ 0x829630B0
CXenonRenderer::CXenonRenderer(const SXenonRendererCreateParams* apParams)
{
    // The secondary base ctor first stages off_82109008 into the +0x04 vptr; the
    // derived ctor immediately overwrites it with off_8210907C (below), so only
    // the final value is modelled.
    mRefCountBase.mRefCount = 1;                        // stw r7(1), 8(r3)
    mpContext = apParams->mpContext;                    // lwz 4(r4) ; stw 0xC(r3)
    mpRendererVTable = &XenonRendererVTable;            // stw off_82109098, 0(r3)
    mRefCountBase.mpVTable = &XenonRendererRouterVTable;// stw off_8210907C, 4(r3)
    muLastCaptureBuffer = 0;                            // stw r8(0), 0x10(r3)
    muCaptureRepeatCount = 0;                           // stw r8(0), 0x14(r3)
    // mpRenderDriverClient (+0x18) is intentionally left as-is: the X360 ctor
    // does not write it (SetCallback seats it first).
}

// @ 0x82962EC0
CXenonRenderer::~CXenonRenderer()
{
    // Re-seat this class's own vtables on dtor entry (standard C++ dtor vptr
    // management), then latch the XBM capture ring and tear the client down.
    mpRendererVTable = &XenonRendererVTable;             // stw off_82109098, 0(r31)
    mRefCountBase.mpVTable = &XenonRendererRouterVTable; // stw off_8210907C, 4(r31)

    guXBMResetMarker = KU_XBM_FRAME_BYTES;               // stw 0x1800, dword_832BE604

    // Atomically latch the ring: if the cursor is still 0, set it to 6144;
    // otherwise leave it (the stwcx r8 path stores the old value back).
    u32 uExpected = 0;
    guXBMCaptureCursor.compare_exchange_strong(uExpected, KU_XBM_FRAME_BYTES);

    s32 iResult = 0;
    if (mpRenderDriverClient)                            // lwz 0x18(r31) ; cmplwi
    {
        iResult = XAudioUnregisterRenderDriverClient(mpRenderDriverClient);
        if (iResult >= 0)                               // cmpwi ; blt
            mpRenderDriverClient = nullptr;             // stw r30(0), 0x18(r31)
    }

    // Final base-dtor re-seat of the secondary vptr to the CObjectRefCount image.
    mRefCountBase.mpVTable = &ObjectRefCountVTable;      // stw off_82108FF4, 4(r31)
}

// @ 0x82962E00
void* CXenonRenderer::CaptureXBMAudioFrame(CFrameBufferProcessingData* apSource)
{
    void* pResult = this; // r3 is left = this when the ring is latched

    if (guXBMCaptureCursor.load(std::memory_order_relaxed) != KU_XBM_FRAME_BYTES)
    {
        // Atomically claim the current slot and advance the cursor by one frame
        // (lwarx/stwcx fetch-add under an interrupt lock on the X360).
        u32 uSlot = guXBMCaptureCursor.fetch_add(KU_XBM_FRAME_BYTES, std::memory_order_acq_rel);
        pResult = reinterpret_cast<void*>(static_cast<uintptr_t>(uSlot));

        if (uSlot != 0)
        {
            // Count consecutive captures that land exactly one frame past the
            // previous slot; any gap restarts the run at 1.
            if (uSlot - muLastCaptureBuffer == KU_XBM_FRAME_BYTES)
                muCaptureRepeatCount = muCaptureRepeatCount + 1;
            else
                muCaptureRepeatCount = 1;
            muLastCaptureBuffer = uSlot;

            if (muCaptureRepeatCount <= 0xE)
            {
                void* pSlot = reinterpret_cast<void*>(static_cast<uintptr_t>(uSlot));
                std::memcpy(pSlot, apSource->mpBuffer, KU_XBM_FRAME_BYTES);
                pResult = pSlot;
                std::atomic_thread_fence(std::memory_order_release); // lwsync
            }
        }
        else
        {
            muLastCaptureBuffer = guXBMResetMarker; // stw dword_832BE604, 0x10(r11)
        }
    }

    return pResult;
}

// @ 0x82962FC8
s32 CXenonRenderer::GetInfo(SXenonRendererInfo* apInfo)
{
    apInfo->muChannels = 2;   // stb r11(2), 0(r4)
    apInfo->muReserved2 = 0;  // sth r10(0), 2(r4)
    return 0;                 // li r3, 0
}

// @ 0x82962D78
s32 CXenonRenderer::Initialize(const SXenonRendererInitParams* apParams)
{
    guXBMResetMarker = 0; // stw r11(0), dword_832BE604
    // Tail-dispatch the primary vtable's Prepare slot (@+0x1C) with the config
    // word from the params (+0x08).
    return mpRendererVTable->mpPrepare(this, apParams->mpConfig);
}

// @ 0x82963000
s32 CXenonRenderer::Process(void* apFrameBuffer)
{
    CFrameBufferProcessingData lProcessingData;

    s32 iResult = XAudioFrameBuffer_GetProcessingData(apFrameBuffer, &lProcessingData);
    if (iResult >= 0) // cmpwi ; blt
    {
        // Render slot @+0x20 (result ignored), then submit the produced buffer.
        mpRendererVTable->mpRender(this, &lProcessingData);
        return XAudioSubmitRenderDriverFrame(mpRenderDriverClient, lProcessingData.mpBuffer);
    }
    return iResult;
}

// @ 0x8296B1E8
void* CXenonRenderer::QueryInterface(void** appInterface)
{
    *appInterface = this; // stw r3, 0(r4)
    return this;          // r3 unchanged
}

// @ 0x82962F80
s32 CXenonRenderer::Release()
{
    // The primary interface's Release is the ref-count body operating on the
    // secondary CObjectRefCount subobject at +0x04: decrement, and at zero
    // dispatch that subobject's destructor slot (off_8210907C[3]). Identical to
    // CObjectRefCount::Release, so it delegates there.
    return mRefCountBase.Release();
}

// @ 0x82962D98
s32 CXenonRenderer::SetCallback(void* apCallback)
{
    s32 iResult = 0;

    if (mpRenderDriverClient) // lwz 0x18(r31) ; cmplwi
    {
        iResult = XAudioUnregisterRenderDriverClient(mpRenderDriverClient);
        if (iResult < 0) // cmpwi ; blt -> LABEL_6
            return iResult;
        mpRenderDriverClient = nullptr; // stw r11(0), 0(r30)
    }

    if (apCallback) // cmplwi r29,0 ; beq
    {
        SRenderDriverRegistration lRegistration;
        lRegistration.mpCallback = apCallback;  // stw r29, var_30
        lRegistration.mpContext = mpContext;    // lwz 0xC(r31) ; stw var_2C
        iResult = XAudioRegisterRenderDriverClient(&lRegistration, &mpRenderDriverClient);
    }

    return iResult;
}

} // namespace XAUDIO
