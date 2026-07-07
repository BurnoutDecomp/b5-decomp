#include "SDKs/XAudio/XAudioFrameBuffer.h"

#include <cstring> // memset

// ===========================================================================
// XAUDIO::CFrameBuffer -- member bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. The PowerPC asm is authoritative for every store;
// see XAudioFrameBuffer.h for the reconstructed layout and vtable notes. No
// reference source and no DWARF exist for this TU.
// ===========================================================================

namespace XAUDIO
{

// CFrameBuffer's own complete-object vtable image @0x821091AC (off_821091AC in
// the constructor's `*this = off_821091AC` seat). External rodata owned by the
// X360 image, not by this TU; declared extern here, this TU only seats it.
extern const CFrameBuffer::VTable FrameBufferVTable; // @0x821091AC

// The shared XAudio base-object vtable image @0x82108FF4 (off_82108FF4) that the
// scalar deleting destructor re-seats into *this -- the same base vtable the
// CObjectRefCount / CEffect base subobjects anchor. External rodata; re-seated
// only, never defined here.
extern const CFrameBuffer::VTable XAudioBaseObjectVTable; // @0x82108FF4

namespace
{

// The XAudio master mix rate (0xBB80) and 256-frame master block the buffer
// sizing formula is expressed against, plus the sample width and buffer
// alignment the ctor bakes in.
const u32 KU_MASTER_SAMPLE_RATE = 48000u; // 0xBB80
const u32 KU_MASTER_FRAME_SIZE  = 256u;   // 0x100
const u32 KU_FRAME_HEADROOM     = 32u;    // 0x20 extra samples reserved
const u32 KU_BYTES_PER_SAMPLE   = 4u;
const u32 KU_BUFFER_ALIGNMENT   = 128u;   // 0x80

// HRESULT-style status codes the asm bakes in.
const s32 KI_HR_E_OUTOFMEMORY = static_cast<s32>(0x8007000Eu); // request exceeds capacity
const s32 KI_HR_E_UNEXPECTED  = static_cast<s32>(0x8000FFFFu); // would shrink a prepared buffer

// The interleaved sample count of `auChannels` at `auSampleRate` within one
// master block: ((256 / (48000 / rate)) & 0xFFFF) * channels. Shared (inlined)
// by both the constructor's buffer sizing and Prepare's capacity check.
u32 ComputeFrameSampleCount(u32 auSampleRate, u32 auChannels)
{
    return ((KU_MASTER_FRAME_SIZE / (KU_MASTER_SAMPLE_RATE / auSampleRate)) & 0xFFFFu) * auChannels;
}

} // namespace

// @ 0x82964E38 -- constructor.
CFrameBuffer::CFrameBuffer(const CFrameBufferDesc* apDesc, IFrameBufferAllocator* apAllocator)
{
    mpVTable            = &FrameBufferVTable;            // stw off_821091AC, 0(this)
    miRefCount          = 1;                            // stw 1, 4(this)
    muUseExternalBuffer = apDesc->muUseExternalBuffer;  // lbz 0(desc) ; stb 8(this)
    muProcessingStatus  = 0;                            // stb 0, 0x14(this)
    muReserved16        = 0;                            // (reserved; +0x16 left live by the asm)
    muSampleRate        = KU_MASTER_SAMPLE_RATE;        // stw 48000, 0x18(this)
    muField20           = apDesc->muField10;            // lwz 0x10(desc) ; stw 0x20(this)

    if (muUseExternalBuffer) // lbz 8(this) ; cmplwi ; bne loc_82964F24
    {
        // Caller-supplied buffer: clear the source format and take the buffer.
        mFormat            = CFrameBufferFormat();  // stw 0, 0xC ; stw 0, 0x10
        muPreparedChannels = 0;                     // stb 0, 0x15
        mpBuffer           = apDesc->mpExternalBuffer; // lwz 0xC(desc) ; stw 0x1C
    }
    else
    {
        // Allocate a buffer sized from the descriptor's format, aligned up to
        // 128 bytes.
        const CFrameBufferFormat* lpFormat = &apDesc->mFormat; // addi r30, desc, 4
        const u32 luSamples = ComputeFrameSampleCount(lpFormat->muSampleRate, lpFormat->muChannels);
        const u32 luBytes   = (luSamples + KU_FRAME_HEADROOM) * KU_BYTES_PER_SAMPLE; // addi 0x20 ; slwi 2

        void* lpRaw = apAllocator->mpVTable->mpAllocate(apAllocator, luBytes); // bctrl
        void* lpAligned = reinterpret_cast<void*>(
            (reinterpret_cast<uintptr_t>(lpRaw) + (KU_BUFFER_ALIGNMENT - 1)) &
            ~static_cast<uintptr_t>(KU_BUFFER_ALIGNMENT - 1));                  // addi 0x7F ; clrrwi 7

        if (lpFormat) // cmplwi r30, 0 ; beq loc_82964F08 (guard on &mFormat; taken here)
        {
            mFormat            = *lpFormat; // lwz 0xC/0x10 <- desc format words
            mpBuffer           = lpAligned; // stw 0x1C
            muPreparedChannels = 0;         // stb 0, 0x15
        }
        else
        {
            mFormat            = CFrameBufferFormat(); // stw 0, 0xC ; stw 0, 0x10
            mpBuffer           = lpAligned;            // stw 0x1C
            muPreparedChannels = 0;                    // stb 0, 0x15
        }
    }
}

// @ 0x82964DD8 -- copy out the processing state. The asm emits three words
// (+0x14, +0x18, +0x1C); the first word is the status/channels/reserved triple.
s32 CFrameBuffer::GetProcessingData(CFrameBufferProcessingData* apOut)
{
    apOut->muStatus     = muProcessingStatus;  // (first emitted word: +0x14 ...)
    apOut->muChannels   = muPreparedChannels;  //                      ... +0x15 ...
    apOut->muReserved   = muReserved16;        //                      ... +0x16)
    apOut->muSampleRate = muSampleRate;        // lwz 0x18(this) ; stw 4(out)
    apOut->mpBuffer     = mpBuffer;            // lwz 0x1C(this) ; stw 8(out)
    return 0;                                  // li r3, 0
}

// @ 0x82964FA0 -- validate a prepare request against the buffer capacity.
s32 CFrameBuffer::Prepare(const CFrameBufferFormat* apFormat, u8 auFlags)
{
    const CFrameBufferFormat* lpFormat = apFormat ? apFormat : &mFormat; // if(!a2) v3 = &mFormat

    const u32 luRequested = ComputeFrameSampleCount(lpFormat->muSampleRate, lpFormat->muChannels);
    const u32 luCapacity  = ComputeFrameSampleCount(mFormat.muSampleRate, mFormat.muChannels);
    const u32 luPrepared  = ComputeFrameSampleCount(muSampleRate, muPreparedChannels);

    if (luRequested > luCapacity)             // cmplw r9, r10 ; ble -> ok
        return KI_HR_E_OUTOFMEMORY;

    if ((auFlags & 1) == 0 && luPrepared > luRequested) // (a3&1)==0 && r11 > r9
        return KI_HR_E_UNEXPECTED;

    if ((auFlags & 2) != 0) // a3 & 2 -> zero the affected sample range
    {
        u32 luStart = luPrepared;
        if ((auFlags & 1) != 0) // reset flag: clear from the start of the buffer
            luStart = 0;

        std::memset(static_cast<u8*>(mpBuffer) + luStart * KU_BYTES_PER_SAMPLE,
                    0,
                    (luRequested - luStart) * KU_BYTES_PER_SAMPLE);
    }

    muPreparedChannels = lpFormat->muChannels; // lbz 1(format) ; stb 0x15(this)
    return 0;                                  // li r3, 0
}

// @ 0x82964F48 -- intrusive ref-count release (same body as
// XAUDIO::CObjectRefCount::Release).
s32 CFrameBuffer::Release()
{
    s32 liNew = miRefCount - 1; // lwz 4(r3) ; addic. r11, r11, -1
    miRefCount = liNew;         // stw 4(r3)
    if (liNew != 0)             // bne
        return liNew;           // return surviving count

    mpVTable->mpDestruct(this); // lwz 0(r3) ; lwz 0xC(vtable) ; bctrl(this)
    return 0;                   // li r3, 0
}

// @ 0x829651B8 -- re-seat the shared base vtable image and return this.
void* CFrameBuffer::ScalarDeletingDestructor()
{
    mpVTable = &XAudioBaseObjectVTable; // stw off_82108FF4, 0(r3)
    return this;                        // mr r3, r31
}

} // namespace XAUDIO
